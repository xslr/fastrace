#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <ctime>
#include <format>
#include <fstream>
#include <istream>
#include <optional>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include <zlib.h>
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


std::string to_hex(char* buf, size_t len) {
  std::string result;
  result.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    result += std::format("{:02x}", static_cast<unsigned char>(buf[i]));
  }
  return result;
}

std::string to_hex(uint32_t num) {
  return to_hex(reinterpret_cast<char*>(&num), 4);
}


constexpr uint32_t BLF_LOGG_SIGNATURE = 0x47474f4c;  // "LOGG"
constexpr uint32_t BLF_LOBJ_SIGNATURE = 0x4a424f4c;  // "LOBJ"


std::optional<BlfObject> nextLogObject(std::istream &file, BlfFileHeader &fileHdr) {
  BlfObject obj;
  bool haveHeader;

  while (!file.eof()) {
    size_t pos = file.tellg();
    spdlog::debug("nextLogObject() @{} (0x{:x})", pos, pos);

    // find start of next "LOBJ". it may not necessarily be 4 byte aligned
    file.read(reinterpret_cast<char*>(&obj.baseHeader.signature), 4);
    spdlog::debug("SIG=0x{:x} read=0x{:x}", BLF_LOBJ_SIGNATURE, obj.baseHeader.signature);
    if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
      spdlog::debug("Found \"LOBJ\" header");
      haveHeader = true;
      break;
    } else if ((obj.baseHeader.signature >> 8) == (BLF_LOBJ_SIGNATURE & 0x00FFFFFF)) {
      spdlog::debug("misalign 1");
      obj.baseHeader.signature = obj.baseHeader.signature >> 8;
      file.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 3, 1);
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
      file.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 2, 2);
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
      file.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 1, 3);
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
    file.read(reinterpret_cast<char*>(&obj.baseHeader) + 4, sizeof(BlfObjectHeaderBase) - 4);
    spdlog::debug("headerSize: {}", obj.baseHeader.headerSize);
    spdlog::debug("headerVersion: {}", obj.baseHeader.headerVersion);
    spdlog::debug("objectSize: {} (0x{:x})", obj.baseHeader.objectSize, obj.baseHeader.objectSize);
    spdlog::debug("objectType: {} (0x{:x})", obj.baseHeader.objectType, obj.baseHeader.objectType);
  }

  #if 0
  file.read(reinterpret_cast<char*>(&obj.baseHeader), sizeof(BlfObjectHeaderBase));
  if (std::string(obj.baseHeader.signature, 4) != "LOBJ") {
    // If we don't find LOBJ, we might be out of sync or at the end
    // TODO: handle out of sync streams. seek to start of next LOBJ

    spdlog::warn("Missing LOBJ signature. Instead of \"LOBJ\", found: {}", to_hex(obj.baseHeader.signature, sizeof(obj.baseHeader.signature)));
  } else {
    spdlog::debug("Found a \"LOBJ\" header");
    spdlog::debug("headerSize: {}", obj.baseHeader.headerSize);
    spdlog::debug("headerVersion: {}", obj.baseHeader.headerVersion);
    spdlog::debug("objectSize: {} (0x{:x})", obj.baseHeader.objectSize, obj.baseHeader.objectSize);
    spdlog::debug("objectType: {} (0x{:x})", obj.baseHeader.objectType, obj.baseHeader.objectType);
  }
  #endif

  // TODO: handoff to object handler


  if (obj.baseHeader.objectType == CONTAINER) {
    spdlog::debug("found a CONTAINER object");
  

  BlfObjectHeader objHeader2;
  file.read(reinterpret_cast<char*>(&objHeader2), sizeof(BlfObjectHeaderBase));
  spdlog::debug("objHeader2.flags: {} 0x{:04x}", objHeader2.flags, objHeader2.flags);
  spdlog::debug("objHeader2.staticSize: {}", objHeader2.staticSize);
  spdlog::debug("objHeader2.version: {}", objHeader2.version);
  spdlog::debug("objHeader2.timestamp: {} 0x{:08x}", objHeader2.timestamp, objHeader2.timestamp);

  std::vector<char> compressed(obj.baseHeader.objectSize - obj.baseHeader.headerSize - sizeof(BlfObjectHeader));
  std::vector<char> decompressed(compressed.size()*5);

  file.read(compressed.data(), obj.baseHeader.objectSize - obj.baseHeader.headerSize - sizeof(BlfObjectHeader));

  // size_t currentPos = file.tellg();
  // spdlog::debug("Current position: {} 0x{:x}", currentPos, currentPos);
  
  // TODO
  // - seek to 4 byte alignment
  // - load all objects
  // - decode zlib
  z_stream zs{};

  zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
  zs.avail_in = compressed.size();
  zs.next_out = reinterpret_cast<Bytef*>(decompressed.data());

  zs.avail_out = decompressed.size();

  if (inflateInit(&zs) != Z_OK) {
    spdlog::error("inflateInit fail");
  }
  int ret = inflate(&zs, Z_FINISH);

  inflateEnd(&zs);

  if (ret == Z_STREAM_END) {
    spdlog::debug("zlib decode successful");

    if (true == dumpObjContents) {
      for (int i=0; i<0x6000; i++) {
        spdlog::trace("{:02x} ", decompressed[i]);
      }
    }
  }
  }

  return obj;
}


// Read and validate the BLF file header ("LOGG" block) from an already-open
// stream. On success the stream is positioned just past the header padding,
// ready for the first LOBJ. Returns nullopt on any error.
std::optional<BlfFileHeader> readFileHeader(std::istream& file) {
  // check minimum size
  file.seekg(0, std::ios::end);
  const auto fileSize = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);

  if (fileSize < sizeof(BlfFileHeader)) {
    spdlog::error("File is too short ({} bytes). stop", fileSize);
    return std::nullopt;
  }

  // read the fixed-size header struct
  BlfFileHeader hdr;
  file.read(reinterpret_cast<char*>(&hdr), sizeof(BlfFileHeader));
  if (!file) {
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
    std::vector<std::byte> pad(padSize);
    file.read(reinterpret_cast<char*>(pad.data()), padSize);
    spdlog::debug("File header pad: {}", to_hex(reinterpret_cast<char*>(pad.data()), padSize));
  }

  return hdr;
}

void processFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    spdlog::error("Could not open file {}", filename);
    return;
  }

  const auto fileSize = [&] {
    file.seekg(0, std::ios::end);
    auto sz = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    return sz;
  }();
  spdlog::info("Processing file: {} ({} bytes)", filename, fileSize);

  auto fileHeader = readFileHeader(file);
  if (!fileHeader) {
    return;  // error already logged inside readFileHeader
  }

  // stream is now positioned at the first LOBJ
  while (!file.eof()) {
    // get log container
    auto obj = nextLogObject(file, *fileHeader);
    if (!obj) {
      return;
    }

    // TODO:process log container
  }
  // [LOBJ|HS|HV|OSIZ|OTYP]
  // 4b    2b 2b 4b   4b
  // @0x90 LOBJ
  //  @0x98 sizeof LOBJ = 0x6b22
  // 0x6bb4 LOBJ
  //  @6bbc sizeof LOBJ =  0x71f2
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
  if (argc != 2) {
    spdlog::error("Usage: {} <filename>", argv[0]);
    return 1;
  }

  // Set log level: change to spdlog::level::debug to see parser internals
  spdlog::set_level(spdlog::level::info);

  std::string filename = argv[1];

  // get file size for speed calculations
  std::ifstream sizeProbe(filename, std::ios::binary | std::ios::ate);
  const size_t fileSize = sizeProbe.is_open() ? static_cast<size_t>(sizeProbe.tellg()) : 0;
  sizeProbe.close();
  const double megabytes = static_cast<double>(fileSize) / (1024.0 * 1024.0);

  // --- BLF processing (cold cache) ---
  dropFileCache(filename);

  auto procStart = std::chrono::high_resolution_clock::now();
  processFile(filename);
  auto procEnd = std::chrono::high_resolution_clock::now();

  const double proc_s = std::chrono::duration<double>(procEnd - procStart).count();

  // --- raw read benchmark (cold cache) ---
  dropFileCache(filename);
  benchmarkRawRead(filename);

  // --- summary (printed last so it doesn't scroll away) ---
  spdlog::info("--- BLF processing ---");
  spdlog::info("processFile took {:.3f} ms", proc_s * 1000.0);
  spdlog::info("Avg processing speed: {:.2f} MiB/s", megabytes / proc_s);

  return 0;
}
