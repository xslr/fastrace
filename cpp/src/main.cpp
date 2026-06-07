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
#include <vector>
#include <libdeflate.h>
#include <spdlog/spdlog.h>

#include "Cursor.h"
#include "MappedFile.h"
#include "WorkQueue.h"
#include "BlfTypes.h"


// ratio used to allocate buffer for decompressed data. if insufficient, a reallocation would be triggered
// based on real traces, compressed objects seem to have a compression ratio of 4.5.
constexpr size_t DECOMP_BUFFER_PREALLOC_RATIO = 6;
constexpr uint32_t BLF_LOGG_SIGNATURE = 0x47474f4c;  // "LOGG"
constexpr uint32_t BLF_LOBJ_SIGNATURE = 0x4a424f4c;  // "LOBJ"

bool dumpObjContents = false;

// ---------------------------------------------------------------------------
// Per-run timing accumulators
// ---------------------------------------------------------------------------
struct PerfCounters {
  uint64_t containers        = 0;
  uint64_t compressedBytes   = 0;
  uint64_t decompressedBytes = 0;
  int64_t  pipelineUs        = 0;  // wall-clock: producer + consumers overlapped
  size_t   nThreads          = 0;
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
// findNextLobj – scan cursor forward until the "LOBJ" signature is found,
// handling up to 3 bytes of misalignment (e.g. inter-object padding).
// On success: sigOut == BLF_LOBJ_SIGNATURE, cursor is past the 4-byte sig.
// ---------------------------------------------------------------------------
static bool findNextLobj(Cursor& cursor, uint32_t& sigOut) {
  while (!cursor.eof()) {
    if (!cursor.read(&sigOut, 4)) return false;
    if (sigOut == BLF_LOBJ_SIGNATURE) return true;

    if ((sigOut >> 8) == (BLF_LOBJ_SIGNATURE & 0x00FFFFFF)) {
      sigOut >>= 8;
      if (!cursor.read(reinterpret_cast<char*>(&sigOut) + 3, 1)) return false;
      if (sigOut == BLF_LOBJ_SIGNATURE) return true;
      spdlog::error("unrecoverable LOBJ misalign-1: 0x{:x}", sigOut); return false;
    }
    if ((sigOut >> 16) == (BLF_LOBJ_SIGNATURE & 0x0000FFFF)) {
      sigOut >>= 16;
      if (!cursor.read(reinterpret_cast<char*>(&sigOut) + 2, 2)) return false;
      if (sigOut == BLF_LOBJ_SIGNATURE) return true;
      spdlog::error("unrecoverable LOBJ misalign-2: 0x{:x}", sigOut); return false;
    }
    if ((sigOut >> 24) == (BLF_LOBJ_SIGNATURE & 0x000000FF)) {
      sigOut >>= 24;
      if (!cursor.read(reinterpret_cast<char*>(&sigOut) + 1, 3)) return false;
      if (sigOut == BLF_LOBJ_SIGNATURE) return true;
      spdlog::error("unrecoverable LOBJ misalign-3: 0x{:x}", sigOut); return false;
    }

    spdlog::debug("scan: 0x{:x} is not LOBJ, continuing", sigOut);
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
                        bool skipDecompress) {
  auto decomp = std::unique_ptr<libdeflate_decompressor, decltype(&libdeflate_free_decompressor)>(
      libdeflate_alloc_decompressor(), libdeflate_free_decompressor);
  if (!decomp) {
    spdlog::error("Failed to allocate libdeflate decompressor");
    return;
  }
  std::vector<char> localBuf;  // per-thread scratch; never freed until exit

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
      // TODO: process inner LOBJs in localBuf[0..actualOut)
      // localBuf is hot in L3 cache here; no DRAM round-trip needed.
    } else {
      spdlog::error("libdeflate failed ({})", static_cast<int>(res));
    }
  }
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

  // ---- Start consumers ----
  std::vector<std::thread> workers;
  workers.reserve(nWorkers);
  for (size_t i = 0; i < nWorkers; ++i) {
    workers.emplace_back(runConsumer, std::ref(queue), std::ref(atomicContainers),
                         std::ref(atomicCompressed), std::ref(atomicDecompressed),
                         skipDecompress);
  }

  // ---- Producer (runs on calling thread, concurrent with consumers) ----
  // MADV_WILLNEED (set in MappedFile::open) has been pre-loading pages since
  // mmap returned, so many of these touches will hit the page cache.
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

  queue.close();  // signal: no more items will be pushed
  for (auto& w : workers) w.join();

  g_perf.containers        = atomicContainers.load();
  g_perf.compressedBytes   = atomicCompressed.load();
  g_perf.decompressedBytes = atomicDecompressed.load();
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
    if (file.gcount() == 0) break;
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
    spdlog::error("Usage: {} <filename> [--no-decompress] [--benchmark]", argv[0]);
    return 1;
  }

  spdlog::set_level(spdlog::level::info);

  std::string filename = argv[1];
  bool skipDecompress = false;
  bool runBenchmark = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--no-decompress") skipDecompress = true;
    else if (arg == "--benchmark") runBenchmark = true;
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

  return 0;
}

