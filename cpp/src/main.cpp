#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <thread>
#include <type_traits>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <array>
#include <libdeflate.h>
#include <spdlog/spdlog.h>

#include "Cursor.h"
#include "MappedFile.h"
#include "WorkQueue.h"
#include "BlfTypes.h"

#include <signal.h>


// ratio used to allocate buffer for decompressed data. if insufficient, a reallocation would be triggered
// based on real traces, compressed objects seem to have a compression ratio of 4.5.
constexpr size_t DECOMP_BUFFER_PREALLOC_RATIO = 6;
constexpr uint32_t BLF_LOGG_SIGNATURE = 0x47474f4c;  // "LOGG"
constexpr uint32_t BLF_LOBJ_SIGNATURE = 0x4a424f4c;  // "LOBJ"

bool dumpObjContents = false;

// ---------------------------------------------------------------------------
// processInnerObjects – walk decompressed container payload, decode CAN and
// Ethernet log objects, and accumulate per-call counters.
//
// Design notes:
//   * Operates entirely on the caller-supplied scratch buffer which is already
//     hot in L3 cache after libdeflate writes it.  Zero extra copies.
//   * Returns counts via out-params so the caller can merge them into a single
//     atomic update – minimising atomic traffic between threads.
// ---------------------------------------------------------------------------
// Forward declarations – both are defined later in this file.
static bool findNextLobj(Cursor& cursor, uint32_t& sigOut);
static std::string_view objectTypeName(uint32_t t);

static void processInnerObjects(const char* data, size_t dataLen,
                                std::unordered_map<uint32_t, uint64_t>& counts,
                                uint64_t& splitCount) {
  Cursor cur{data, data + dataLen, data};

  BlfObjectHeaderBase base;
  while (!cur.eof()) {
    if (!findNextLobj(cur, base.signature)) break;
    if (!cur.read(reinterpret_cast<char*>(&base) + 4,
                  sizeof(BlfObjectHeaderBase) - 4)) break;

    // -----------------------------------------------------------------------
    // Cross-container split detection.
    //
    // base.objectSize is the total byte length of this LOBJ (header + payload)
    // measured from the start of the "LOBJ" signature.  The object started
    // sizeof(BlfObjectHeaderBase) bytes before the current cursor position.
    //
    //   objectEnd = objectStart + base.objectSize
    //             = (cur.tell() - sizeof(BlfObjectHeaderBase)) + base.objectSize
    //
    // If objectEnd > dataLen the encoder placed the tail of this object into
    // the next compressed container – exactly the split case we want to detect.
    // -----------------------------------------------------------------------
    const size_t objectStart = cur.tell() - sizeof(BlfObjectHeaderBase);
    const size_t objectEnd   = objectStart + base.objectSize;
    if (objectEnd > dataLen) {
      const size_t bytesAvail    = dataLen - objectStart;
      const size_t bytesOverflow = objectEnd - dataLen;
      spdlog::debug(
          "SPLIT: type={} ({}) objectSize={} B  |  {} B in this container, {} B in next",
          base.objectType, objectTypeName(base.objectType),
          base.objectSize, bytesAvail, bytesOverflow);
      ++splitCount;
      break;  // remainder of this object lives in the next container; stop here
    }

    // The object's payload starts right after the base header.
    // headerSize includes the 4-byte signature but NOT the BlfObjectHeader
    // extension; it equals sizeof(BlfObjectHeaderBase) + sizeof(BlfObjectHeader)
    // for type-1 headers, or may differ for type-2/3.  We read exactly
    // (headerSize - sizeof(BlfObjectHeaderBase)) bytes of extra header,
    // then the remaining payload is (objectSize - headerSize) bytes.
    const size_t extraHdrBytes = base.headerSize > sizeof(BlfObjectHeaderBase)
        ? base.headerSize - sizeof(BlfObjectHeaderBase)
        : 0;
    if (extraHdrBytes > 0) {
      // Peek past the extended header without copying
      if (!cur.skip(extraHdrBytes)) break;
    }

    const size_t payloadBytes = base.objectSize > base.headerSize
        ? base.objectSize - base.headerSize
        : 0;

    switch (static_cast<BLFObjectType>(base.objectType)) {

      case CAN_MESSAGE:
      case CAN_MESSAGE2: {
        if (payloadBytes < sizeof(CANMessage)) {
          spdlog::debug("inner: CAN object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        CANMessage msg;
        cur.read(&msg, sizeof(CANMessage));
        const size_t surplus = payloadBytes - sizeof(CANMessage);
        if (surplus > 0) cur.skip(surplus);

        ++counts[base.objectType];
        if (dumpObjContents) {
          const uint32_t rawId = msg.id;
          const bool     ext   = (rawId >> 31) & 1;
          const uint32_t arbId = ext ? (rawId & 0x1FFFFFFFu) : (rawId & 0x7FFu);
          spdlog::debug("CAN ch={} id=0x{:x}{} dlc={} flags=0x{:02x}",
                        msg.channel, arbId, ext ? "x" : "", msg.dlc, msg.flags);
        }
        break;
      }

      case ETHERNET_FRAME: {
        if (payloadBytes < sizeof(EthernetFrameHeader)) {
          spdlog::debug("inner: ETH object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        EthernetFrameHeader eth;
        cur.read(&eth, sizeof(EthernetFrameHeader));
        const size_t surplus = payloadBytes - sizeof(EthernetFrameHeader);
        if (surplus > 0) cur.skip(surplus);

        ++counts[base.objectType];
        if (dumpObjContents) {
          spdlog::debug("ETH ch={} dir={} type=0x{:04x} paylen={}",
                        eth.channel, eth.direction, eth.ethType, eth.payloadLength);
        }
        break;
      }

      case CONTAINER:
        cur.skip(payloadBytes);
        break;

      default:
        if (payloadBytes > 0) cur.skip(payloadBytes);
        ++counts[base.objectType];
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// Per-run timing accumulators
// ---------------------------------------------------------------------------
struct PerfCounters {
  uint64_t containers        = 0;
  uint64_t compressedBytes   = 0;
  uint64_t decompressedBytes = 0;
  uint64_t splitObjects      = 0;  // LOBJs whose tail extends past their container's end
  int64_t  pipelineUs        = 0;  // wall-clock: producer + consumers overlapped
  size_t   nThreads          = 0;
  // per-type object counts, merged from all consumer threads
  std::unordered_map<uint32_t, uint64_t> objectCounts;
  std::mutex objectCountsMu;  // guards objectCounts during thread merges
};
static PerfCounters g_perf;


std::string to_hex(const char* buf, size_t len) {
  std::string result;
  result.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    result += std::format("{:02x}", static_cast<unsigned char>(buf[i]));
  }
  return result;
}


std::string to_hex(uint32_t num) {
  return to_hex(reinterpret_cast<const char*>(&num), 4);
}


// ---------------------------------------------------------------------------
// objectTypeName – human-readable label for a BLF object type ID.
// Returns "UNKNOWN_<n>" for any type not in the table.
// ---------------------------------------------------------------------------
static std::string_view objectTypeName(uint32_t t) {
  static const auto names = [] {
    std::array<std::string_view, 256> arr{};
    arr[CAN_MESSAGE] = "CAN_MESSAGE";
    arr[CAN_ERROR] = "CAN_ERROR";
    arr[CAN_OVERLOAD] = "CAN_OVERLOAD";
    arr[CAN_STATISTIC] = "CAN_STATISTIC";
    arr[APP_TEXT] = "APP_TEXT";
    arr[CAN_REMOTE_FRAME] = "CAN_REMOTE_FRAME";
    arr[LIN_MESSAGE] = "LIN_MESSAGE";
    arr[CONTAINER] = "CONTAINER";
    arr[LIN_RX_ERROR] = "LIN_RX_ERROR";
    arr[LIN_SEND_ERROR] = "LIN_SEND_ERROR";
    arr[LIN_SLAVE_TIMEOUT] = "LIN_SLAVE_TIMEOUT";
    arr[LIN_NOANS] = "LIN_NOANS";
    arr[LIN_WAKEUP] = "LIN_WAKEUP";
    arr[LIN_SPIKE] = "LIN_SPIKE";
    arr[LIN_DLCINFO] = "LIN_DLCINFO";
    arr[LIN_RCV_ERROR] = "LIN_RCV_ERROR";
    arr[LIN_SYNCERROR] = "LIN_SYNCERROR";
    arr[LIN_BAUDRATE] = "LIN_BAUDRATE";
    arr[LIN_SLEEP] = "LIN_SLEEP";
    arr[LIN_WAKEUP2] = "LIN_WAKEUP2";
    arr[MOST_SPY] = "MOST_SPY";
    arr[MOST_CTRL] = "MOST_CTRL";
    arr[MOST_LIGHTLOCK] = "MOST_LIGHTLOCK";
    arr[MOST_STATISTIC] = "MOST_STATISTIC";
    arr[FLEXRAY_DATA] = "FLEXRAY_DATA";
    arr[FLEXRAY_SYNC] = "FLEXRAY_SYNC";
    arr[CAN_DRIVER_ERROR] = "CAN_DRIVER_ERROR";
    arr[MOST_PKT] = "MOST_PKT";
    arr[MOST_PKT2] = "MOST_PKT2";
    arr[MOST_HWMODE] = "MOST_HWMODE";
    arr[MOST_REG] = "MOST_REG";
    arr[MOST_GENREG] = "MOST_GENREG";
    arr[MOST_NETSTATE] = "MOST_NETSTATE";
    arr[MOST_DATALOST] = "MOST_DATALOST";
    arr[MOST_TRIGGER] = "MOST_TRIGGER";
    arr[FLEXRAY_CYCLE] = "FLEXRAY_CYCLE";
    arr[FLEXRAY_MESSAGE] = "FLEXRAY_MESSAGE";
    arr[LIN_CHECKSUM_INFO] = "LIN_CHECKSUM_INFO";
    arr[LIN_SPIKE_IGNORE] = "LIN_SPIKE_IGNORE";
    arr[LIN_WAKEUP_INFO] = "LIN_WAKEUP_INFO";
    arr[LIN_IN_PROGRESS] = "LIN_IN_PROGRESS";
    arr[LIN_UNEXPECTED_WAKEUP] = "LIN_UNEXPECTED_WAKEUP";
    arr[LIN_SHORT_OR_SLOW_RESPONSE] = "LIN_SHORT_OR_SLOW_RESPONSE";
    arr[LIN_DISTURBANCE_EVENT] = "LIN_DISTURBANCE_EVENT";
    arr[SERIAL_EVENT] = "SERIAL_EVENT";
    arr[OVERRUN_ERROR] = "OVERRUN_ERROR";
    arr[EVENT_COMMENT] = "EVENT_COMMENT";
    arr[WLAN_FRAME] = "WLAN_FRAME";
    arr[WLAN_STATISTIC] = "WLAN_STATISTIC";
    arr[MOST_ECL] = "MOST_ECL";
    arr[SYS_VARIABLE] = "SYS_VARIABLE";
    arr[CAN_ERROR_EXT] = "CAN_ERROR_EXT";
    arr[CAN_DRIVER_ERROR_EXT] = "CAN_DRIVER_ERROR_EXT";
    arr[LIN_LONG_DOM_SIG] = "LIN_LONG_DOM_SIG";
    arr[MOST_150_MESSAGE] = "MOST_150_MESSAGE";
    arr[MOST_150_PKT] = "MOST_150_PKT";
    arr[MOST_ETHERNET_PKT] = "MOST_ETHERNET_PKT";
    arr[MOST_150_MESSAGE_FRAGMENT] = "MOST_150_MESSAGE_FRAGMENT";
    arr[MOST_150_PKT_FRAGMENT] = "MOST_150_PKT_FRAGMENT";
    arr[MOST_ETHERNET_PKT_FRAGMENT] = "MOST_ETHERNET_PKT_FRAGMENT";
    arr[MOST_SYSTEM_EVENT] = "MOST_SYSTEM_EVENT";
    arr[MOST_150_ALLOCTAB] = "MOST_150_ALLOCTAB";
    arr[MOST_50_MESSAGE] = "MOST_50_MESSAGE";
    arr[MOST_50_PKT] = "MOST_50_PKT";
    arr[CAN_MESSAGE2] = "CAN_MESSAGE2";
    arr[LIN_UNEXPECTED_WAKEUP2] = "LIN_UNEXPECTED_WAKEUP2";
    arr[LIN_SHORT_OR_SLOW_RESPONSE3] = "LIN_SHORT_OR_SLOW_RESPONSE3";
    arr[LIN_DISTURBANCE_EVENT2] = "LIN_DISTURBANCE_EVENT2";
    arr[APP_TRIGGER] = "APP_TRIGGER";
    arr[ENV_INTEGER] = "ENV_INTEGER";
    arr[ENV_DOUBLE] = "ENV_DOUBLE";
    arr[ENV_STRING] = "ENV_STRING";
    arr[ENV_DATA] = "ENV_DATA";
    arr[GRAPHICS_OBJECT] = "GRAPHICS_OBJECT";
    arr[GLOBAL_MARKER] = "GLOBAL_MARKER";
    arr[AFDX_FRAME] = "AFDX_FRAME";
    arr[AFDX_STATISTIC] = "AFDX_STATISTIC";
    arr[KLINE_STATUSEVENT] = "KLINE_STATUSEVENT";
    arr[CAN_FD_MESSAGE] = "CAN_FD_MESSAGE";
    arr[CAN_FD_MESSAGE_64] = "CAN_FD_MESSAGE_64";
    arr[ETHERNET_RX_ERROR] = "ETHERNET_RX_ERROR";
    arr[ETHERNET_STATUS] = "ETHERNET_STATUS";
    arr[CAN_FD_ERROR_64] = "CAN_FD_ERROR_64";
    arr[LIN_SHORT_OR_SLOW_RESPONSE2] = "LIN_SHORT_OR_SLOW_RESPONSE2";
    arr[AFDX_STATUS] = "AFDX_STATUS";
    arr[AFDX_BUS_STATISTIC] = "AFDX_BUS_STATISTIC";
    arr[AFDX_ERROR_EVENT] = "AFDX_ERROR_EVENT";
    arr[A429_ERROR] = "A429_ERROR";
    arr[A429_STATUS] = "A429_STATUS";
    arr[A429_BUS_STATISTIC] = "A429_BUS_STATISTIC";
    arr[A429_MESSAGE] = "A429_MESSAGE";
    arr[ETHERNET_STATISTIC] = "ETHERNET_STATISTIC";
    arr[RESERVED_1] = "RESERVED_1";
    arr[RESERVED_2] = "RESERVED_2";
    arr[RESERVED_3] = "RESERVED_3";
    arr[TEST_STRUCTURE] = "TEST_STRUCTURE";
    arr[DIAG_REQUEST_INTERPRETATION] = "DIAG_REQUEST_INTERPRETATION";
    arr[ETHERNET_FRAME] = "ETHERNET_FRAME";
    arr[ETHERNET_FRAME_EX] = "ETHERNET_FRAME_EX";
    arr[ETHERNET_ERROR_EX] = "ETHERNET_ERROR_EX";
    arr[ETHERNET_ERROR_FORWARDED] = "ETHERNET_ERROR_FORWARDED";
    arr[FUNC_BUS] = "FUNC_BUS";
    arr[DATA_LOST_BEGIN] = "DATA_LOST_BEGIN";
    arr[DATA_LOST_END] = "DATA_LOST_END";
    arr[WATER_MARK_EVENT] = "WATER_MARK_EVENT";
    arr[TRIGGER_CONDITION] = "TRIGGER_CONDITION";
    arr[CAN_SETTING_CHANGED] = "CAN_SETTING_CHANGED";
    arr[DISTRIBUTED_OBJECT] = "DISTRIBUTED_OBJECT";
    return arr;
  }();
  if (t < names.size()) return names[t];
  return {};
}


// ---------------------------------------------------------------------------
// findNextLobj – scan cursor forward until the "LOBJ" signature is found,
// handling up to 3 bytes of misalignment (e.g. inter-object padding).
// On success: sigOut == BLF_LOBJ_SIGNATURE, cursor is past the 4-byte sig.
// ---------------------------------------------------------------------------
static bool findNextLobj(Cursor& cursor, uint32_t& sigOut) {
  while (cursor.remaining() >= 4) {
    // casting pointer to uint32 breaks c++ strict aliasing rules and is technically UB.
    // this is still ok on x86-64. on ARM, fallback to memcpy
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_IX86)
    sigOut = *reinterpret_cast<const uint32_t*>(cursor.pos);
#else
    std::memcpy(&sigOut, cursor.pos, 4);
#endif
    if (BLF_LOBJ_SIGNATURE == sigOut) {
      cursor.pos += 4;
      return true;
    }
    cursor.pos += 1;
  }
  return false;
}


// ---------------------------------------------------------------------------
// readFileHeader – validate the LOGG header and advance the cursor past it.
// ---------------------------------------------------------------------------
std::optional<BlfFileHeader> readFileHeader(Cursor& cursor) {
  const size_t fileSize = static_cast<size_t>(cursor.end - cursor.base);
  if (fileSize < sizeof(BlfFileHeader)) {
    spdlog::error("File is too short ({} bytes). stop", fileSize); return std::nullopt;
  }

  BlfFileHeader hdr;
  if (!cursor.read(&hdr, sizeof(BlfFileHeader))) {
    spdlog::error("Failed to read file header"); return std::nullopt;
  }
  if (std::string_view(hdr.signature, sizeof(hdr.signature)) != "LOGG") {
    spdlog::error("File does not contain valid LOGG header"); return std::nullopt;
  }

  spdlog::info("Found valid \"LOGG\" header");
  spdlog::debug("Header size:       {} (0x{:x})", hdr.headerSize, hdr.headerSize);
  spdlog::debug("apiVersion:        {} (0x{:x})", hdr.apiVersion, hdr.apiVersion);
  spdlog::debug("compressionLevel:  {}", hdr.compressionLevel);
  spdlog::debug("version:           {}.{}", hdr.majorVersion, hdr.minorVersion);
  spdlog::debug("compressedSize:    {:.2f} MiB", hdr.compressedSize / 1048576.0);
  spdlog::debug("uncompressedSize:  {:.2f} MiB", hdr.uncompressedSize / 1048576.0);
  spdlog::debug("numObj:            {}", hdr.numObj);

  const size_t padSize = hdr.headerSize - sizeof(BlfFileHeader);
  if (padSize > 0) {
    const char* pad = cursor.peek(padSize);
    if (pad) spdlog::debug("File header pad: {}", to_hex(pad, padSize));
  }
  return hdr;
}


// ---------------------------------------------------------------------------
// runConsumer – consumer thread entry point.
// Decompresses BLF container payloads popped from the work queue.
// ---------------------------------------------------------------------------
static void runConsumer(WorkQueue& queue,
                        std::atomic<uint64_t>& atomicContainers,
                        std::atomic<uint64_t>& atomicCompressed,
                        std::atomic<uint64_t>& atomicDecompressed,
                        std::mutex& countsMu,
                        std::unordered_map<uint32_t, uint64_t>& sharedCounts,
                        uint64_t& sharedSplits,
                        bool skipDecompress) {
  auto decomp = std::unique_ptr<libdeflate_decompressor, decltype(&libdeflate_free_decompressor)>(
      libdeflate_alloc_decompressor(), libdeflate_free_decompressor);
  if (!decomp) {
    spdlog::error("Failed to allocate libdeflate decompressor");
    return;
  }
  std::vector<char> localBuf;  // per-thread scratch; never freed until exit
  std::unordered_map<uint32_t, uint64_t> localCounts;  // accumulate without contention
  uint64_t localSplitCount = 0;

  // queue::pop will only return with an item (log object) or if the queue was closed.
  // We do not risk prematurely terminating consumers due to starvation
  while (auto item = queue.pop()) {
    atomicContainers.fetch_add(1, std::memory_order_relaxed);
    atomicCompressed.fetch_add(item->compSize, std::memory_order_relaxed);

    if (skipDecompress) continue;

    // Grow the local buffer as needed; the buffer stays at the high-water mark
    // for the rest of this thread's lifetime.
    const size_t needed = item->compSize * DECOMP_BUFFER_PREALLOC_RATIO;
    if (localBuf.size() < needed) localBuf.resize(needed);

    // decompress the log object. expand decompression buffer if it is insufficient.
    size_t actualOut = 0;
    libdeflate_result res;
    do {
      res = libdeflate_zlib_decompress(
          decomp.get(),
          item->compData,  item->compSize,
          localBuf.data(), localBuf.size(),
          &actualOut);
      if (LIBDEFLATE_INSUFFICIENT_SPACE == res) {
        localBuf.resize(localBuf.size() * 2);
        spdlog::debug("localBuf grown to {} B", localBuf.size());
      }
    } while (LIBDEFLATE_INSUFFICIENT_SPACE == res);

    if (LIBDEFLATE_SUCCESS == res) {
      atomicDecompressed.fetch_add(actualOut, std::memory_order_relaxed);

      // Parse inner LOBJs while the decompressed data is hot in L3 cache.
      uint64_t localSplits = 0;
      processInnerObjects(localBuf.data(), actualOut, localCounts, localSplits);
      if (localSplits) localSplitCount += localSplits;
    } else {
      spdlog::error("libdeflate failed ({})", static_cast<int>(res));
    }
  }

  // Thread is done: merge local counts into the shared map under a mutex.
  // This happens exactly once per thread, so contention is negligible.
  if (!localCounts.empty() || localSplitCount > 0) {
    std::lock_guard<std::mutex> lk(countsMu);
    for (const auto& [type, cnt] : localCounts)
      sharedCounts[type] += cnt;
    sharedSplits += localSplitCount;
  }
}


// ---------------------------------------------------------------------------
// runProducer – scan mmap'd file sequentially, push payload to work queue.
// ---------------------------------------------------------------------------
static void runProducer(Cursor cursor, WorkQueue& queue) {
  using Clock = std::chrono::steady_clock;
  auto prodStart = Clock::now();

  BlfObjectHeaderBase base;
  while (!cursor.eof()) {
    // seek to start of next object and read it
    if (!findNextLobj(cursor, base.signature)) break;
    if (!cursor.read(reinterpret_cast<char*>(&base) + 4,
                     sizeof(BlfObjectHeaderBase) - 4)) break;

    spdlog::debug("pipeline: type={} objectSize={}", base.objectType, base.objectSize);

    if (CONTAINER == base.objectType) {
      BlfObjectHeader extHdr;
      if (!cursor.read(reinterpret_cast<char*>(&extHdr),
                       sizeof(BlfObjectHeaderBase))) break;

      const size_t compSize =
          base.objectSize - base.headerSize - sizeof(BlfObjectHeader);
      const char* compData = cursor.peek(compSize);
      if (!compData) break;

      queue.push({compData, compSize});  // blocks if consumers are behind
    } else {
      // Skip non-CONTAINER object payload
      const size_t remaining = base.objectSize - sizeof(BlfObjectHeaderBase);
      if (!cursor.skip(remaining)) break;
    }
  }

  auto prodEnd = Clock::now();
  const double prodMs = std::chrono::duration<double, std::milli>(prodEnd - prodStart).count();
  spdlog::info("Producer done in {:.2f} ms; waiting for consumers...", prodMs);
}


// ---------------------------------------------------------------------------
// runPipeline – single-pass producer-consumer pipeline.
//
// Why this is faster than the previous two-phase approach:
//
//   Old approach (two phases, sequential):
//     Phase 1  – producer reads all 300 MB cold (mmap page faults): 168 ms
//     Phase 1b – pre-allocates 1.4 GB of output buffers: 22 ms
//     Phase 2  – workers read 300 MB (warm) + write 1444 MB (cold): 188 ms
//     Total DRAM traffic: 300 + 300 + 1444 = 2044 MB → ~5.7 GB/s → 580 MB/s
//
//   New approach (pipelined):
//     Producer scans the mmap'd file sequentially, pushing {ptr, size} items
//     to the work queue.  Consumers pop items and decompress into a small
//     per-thread buffer. The decompression buffer is reused for  all containers
//     on that thread.
//     The reused buffer (~130 KB) should stay in L3 cache; decompressed bytes
//     are (mostly) never written to DRAM. Try to process the decompressed
//     containers before they are evicted from cache.
//
//     Total DRAM traffic: ~300 MB (one cold read pass, overlapped with compute)
//
//     Additionally:
//     - MADV_WILLNEED kicks off async page loading before the producer starts,
//       so the producer finds pages warm in the kernel page cache.
//     - Producers and consumers run concurrently; I/O and compute overlap.
// ---------------------------------------------------------------------------
static void runPipeline(Cursor cursor, const BlfFileHeader& hdr,
                        size_t nWorkers, bool skipDecompress) {
  using Clock = std::chrono::steady_clock;

  // Bound the queue to 4× workers: producer stays ahead without excessive
  // getting blocked by memory. Each in-flight item is just a pointer + size (16 B).
  WorkQueue queue(nWorkers * 4);

  // Shared perf counters written atomically by workers
  std::atomic<uint64_t> atomicContainers{0};
  std::atomic<uint64_t> atomicCompressed{0};
  std::atomic<uint64_t> atomicDecompressed{0};
  // Per-type object counts and split detections: accumulated locally per thread,
  // merged once at thread exit under a mutex.
  std::mutex countsMu;
  std::unordered_map<uint32_t, uint64_t> sharedCounts;
  uint64_t sharedSplits = 0;

  // ---- Start consumers ----
  std::vector<std::thread> workers;
  workers.reserve(nWorkers);
  for (size_t i = 0; i < nWorkers; ++i) {
    workers.emplace_back(runConsumer, std::ref(queue), std::ref(atomicContainers),
                         std::ref(atomicCompressed), std::ref(atomicDecompressed),
                         std::ref(countsMu), std::ref(sharedCounts),
                         std::ref(sharedSplits),
                         skipDecompress);
  }

  // ---- Producer (runs on calling thread, concurrent with consumers) ----
  // MADV_WILLNEED (set in MappedFile::open) has been pre-loading pages since
  // mmap returned, so many of these touches will hit the page cache.
  runProducer(cursor, queue);

  queue.close();  // signal: no more items will be pushed
  for (auto& w : workers) w.join();

  g_perf.containers        = atomicContainers.load();
  g_perf.compressedBytes   = atomicCompressed.load();
  g_perf.decompressedBytes = atomicDecompressed.load();
  g_perf.objectCounts      = std::move(sharedCounts);
  g_perf.splitObjects      = sharedSplits;
  g_perf.nThreads          = nWorkers;
}


// ---------------------------------------------------------------------------
// processFile – top-level orchestrator
// ---------------------------------------------------------------------------
void processFile(const std::string& filename, bool skipDecompress) {
  using Clock = std::chrono::steady_clock;

  MappedFile mf;
  if (!mf.open(filename)) return;
  spdlog::info("Processing file: {} ({:.2f} MiB)", filename, mf.size / (1024.0 * 1024.0));

  Cursor cursor = mf.cursor();
  auto fileHeader = readFileHeader(cursor);
  if (!fileHeader) return;

  const size_t nWorkers = std::max<size_t>(1, std::thread::hardware_concurrency());
  spdlog::info("Starting pipeline: {} worker thread(s)", nWorkers);

  auto t0 = Clock::now();
  runPipeline(cursor, *fileHeader, nWorkers, skipDecompress);
  g_perf.pipelineUs = std::chrono::duration_cast<std::chrono::microseconds>(
      Clock::now() - t0).count();
}


void benchmarkRawRead(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    spdlog::error("Could not open file {} for raw read benchmark", filename);
    return;
  }

  const size_t fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  constexpr size_t kBufSize = 1024 * 1024;  // 1 MiB chunks
  std::vector<char> buf(kBufSize);

  auto start = std::chrono::high_resolution_clock::now();
  while (file.read(buf.data(), kBufSize) || file.gcount() > 0) {
    if (0 == file.gcount()) break;
  }
  auto end = std::chrono::high_resolution_clock::now();

  const double seconds = std::chrono::duration<double>(end - start).count();
  const double megabytes = static_cast<double>(fileSize) / (1024.0 * 1024.0);

  spdlog::info("--- Raw read benchmark ---");
  spdlog::info("File size : {:.2f} MiB", megabytes);
  spdlog::info("Read time : {:.3f} ms", seconds * 1000.0);
  spdlog::info("Throughput: {:.2f} MiB/s", megabytes / seconds);
}


// Evict a single file from the Linux page cache using posix_fadvise.
void dropFileCache(const std::string& filename) {
#ifdef _WIN32
  // Windows doesn't have a direct equivalent to posix_fadvise(DONTNEED) for a single file cache drop
  // without administrative privileges (using system-wide cache drop).
#else
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) { spdlog::warn("could not open file for cache drop"); return; }

  fdatasync(fd);
  off_t len = lseek(fd, 0, SEEK_END);
  if (len > 0) {
    int ret = posix_fadvise(fd, 0, len, POSIX_FADV_DONTNEED);
    if (ret != 0) spdlog::warn("posix_fadvise failed ({})", ret);
    else          spdlog::debug("Page cache dropped for file");
  }
  close(fd);
#endif
}


int main(int argc, char* argv[]) {
  if (argc < 2) {
    spdlog::error("Usage: {} <filename> [--no-decompress] [--benchmark] [--dump-objects] [--debug]", argv[0]);
    return 1;
  }

  spdlog::set_level(spdlog::level::info);

  std::string filename = argv[1];
  bool skipDecompress = false;
  bool runBenchmark = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if ("--no-decompress" == arg)   skipDecompress = true;
    else if ("--benchmark" == arg)  runBenchmark   = true;
    else if ("--dump-objects" == arg) dumpObjContents = true;
    else if ("--debug" == arg)      spdlog::set_level(spdlog::level::debug);
  }

  if (skipDecompress) {
    spdlog::info("*** --no-decompress: consumers will skip inflate() ***");
  }

  std::ifstream sizeProbe(filename, std::ios::binary | std::ios::ate);
  const size_t sizeBytes = sizeProbe.is_open() ? static_cast<size_t>(sizeProbe.tellg()) : 0;
  sizeProbe.close();
  const double sizeMegabytes = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);

  // --- BLF processing (cold cache) ---
  // TODO: we want to drop caches only when measuring performance
  dropFileCache(filename);

  auto procStartTs = std::chrono::high_resolution_clock::now();
  processFile(filename, skipDecompress);
  auto procEndTs = std::chrono::high_resolution_clock::now();
  const double proc_s = std::chrono::duration<double>(procEndTs - procStartTs).count();

  // --- raw read benchmark (cold cache) ---
  if (runBenchmark) {
    dropFileCache(filename);
    benchmarkRawRead(filename);
  }

  // --- summary ---
  const double compMiB   = static_cast<double>(g_perf.compressedBytes)   / (1024.0 * 1024.0);
  const double decompMiB = static_cast<double>(g_perf.decompressedBytes) / (1024.0 * 1024.0);
  const double pipe_ms   = g_perf.pipelineUs * 1e-3;

  spdlog::info("--- BLF processing ---");
  spdlog::info("processFile took      {:.3f} ms", proc_s * 1000.0);
  spdlog::info("Avg processing speed: {:.2f} MiB/s (compressed throughput)", sizeMegabytes / proc_s);
  spdlog::info("--- Pipeline breakdown ---");
  spdlog::info("  Workers             : {}", g_perf.nThreads);
  spdlog::info("  Containers          : {}", g_perf.containers);
  spdlog::info("  Compressed in       : {:.2f} MiB", compMiB);
  spdlog::info("  Decompressed out    : {:.2f} MiB", decompMiB);
  spdlog::info("  Pipeline wall-clock : {:.3f} ms  (producer + consumers overlapped)", pipe_ms);
  if (!skipDecompress && pipe_ms > 0.0) {
    spdlog::info("  Decomp throughput   : {:.2f} MiB/s compressed  /  {:.2f} MiB/s uncompressed",
                 compMiB  / (pipe_ms * 1e-3),
                 decompMiB / (pipe_ms * 1e-3));
  }
  spdlog::info("  Split objects       : {}  (tail in next container; add --debug for per-split detail)",
               g_perf.splitObjects);
  spdlog::info("--- Decoded objects (by type, sorted by count) ---");
  if (g_perf.objectCounts.empty()) {
    spdlog::info("  (none — run without --no-decompress to decode objects)");
  } else {
    // Sort entries by count descending for readability
    std::vector<std::pair<uint32_t, uint64_t>> sorted(
        g_perf.objectCounts.begin(), g_perf.objectCounts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [type, cnt] : sorted) {
      auto name = objectTypeName(type);
      if (name.empty())
        spdlog::info("  [{:>3}] {:<36} : {}", type, std::format("UNKNOWN_{}", type), cnt);
      else
        spdlog::info("  [{:>3}] {:<36} : {}", type, name, cnt);
    }
  }

  return 0;
}

