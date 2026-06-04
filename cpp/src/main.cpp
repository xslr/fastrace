#include <chrono>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <fstream>
#include <istream>
#include <optional>
#include <cstdint>
#include <string>
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

typedef struct BlfObject {
  BlfObjectHeader extHeader;
  BlfObjectHeaderBase baseHeader;
  std::vector<char> payload;
} BlfObject;


enum BLFObjectType : uint32_t {
  CAN_MESSAGE = 1,
  CAN_MESSAGE2 = 86,
  ETHERNET_FRAME = 120,
  CONTAINER = 10
};

bool dumpObjContents = false;

// ---------------------------------------------------------------------------
// Per-run timing accumulators – populated by nextLogObject, printed by main.
// ---------------------------------------------------------------------------
struct PerfCounters {
  uint64_t containers      = 0;
  uint64_t compressedBytes = 0;
  uint64_t decompressedBytes = 0;
  int64_t  inflateNs       = 0;   // total ns spent inside inflate()
  int64_t  parseNs         = 0;   // total ns spent outside inflate()
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
// All reads are bounds-checked; no heap allocations, no syscalls.
// ---------------------------------------------------------------------------
struct Cursor {
    const char* base;    // start of mapped region
    const char* end;     // one-past-end of mapped region
    const char* pos;     // current read position

    bool   eof()       const noexcept { return pos >= end; }
    size_t tell()      const noexcept { return static_cast<size_t>(pos - base); }
    size_t remaining() const noexcept { return static_cast<size_t>(end - pos); }

    // Copy n bytes into dst and advance.  Returns false if not enough data.
    bool read(void* dst, size_t n) noexcept {
        if (pos + n > end) return false;
        std::memcpy(dst, pos, n);
        pos += n;
        return true;
    }

    // Return a direct pointer into the mapped region and advance (zero-copy).
    // Returns nullptr if not enough data remains.
    const char* peek(size_t n) noexcept {
        if (pos + n > end) return nullptr;
        const char* p = pos;
        pos += n;
        return p;
    }

    // Skip n bytes without reading them.
    bool skip(size_t n) noexcept {
        if (pos + n > end) return false;
        pos += n;
        return true;
    }
};


// ---------------------------------------------------------------------------
// MappedFile – RAII wrapper around mmap.  The file descriptor is closed
// immediately after mmap() so callers need no separate fd management.
// ---------------------------------------------------------------------------
struct MappedFile {
    void*  addr = MAP_FAILED;
    size_t size = 0;

    MappedFile() = default;
    ~MappedFile() {
        if (addr != MAP_FAILED) munmap(addr, size);
    }
    // Non-copyable
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    // Open path and map it read-only.  Returns false on any error.
    bool open(const std::string& path) {
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            spdlog::error("Could not open file {}", path);
            return false;
        }
        struct stat st{};
        if (fstat(fd, &st) != 0) {
            spdlog::error("fstat failed for {}", path);
            close(fd);
            return false;
        }
        size = static_cast<size_t>(st.st_size);
        addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);  // fd can be closed immediately after mmap()
        if (addr == MAP_FAILED) {
            spdlog::error("mmap failed for {}", path);
            return false;
        }
        // Hint the kernel to prefetch pages ahead sequentially
        madvise(addr, size, MADV_SEQUENTIAL);
        return true;
    }

    // Return a Cursor positioned at the start of the mapping.
    Cursor cursor() const noexcept {
        const char* b = static_cast<const char*>(addr);
        return Cursor{b, b + size, b};
    }
};


// ---------------------------------------------------------------------------
// nextLogObject – parse the next LOBJ from the cursor.
//
// decompBuf:    caller-owned buffer reused across calls; grown on demand.
// decompressor: caller-owned libdeflate decompressor allocated once and reused
//               across all objects — libdeflate has no per-call state mutation.
// skipDecompress: when true, skip decompression entirely (used to isolate
//               whether decompression is the bottleneck).
// ---------------------------------------------------------------------------
std::optional<BlfObject> nextLogObject(Cursor& cursor, BlfFileHeader& fileHdr,
                                       std::vector<char>& decompBuf,
                                       libdeflate_decompressor* decompressor,
                                       bool skipDecompress) {
  using Clock = std::chrono::steady_clock;

  BlfObject obj;
  bool haveHeader = false;

  auto parseT0 = Clock::now();

  while (!cursor.eof()) {
    size_t pos = cursor.tell();
    spdlog::debug("nextLogObject() @{} (0x{:x})", pos, pos);

    // find start of next "LOBJ". it may not necessarily be 4 byte aligned
    if (!cursor.read(&obj.baseHeader.signature, 4)) break;
    spdlog::debug("SIG=0x{:x} read=0x{:x}", BLF_LOBJ_SIGNATURE, obj.baseHeader.signature);
    if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
      spdlog::debug("Found \"LOBJ\" header");
      haveHeader = true;
      break;
    } else if ((obj.baseHeader.signature >> 8) == (BLF_LOBJ_SIGNATURE & 0x00FFFFFF)) {
      spdlog::debug("misalign 1");
      obj.baseHeader.signature = obj.baseHeader.signature >> 8;
      if (!cursor.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 3, 1)) break;
      spdlog::debug("CATCHUP read=0x{:x}", obj.baseHeader.signature);
      if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
        spdlog::debug("signature (0x{:x}) matches LOBJ", obj.baseHeader.signature);
        haveHeader = true;
        break;
      } else {
        spdlog::error("could not get a valid LOBJ signature 0x{:x}", obj.baseHeader.signature);
        haveHeader = false;
        exit(1);
      }
    } else if ((obj.baseHeader.signature >> 16) == (BLF_LOBJ_SIGNATURE & 0x0000FFFF)) {
      spdlog::debug("misalign 2");
      obj.baseHeader.signature = obj.baseHeader.signature >> 16;
      if (!cursor.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 2, 2)) break;
      spdlog::debug("CATCHUP read=0x{:x}", obj.baseHeader.signature);
      if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
        spdlog::debug("signature (0x{:x}) matches LOBJ", obj.baseHeader.signature);
        haveHeader = true;
        break;
      } else {
        spdlog::error("could not get a valid LOBJ signature 0x{:x}", obj.baseHeader.signature);
        haveHeader = false;
        exit(2);
      }
    } else if ((obj.baseHeader.signature >> 24) == (BLF_LOBJ_SIGNATURE & 0x000000FF)) {
      spdlog::debug("misalign 3");
      obj.baseHeader.signature = obj.baseHeader.signature >> 24;
      if (!cursor.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 1, 3)) break;
      spdlog::debug("CATCHUP read=0x{:x}", obj.baseHeader.signature);
      if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
        spdlog::debug("signature (0x{:x}) matches LOBJ", obj.baseHeader.signature);
        haveHeader = true;
        break;
      } else {
        spdlog::error("could not get a valid LOBJ signature 0x{:x}", obj.baseHeader.signature);
        haveHeader = false;
        exit(3);
      }
    } else {
      spdlog::warn("Missing LOBJ signature. Instead of \"LOBJ\", found: {}", to_hex(obj.baseHeader.signature));
      uint32_t shift1L = obj.baseHeader.signature << 8;
      uint32_t shift2L = obj.baseHeader.signature << 16;
      uint32_t shift3L = obj.baseHeader.signature << 24;

      uint32_t shift1r = obj.baseHeader.signature >> 8;
      uint32_t shift2r = obj.baseHeader.signature >> 16;
      uint32_t shift3r = obj.baseHeader.signature >> 24;

      spdlog::debug("0x{:x} L shifts: 0x{:x} 0x{:x} 0x{:x}", obj.baseHeader.signature, shift1L, shift2L, shift3L);
      spdlog::debug("0x{:x} R shifts: 0x{:x} 0x{:x} 0x{:x}", obj.baseHeader.signature, shift1r, shift2r, shift3r);
    }
  }

  if (haveHeader) {
    cursor.read(reinterpret_cast<char*>(&obj.baseHeader) + 4, sizeof(BlfObjectHeaderBase) - 4);
    spdlog::debug("headerSize: {}", obj.baseHeader.headerSize);
    spdlog::debug("headerVersion: {}", obj.baseHeader.headerVersion);
    spdlog::debug("objectSize: {} (0x{:x})", obj.baseHeader.objectSize, obj.baseHeader.objectSize);
    spdlog::debug("objectType: {} (0x{:x})", obj.baseHeader.objectType, obj.baseHeader.objectType);
  }

  // TODO: handoff to object handler

  if (obj.baseHeader.objectType == CONTAINER) {
    spdlog::debug("found a CONTAINER object");

    BlfObjectHeader objHeader2;
    cursor.read(reinterpret_cast<char*>(&objHeader2), sizeof(BlfObjectHeaderBase));
    spdlog::debug("objHeader2.flags: {} 0x{:04x}", objHeader2.flags, objHeader2.flags);
    spdlog::debug("objHeader2.staticSize: {}", objHeader2.staticSize);
    spdlog::debug("objHeader2.version: {}", objHeader2.version);
    spdlog::debug("objHeader2.timestamp: {} 0x{:08x}", objHeader2.timestamp, objHeader2.timestamp);

    const size_t compressedSize =
        obj.baseHeader.objectSize - obj.baseHeader.headerSize - sizeof(BlfObjectHeader);

    // Zero-copy: point directly into the mapped region rather than copying
    // the compressed bytes into a separate heap buffer.
    const char* compressedData = cursor.peek(compressedSize);

    g_perf.containers++;
    g_perf.compressedBytes += compressedSize;

    if (!skipDecompress) {
      // --- timed libdeflate_zlib_decompress() ---
      // libdeflate operates on the whole input buffer at once — a perfect
      // match for our mmap'd data.  No streaming state, no per-call alloc.
      // Retry with a larger buffer on the rare INSUFFICIENT_SPACE result.
      g_perf.parseNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
          Clock::now() - parseT0).count();

      auto inflateT0 = Clock::now();

      // Start at 5× compressed size; double on each INSUFFICIENT_SPACE retry.
      if (decompBuf.size() < compressedSize * 5) decompBuf.resize(compressedSize * 5);

      size_t actualOut = 0;
      libdeflate_result res;
      do {
        res = libdeflate_zlib_decompress(
            decompressor,
            compressedData, compressedSize,
            decompBuf.data(), decompBuf.size(),
            &actualOut);
        if (res == LIBDEFLATE_INSUFFICIENT_SPACE) {
          decompBuf.resize(decompBuf.size() * 2);
          spdlog::warn("decompBuf too small, doubled to {} bytes", decompBuf.size());
        }
      } while (res == LIBDEFLATE_INSUFFICIENT_SPACE);

      g_perf.inflateNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
          Clock::now() - inflateT0).count();
      parseT0 = Clock::now();

      if (res == LIBDEFLATE_SUCCESS) {
        g_perf.decompressedBytes += actualOut;
        spdlog::debug("libdeflate decode successful ({} bytes)", actualOut);

        if (true == dumpObjContents) {
          for (int i=0; i<0x6000; i++) {
            spdlog::trace("{:02x} ", decompBuf[i]);
          }
        }
      } else {
        spdlog::error("libdeflate_zlib_decompress failed ({})", static_cast<int>(res));
      }
    }
  }

  g_perf.parseNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
      Clock::now() - parseT0).count();

  return obj;
}


// Read and validate the BLF file header ("LOGG" block) from the cursor.
// On success the cursor is positioned just past the header padding,
// ready for the first LOBJ. Returns nullopt on any error.
std::optional<BlfFileHeader> readFileHeader(Cursor& cursor) {
  const size_t fileSize = static_cast<size_t>(cursor.end - cursor.base);

  if (fileSize < sizeof(BlfFileHeader)) {
    spdlog::error("File is too short ({} bytes). stop", fileSize);
    return std::nullopt;
  }

  // read the fixed-size header struct
  BlfFileHeader hdr;
  if (!cursor.read(&hdr, sizeof(BlfFileHeader))) {
    spdlog::error("Failed to read file header");
    return std::nullopt;
  }

  // validate LOGG signature
  if (std::string_view(hdr.signature, sizeof(hdr.signature)) != "LOGG") {
    spdlog::error("File does not contain valid LOGG header");
    return std::nullopt;
  }

  spdlog::info("Found valid \"LOGG\" header");
  spdlog::debug("Header size:       {} (0x{:x})", hdr.headerSize, hdr.headerSize);
  spdlog::debug("apiVersion:        {} (0x{:x})", hdr.apiVersion, hdr.apiVersion);
  spdlog::debug("appId:             {} (0x{:x})", hdr.appId, hdr.appId);
  spdlog::debug("compressionLevel:  {}", hdr.compressionLevel);
  spdlog::debug("version:           {}.{}", hdr.majorVersion, hdr.minorVersion);
  spdlog::debug("compressedSize:    {} (0x{:x})", hdr.compressedSize, hdr.compressedSize);
  spdlog::debug("uncompressedSize:  {} (0x{:x})", hdr.uncompressedSize, hdr.uncompressedSize);
  spdlog::debug("numObj:            {}", hdr.numObj);
  spdlog::debug("nextRestorepoint:  {} (0x{:x})", hdr.nextRestorepoint, hdr.nextRestorepoint);

  // consume any padding bytes between the fixed struct and the first LOBJ
  const size_t padSize = hdr.headerSize - sizeof(BlfFileHeader);
  if (padSize > 0) {
    const char* pad = cursor.peek(padSize);
    if (pad) {
      spdlog::debug("File header pad: {}", to_hex(pad, padSize));
    }
  }

  return hdr;
}

void processFile(const std::string& filename, bool skipDecompress) {
  MappedFile mf;
  if (!mf.open(filename)) return;  // errors already logged inside open()

  spdlog::info("Processing file: {} ({} bytes)", filename, mf.size);

  Cursor cursor = mf.cursor();

  auto fileHeader = readFileHeader(cursor);
  if (!fileHeader) return;  // error already logged inside readFileHeader()

  // Pre-allocate a reusable decompression buffer.  It is grown on demand
  // (doubling semantics from std::vector::resize) but never shrunk, so
  // the allocator is called at most O(log(max_object_size)) times total.
  std::vector<char> decompBuf(1 * 1024 * 1024);  // 1 MiB initial capacity

  // Allocate one libdeflate decompressor and reuse it across every CONTAINER
  // object.  libdeflate has no mutable per-call state — no Reset() needed.
  libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
  if (!decompressor) {
    spdlog::error("libdeflate_alloc_decompressor failed");
    return;
  }

  // cursor is now positioned at the first LOBJ
  while (!cursor.eof()) {
    // get log container
    auto obj = nextLogObject(cursor, *fileHeader, decompBuf, decompressor, skipDecompress);
    if (!obj) break;

    // TODO: process log container
  }

  libdeflate_free_decompressor(decompressor);
  // MappedFile destructor calls munmap() automatically
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
    // intentionally empty – just drain the file as fast as possible
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
// Does not require root (unlike /proc/sys/vm/drop_caches).
void dropFileCache(const std::string& filename) {
  int fd = open(filename.c_str(), O_RDONLY);
  if (fd < 0) {
    spdlog::warn("could not open file for cache drop");
    return;
  }

  // flush any dirty pages to disk first
  fdatasync(fd);

  // advise the kernel to release cached pages for this file
  off_t len = lseek(fd, 0, SEEK_END);
  if (len > 0) {
    int ret = posix_fadvise(fd, 0, len, POSIX_FADV_DONTNEED);
    if (ret != 0) {
      spdlog::warn("posix_fadvise failed ({})", ret);
    } else {
      spdlog::debug("Page cache dropped for file");
    }
  }
  close(fd);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    spdlog::error("Usage: {} <filename> [--no-decompress]", argv[0]);
    return 1;
  }

  // Set log level: change to spdlog::level::debug to see parser internals
  spdlog::set_level(spdlog::level::info);

  std::string filename = argv[1];
  bool skipDecompress = (argc >= 3 && std::string(argv[2]) == "--no-decompress");

  if (skipDecompress) {
    spdlog::info("*** --no-decompress mode: inflate() will be skipped ***");
  }

  // get file size for speed calculations
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

  // --- timing breakdown ---
  const double inflate_s = static_cast<double>(g_perf.inflateNs) * 1e-9;
  const double parse_s   = static_cast<double>(g_perf.parseNs)   * 1e-9;
  const double compMiB   = static_cast<double>(g_perf.compressedBytes)   / (1024.0 * 1024.0);
  const double decompMiB = static_cast<double>(g_perf.decompressedBytes) / (1024.0 * 1024.0);

  spdlog::info("--- BLF processing ---");
  spdlog::info("processFile took    {:.3f} ms", proc_s * 1000.0);
  spdlog::info("Avg processing speed: {:.2f} MiB/s (compressed throughput)", megabytes / proc_s);
  spdlog::info("--- Timing breakdown ---");
  spdlog::info("  Containers processed : {}", g_perf.containers);
  spdlog::info("  Compressed in        : {:.2f} MiB", compMiB);
  spdlog::info("  Decompressed out     : {:.2f} MiB", decompMiB);
  if (!skipDecompress) {
    spdlog::info("  inflate() time       : {:.3f} ms  ({:.1f}% of total)",
                 inflate_s * 1000.0, 100.0 * inflate_s / proc_s);
    spdlog::info("  inflate() throughput : {:.2f} MiB/s (compressed in)",
                 compMiB / inflate_s);
    spdlog::info("  parse/other time     : {:.3f} ms  ({:.1f}% of total)",
                 parse_s * 1000.0, 100.0 * parse_s / proc_s);
  }

  return 0;
}
