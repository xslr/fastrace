#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <fstream>
#include <istream>
#include <mutex>
#include <optional>
#include <cstdint>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <libdeflate.h>
#include <spdlog/spdlog.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t year;
    uint16_t month;
    uint16_t dayOfWeek;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint16_t milliseconds;
} BlfTimeStamp;

typedef struct {
  char signature[4];
  uint32_t headerSize;
  uint32_t apiVersion;
  uint8_t appId;
  uint8_t compressionLevel;
  uint8_t majorVersion;
  uint8_t minorVersion;
  uint64_t compressedSize;
  uint64_t uncompressedSize;
  uint32_t numObj;
  uint32_t appBuildId;
  BlfTimeStamp measurementStartTime;
  BlfTimeStamp measurementEndTime;
  uint64_t nextRestorepoint;
} BlfFileHeader;

typedef struct {
  uint32_t signature;      // "LOBJ"
  uint16_t headerSize;
  uint16_t headerVersion;
  uint32_t objectSize;
  uint32_t objectType;
} BlfObjectHeaderBase;

typedef struct {
  uint32_t flags;
  uint16_t staticSize;
  uint16_t version;
  uint64_t timestamp;
} BlfObjectHeader;

struct CANMessage {
  uint16_t channel;
  uint8_t flags;
  uint8_t dlc;
  uint32_t id;
  uint8_t data[8];
};

struct EthernetFrame {
  uint16_t channel;
  uint16_t flags;
  uint32_t length;
  // Data follows
};
#pragma pack(pop)

enum BLFObjectType : uint32_t {
  CAN_MESSAGE = 1,
  CAN_MESSAGE2 = 86,
  ETHERNET_FRAME = 120,
  CONTAINER = 10
};

bool dumpObjContents = false;

// ---------------------------------------------------------------------------
// Per-run timing accumulators
// ---------------------------------------------------------------------------
struct PerfCounters {
  uint64_t containers        = 0;
  uint64_t compressedBytes   = 0;
  uint64_t decompressedBytes = 0;
  int64_t  pipelineNs        = 0;  // wall-clock: producer + consumers overlapped
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


constexpr uint32_t BLF_LOGG_SIGNATURE = 0x47474f4c;  // "LOGG"
constexpr uint32_t BLF_LOBJ_SIGNATURE = 0x4a424f4c;  // "LOBJ"


// ---------------------------------------------------------------------------
// Cursor – lightweight read-cursor over a contiguous memory region.
// ---------------------------------------------------------------------------
struct Cursor {
    const char* base;
    const char* end;
    const char* pos;

    bool   eof()       const noexcept { return pos >= end; }
    size_t tell()      const noexcept { return static_cast<size_t>(pos - base); }
    size_t remaining() const noexcept { return static_cast<size_t>(end - pos); }

    bool read(void* dst, size_t n) noexcept {
        if (pos + n > end) return false;
        std::memcpy(dst, pos, n);
        pos += n;
        return true;
    }

    const char* peek(size_t n) noexcept {
        if (pos + n > end) return nullptr;
        const char* p = pos;
        pos += n;
        return p;
    }

    bool skip(size_t n) noexcept {
        if (pos + n > end) return false;
        pos += n;
        return true;
    }
};


// ---------------------------------------------------------------------------
// MappedFile – Maps file into memory and pre-faults in the background
// ---------------------------------------------------------------------------
struct MappedFile {
#ifdef _WIN32
    void*  addr = nullptr;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
#else
    void*  addr = MAP_FAILED;
#endif
    size_t size = 0;
    std::thread prefault_thread;

    MappedFile() = default;
    ~MappedFile() {
        if (prefault_thread.joinable()) prefault_thread.join();
#ifdef _WIN32
        if (addr) UnmapViewOfFile(addr);
        if (hMapping) CloseHandle(hMapping);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
#else
        if (addr != MAP_FAILED) munmap(addr, size);
#endif
    }
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    bool open(const std::string& path) {
#ifdef _WIN32
        hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) { spdlog::error("Could not open file {}", path); return false; }

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) {
            spdlog::error("GetFileSizeEx failed for {}", path); return false;
        }
        size = static_cast<size_t>(fileSize.QuadPart);

        hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (hMapping == NULL) { spdlog::error("CreateFileMapping failed for {}", path); return false; }

        addr = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (addr == NULL) { spdlog::error("MapViewOfFile failed for {}", path); return false; }

        prefault_thread = std::thread([this]() {
            HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
            if (hKernel32) {
                typedef BOOL (WINAPI *PrefetchVirtualMemory_t)(HANDLE, ULONG_PTR, PVOID, ULONG);
                auto pPrefetch = (PrefetchVirtualMemory_t)GetProcAddress(hKernel32, "PrefetchVirtualMemory");
                if (pPrefetch) {
                    struct { PVOID VirtualAddress; SIZE_T NumberOfBytes; } entry = { addr, size };
                    pPrefetch(GetCurrentProcess(), 1, &entry, 0);
                    return;
                }
            }
            // Fallback: manually fault pages
            const size_t pageSize = 4096;
            volatile char* p = static_cast<volatile char*>(addr);
            for (size_t i = 0; i < size; i += pageSize) {
                char c = p[i];
                (void)c;
            }
        });
#else
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) { spdlog::error("Could not open file {}", path); return false; }

        struct stat st{};
        if (fstat(fd, &st) != 0) {
            spdlog::error("fstat failed for {}", path); close(fd); return false;
        }
        size = static_cast<size_t>(st.st_size);

        // Map without MAP_POPULATE so mmap returns immediately.
        addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (addr == MAP_FAILED) { spdlog::error("mmap failed for {}", path); return false; }

        // Start dedicated background thread to pre-fault pages at full I/O bandwidth.
        // MADV_POPULATE_READ (Linux 5.14+) synchronously populates page tables,
        // acting exactly like MAP_POPULATE but off the main thread.
        prefault_thread = std::thread([this]() {
#ifndef MADV_POPULATE_READ
#define MADV_POPULATE_READ 22
#endif
            int ret = madvise(addr, size, MADV_POPULATE_READ);
            if (ret != 0) {
                // Fallback to sequential read-ahead
                madvise(addr, size, MADV_SEQUENTIAL);
                madvise(addr, size, MADV_WILLNEED);
            }
        });

#endif
        return true;
    }

    Cursor cursor() const noexcept {
        const char* b = static_cast<const char*>(addr);
        return Cursor{b, b + size, b};
    }
};


// ---------------------------------------------------------------------------
// WorkQueue – bounded SPMC blocking queue for the producer-consumer pipeline.
//
// Capacity is set to a small multiple of the worker count.  This prevents the
// producer from racing far ahead (which would expand memory pressure) while
// still giving workers enough tasks to stay busy across scheduling jitter.
// ---------------------------------------------------------------------------
struct WorkQueue {
  struct Item { const char* compData; size_t compSize; };

  explicit WorkQueue(size_t cap) : cap_(cap), ring_(cap) {}

  // Called by producer.  Blocks if the queue is full.
  void push(Item item) {
    std::unique_lock<std::mutex> lk(mu_);
    cvSpace_.wait(lk, [&] { return count_ < cap_; });
    ring_[tail_] = item;
    tail_ = (tail_ + 1) % cap_;
    ++count_;
    cvWork_.notify_one();
  }

  // Called by consumers.  Returns nullopt when closed AND empty.
  std::optional<Item> pop() {
    std::unique_lock<std::mutex> lk(mu_);
    cvWork_.wait(lk, [&] { return count_ > 0 || closed_; });
    if (count_ == 0) return std::nullopt;
    Item item = ring_[head_];
    head_ = (head_ + 1) % cap_;
    --count_;
    cvSpace_.notify_one();
    return item;
  }

  // Signal that no more items will be pushed.  Wakes all waiting consumers.
  void close() {
    std::unique_lock<std::mutex> lk(mu_);
    closed_ = true;
    cvWork_.notify_all();
  }

private:
  const size_t             cap_;
  std::vector<Item>        ring_;
  size_t                   head_   = 0;
  size_t                   tail_   = 0;
  size_t                   count_  = 0;
  bool                     closed_ = false;
  std::mutex               mu_;
  std::condition_variable  cvWork_;   // consumers wait here
  std::condition_variable  cvSpace_;  // producer waits here
};


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
//     per-thread buffer that is REUSED across all containers on that thread.
//     The reused buffer (~130 KB) stays hot in L3 cache; decompressed bytes
//     are never written to DRAM.
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
  // memory pressure.  Each in-flight item is just a pointer + size (16 B).
  WorkQueue queue(nWorkers * 4);

  // Shared perf counters written atomically by workers
  std::atomic<uint64_t> atomicContainers{0};
  std::atomic<uint64_t> atomicCompressed{0};
  std::atomic<uint64_t> atomicDecompressed{0};

  // ---- Consumer lambda ----
  // Each consumer owns:
  //   • one libdeflate_decompressor (no shared state between threads)
  //   • one reusable output buffer (grows once to max container size, then
  //     reused for all subsequent containers on this thread → stays in L3)
  auto consumer = [&]() {
    libdeflate_decompressor* decomp = libdeflate_alloc_decompressor();
    std::vector<char> localBuf;  // per-thread scratch; never freed until exit

    while (auto item = queue.pop()) {
      atomicCompressed.fetch_add(item->compSize, std::memory_order_relaxed);

      if (!skipDecompress) {
        // Grow only when needed; the buffer stays at the high-water mark
        // for the rest of this thread's lifetime.
        const size_t needed = item->compSize * 6;
        if (localBuf.size() < needed) localBuf.resize(needed);

        size_t actualOut = 0;
        libdeflate_result res;
        do {
          res = libdeflate_zlib_decompress(
              decomp,
              item->compData,  item->compSize,
              localBuf.data(), localBuf.size(),
              &actualOut);
          if (res == LIBDEFLATE_INSUFFICIENT_SPACE) {
            localBuf.resize(localBuf.size() * 2);
            spdlog::debug("localBuf grown to {} B", localBuf.size());
          }
        } while (res == LIBDEFLATE_INSUFFICIENT_SPACE);

        if (res == LIBDEFLATE_SUCCESS) {
          atomicDecompressed.fetch_add(actualOut, std::memory_order_relaxed);
          // TODO: process inner LOBJs in localBuf[0..actualOut)
          // localBuf is hot in L3 cache here; no DRAM round-trip needed.
        } else {
          spdlog::error("libdeflate failed ({})", static_cast<int>(res));
        }
      }

      atomicContainers.fetch_add(1, std::memory_order_relaxed);
    }

    libdeflate_free_decompressor(decomp);
  };

  // ---- Start consumers ----
  std::vector<std::thread> workers;
  workers.reserve(nWorkers);
  for (size_t i = 0; i < nWorkers; ++i) workers.emplace_back(consumer);

  // ---- Producer (runs on calling thread, concurrent with consumers) ----
  // MADV_WILLNEED (set in MappedFile::open) has been pre-loading pages since
  // mmap returned, so many of these touches will hit the page cache.
  auto prodStart = Clock::now();

  BlfObjectHeaderBase base;
  while (!cursor.eof()) {
    if (!findNextLobj(cursor, base.signature)) break;
    if (!cursor.read(reinterpret_cast<char*>(&base) + 4,
                     sizeof(BlfObjectHeaderBase) - 4)) break;

    spdlog::debug("pipeline: type={} objectSize={}", base.objectType, base.objectSize);

    if (base.objectType == CONTAINER) {
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
  spdlog::info("Processing file: {} ({} bytes)", filename, mf.size);

  Cursor cursor = mf.cursor();
  auto fileHeader = readFileHeader(cursor);
  if (!fileHeader) return;

  const size_t nWorkers = std::max<size_t>(1, std::thread::hardware_concurrency());
  spdlog::info("Starting pipeline: {} worker thread(s)", nWorkers);

  auto t0 = Clock::now();
  runPipeline(cursor, *fileHeader, nWorkers, skipDecompress);
  g_perf.pipelineNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
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
    spdlog::error("Usage: {} <filename> [--no-decompress]", argv[0]);
    return 1;
  }

  spdlog::set_level(spdlog::level::info);

  std::string filename     = argv[1];
  bool        skipDecompress = (argc >= 3 && std::string(argv[2]) == "--no-decompress");

  if (skipDecompress)
    spdlog::info("*** --no-decompress: consumers will skip inflate() ***");

  std::ifstream sizeProbe(filename, std::ios::binary | std::ios::ate);
  const size_t fileSize = sizeProbe.is_open() ? static_cast<size_t>(sizeProbe.tellg()) : 0;
  sizeProbe.close();
  const double megabytes = static_cast<double>(fileSize) / (1024.0 * 1024.0);

  // --- BLF processing (cold cache) ---
  dropFileCache(filename);

  auto procStart = std::chrono::high_resolution_clock::now();
  processFile(filename, skipDecompress);
  auto procEnd = std::chrono::high_resolution_clock::now();
  const double proc_s = std::chrono::duration<double>(procEnd - procStart).count();

  // --- raw read benchmark (cold cache) ---
  dropFileCache(filename);
  benchmarkRawRead(filename);

  // --- summary ---
  const double compMiB   = static_cast<double>(g_perf.compressedBytes)   / (1024.0 * 1024.0);
  const double decompMiB = static_cast<double>(g_perf.decompressedBytes) / (1024.0 * 1024.0);
  const double pipe_ms   = g_perf.pipelineNs * 1e-6;

  spdlog::info("--- BLF processing ---");
  spdlog::info("processFile took      {:.3f} ms", proc_s * 1000.0);
  spdlog::info("Avg processing speed: {:.2f} MiB/s (compressed throughput)", megabytes / proc_s);
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

