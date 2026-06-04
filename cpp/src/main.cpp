#include <chrono>
#include <cstdio>
#include <ctime>
#include <format>
#include <iostream>
#include <fstream>
#include <istream>
#include <optional>
#include <ostream>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include <zlib.h>

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


void print_hex(char* buf, size_t len, bool newline=true) {
  for (size_t i = 0; i < len; i++) {
    printf("%02x", buf[i]);
  }

  if (newline) {
    printf("\n");
  }
}

void print_hex(uint32_t num, bool newline=true) {
  print_hex(reinterpret_cast<char*>(&num), 4, newline);
}


constexpr uint32_t BLF_LOGG_SIGNATURE = 0x47474f4c;  // "LOGG"
constexpr uint32_t BLF_LOBJ_SIGNATURE = 0x4a424f4c;  // "LOBJ"


std::optional<BlfObject> nextLogObject(std::istream &file, BlfFileHeader &fileHdr) {
  BlfObject obj;
  bool haveHeader;

  while (!file.eof()) {
    size_t pos = file.tellg();
    std::cout << std::format("nextLogObject() @{} (0x{:x})\n", pos, pos);

    // find start of next "LOBJ". it may not necessarily be 4 byte aligned
    file.read(reinterpret_cast<char*>(&obj.baseHeader.signature), 4);
    std::cout << std::format("SIG=0x{:x} read=0x{:x}\n", BLF_LOBJ_SIGNATURE, obj.baseHeader.signature);
    if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
      std::cout << "Found \"LOBJ\" header\n";
      haveHeader = true;
      break;
    } else if ((obj.baseHeader.signature >> 8) == (BLF_LOBJ_SIGNATURE & 0x00FFFFFF)) {
      std::cout << "misalign 1\n";
      obj.baseHeader.signature = obj.baseHeader.signature >> 8;
      file.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 3, 1);
      std::cout << std::format("CATCHUP read=0x{:x}\n", obj.baseHeader.signature);
      if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
        std::cout << std::format("signature (0x{:x}) matches LOBJ\n", obj.baseHeader.signature);
        haveHeader = true;
        break;
      } else {
        std::cout << std::format("could not get a valid LOBJ signature 0x{:x}", obj.baseHeader.signature);
        haveHeader = false;
        exit(1);
      }
    } else if ((obj.baseHeader.signature >> 16) == (BLF_LOBJ_SIGNATURE & 0x0000FFFF)) {
      std::cout << "misalign 2\n";
      obj.baseHeader.signature = obj.baseHeader.signature >> 16;
      file.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 2, 2);
      std::cout << std::format("CATCHUP read=0x{:x}\n", obj.baseHeader.signature);
      if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
        std::cout << std::format("signature (0x{:x}) matches LOBJ\n", obj.baseHeader.signature);
        haveHeader = true;
        break;
      } else {
        std::cout << std::format("could not get a valid LOBJ signature 0x{:x}", obj.baseHeader.signature);
        haveHeader = false;
        exit(2);
      }
    } else if ((obj.baseHeader.signature >> 24) == (BLF_LOBJ_SIGNATURE & 0x000000FF)) {
      std::cout << "misalign 3\n";
      obj.baseHeader.signature = obj.baseHeader.signature >> 24;
      file.read(reinterpret_cast<char*>(&obj.baseHeader.signature) + 1, 3);
      std::cout << std::format("CATCHUP read=0x{:x}\n", obj.baseHeader.signature);
      if (obj.baseHeader.signature == BLF_LOBJ_SIGNATURE) {
        std::cout << std::format("signature (0x{:x}) matches LOBJ\n", obj.baseHeader.signature);
        haveHeader = true;
        break;
      } else {
        std::cout << std::format("could not get a valid LOBJ signature 0x{:x}", obj.baseHeader.signature);
        haveHeader = false;
        exit(3);
      }
    } else {
      printf("Missing LOBJ signature. stop. Instead of \"LOBJ\", i found this:\n    ");
      print_hex(obj.baseHeader.signature);
      uint32_t shift1L = obj.baseHeader.signature << 8;
      uint32_t shift2L = obj.baseHeader.signature << 16;
      uint32_t shift3L = obj.baseHeader.signature << 24;

      uint32_t shift1r = obj.baseHeader.signature >> 8;
      uint32_t shift2r = obj.baseHeader.signature >> 16;
      uint32_t shift3r = obj.baseHeader.signature >> 24;

      std::cout << std::format("0x{:x} L shifts: 0x{:x} 0x{:x} 0x{:x} \n0x{:x} R shifts: 0x{:x} 0x{:x} 0x{:x}\n",
                               obj.baseHeader.signature, shift1L, shift2L, shift3L,
                               obj.baseHeader.signature, shift1r, shift2r, shift3r);
    }
  }

  if (haveHeader) {
    file.read(reinterpret_cast<char*>(&obj.baseHeader) + 4, sizeof(BlfObjectHeaderBase) - 4);
    std::cout << std::format("headerSize: {}\n", obj.baseHeader.headerSize);
    std::cout << std::format("headerVersion: {}\n", obj.baseHeader.headerVersion);
    std::cout << std::format("objectSize: {} (0x{:x})\n", obj.baseHeader.objectSize, obj.baseHeader.objectSize);
    std::cout << std::format("objectType: {} (0x{:x})\n", obj.baseHeader.objectType, obj.baseHeader.objectType);
  }

  #if 0
  file.read(reinterpret_cast<char*>(&obj.baseHeader), sizeof(BlfObjectHeaderBase));
  if (std::string(obj.baseHeader.signature, 4) != "LOBJ") {
    // If we don't find LOBJ, we might be out of sync or at the end
    // TODO: handle out of sync streams. seek to start of next LOBJ

    printf("Missing LOBJ signature. stop. Instead of \"LOBJ\", i found this:\n    ");
    print_hex(obj.baseHeader.signature, sizeof(obj.baseHeader.signature));
  } else {
    std::cout << "Found a \"LOBJ\" header\n";
    std::cout << std::format("headerSize: {}\n", obj.baseHeader.headerSize);
    std::cout << std::format("headerVersion: {}\n", obj.baseHeader.headerVersion);
    std::cout << std::format("objectSize: {} (0x{:x})\n", obj.baseHeader.objectSize, obj.baseHeader.objectSize);
    std::cout << std::format("objectType: {} (0x{:x})\n", obj.baseHeader.objectType, obj.baseHeader.objectType);
  }
  #endif

  // TODO: handoff to object handler


  if (obj.baseHeader.objectType == CONTAINER) {
    std::cout << "found a CONTAINER object\n";
  

  BlfObjectHeader objHeader2;
  file.read(reinterpret_cast<char*>(&objHeader2), sizeof(BlfObjectHeaderBase));
  std::cout << std::format("objHeader2.flags: {} 0x{:04x}\n", objHeader2.flags, objHeader2.flags);
  std::cout << std::format("objHeader2.staticSize: {}\n", objHeader2.staticSize);
  std::cout << std::format("objHeader2.version: {}\n", objHeader2.version);
  std::cout << std::format("objHeader2.timestamp: {} 0x{:08x}\n", objHeader2.timestamp, objHeader2.timestamp);

  std::vector<char> compressed(obj.baseHeader.objectSize - obj.baseHeader.headerSize - sizeof(BlfObjectHeader));
  std::vector<char> decompressed(compressed.size()*5);

  file.read(compressed.data(), obj.baseHeader.objectSize - obj.baseHeader.headerSize - sizeof(BlfObjectHeader));

  // size_t currentPos = file.tellg();
  // std::cout << std::format("Current position: {} 0x{:x}\n", currentPos, currentPos);
  
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
    std::cerr << "inflateInit fail\n";
  }
  int ret = inflate(&zs, Z_FINISH);

  inflateEnd(&zs);

  if (ret == Z_STREAM_END) {
    std::cout << "zlib decode successful\n";

    if (true == dumpObjContents) {
      for (int i=0; i<0x6000; i++) {
        std::cout << std::format("{:02x} ", decompressed[i]);
      }
      std::cout << std::endl;
    }
  }
  }

  return obj;
}


void processFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file " << filename << std::endl;
    return;
  }

  // get file size
  file.seekg(0, std::ios::end);
  std::streampos fileSize = file.tellg();
  printf("Processing file: %s (%lu)\n", filename.c_str(), static_cast<size_t>(fileSize));
  file.seekg(0, std::ios::beg);

  if (static_cast<size_t>(fileSize) < 4) {
    std::cerr << "File is too short (" << static_cast<size_t>(fileSize) << "). stop\n";
    return;
  }

  // read file header and check for standard LOGG signature
  BlfFileHeader fileHeader;
  file.read(reinterpret_cast<char*>(&fileHeader), sizeof(BlfFileHeader));

  if (std::string_view(fileHeader.signature, sizeof(fileHeader.signature)) != "LOGG") {
    std::cerr << "File does not contain valid header\n";
  } else {
    std::cout << "Found valid \"LOGG\" header\n";
    std::cout << std::format("Header size: {} 0x{:x}\n", fileHeader.headerSize, fileHeader.headerSize);
    std::cout << std::format("apiVersion: {} (0x{:x})\n", fileHeader.apiVersion, fileHeader.apiVersion);
    std::cout << std::format("appid: {} (0x{:x})\n", fileHeader.appId, fileHeader.appId);
    std::cout << std::format("compression: {}\n",fileHeader.compressionLevel);
    std::cout << std::format("majorver: {}\n", fileHeader.majorVersion);
    std::cout << std::format("minorver: {}\n", fileHeader.minorVersion);
    std::cout << std::format("compressedSize: {} (0x{:x})\n", fileHeader.compressedSize, fileHeader.compressedSize);
    std::cout << std::format("uncompressedSize: {} (0x{:x})\n", fileHeader.uncompressedSize, fileHeader.uncompressedSize);
    std::cout << "numObj: " << fileHeader.numObj << std::endl;
    std::cout << std::format("nextRestorepoint: {} 0x{:x}\n",fileHeader.nextRestorepoint, fileHeader.nextRestorepoint);
  }

  // read unknown file header bytes
  std::cout << "File header pad: ";
  const size_t fileHeaderPadSize = fileHeader.headerSize - sizeof(BlfFileHeader);
  std::vector<std::byte> fileHeaderPad(fileHeaderPadSize);
  file.read(reinterpret_cast<char*>(fileHeaderPad.data()), fileHeaderPadSize);
  print_hex(reinterpret_cast<char*>(fileHeaderPad.data()), fileHeaderPadSize);

  // we have read the file header and any unknown padding.
  // now try to read the next log object (LOBJ)
  
  while (!file.eof()) {
    // get log container
    auto obj = nextLogObject(file, fileHeader);
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

  std::cout << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  std::string filename = argv[1];

  auto start = std::chrono::high_resolution_clock::now();
  processFile(filename);
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> diff = end - start;
  std::cout << "processFile took " << diff.count() * 1000 << " ms" << std::endl;

  return 0;
}
