#include <cstdio>
#include <ctime>
#include <format>
#include <iostream>
#include <fstream>
#include <iterator>
#include <ostream>
#include <cstdint>
#include <string>
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
  char signature[4];      // "LOBJ"
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

void print_hex(char* buf, size_t len, bool newline=true) {
  for (size_t i = 0; i < len; i++) {
    printf("%02x", buf[i]);
  }

  if (newline) {
    printf("\n");
  }
}

void processFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file " << filename << std::endl;
    return;
  }

  file.seekg(0, std::ios::end);
  std::streampos fileSize = file.tellg();
  printf("Processing file: %s (%lu)\n", filename.c_str(), static_cast<size_t>(fileSize));
  file.seekg(0, std::ios::beg);

  if (static_cast<size_t>(fileSize) < 4) {
    std::cerr << "File is too short (" << static_cast<size_t>(fileSize) << "). stop\n";
    return;
  }

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
    //std::cout << fileHeader.measurementStartTime;
    //std::cout << fileHeader.measurementEndTime;
    std::cout << std::format("nextRestorepoint: {} 0x{:x}\n",fileHeader.nextRestorepoint, fileHeader.nextRestorepoint);
  }

  std::cout << "File header pad: ";
  const size_t fileHeaderPadSize = fileHeader.headerSize - sizeof(BlfFileHeader);
  std::vector<std::byte> fileHeaderPad(fileHeaderPadSize);
  file.read(reinterpret_cast<char*>(fileHeaderPad.data()), fileHeaderPadSize);
  print_hex(reinterpret_cast<char*>(fileHeaderPad.data()), fileHeaderPadSize);

  // we have read the file header and any unknown padding.
  // now try to read the next log object (LOBJ)
  BlfObjectHeaderBase objHeaderBase;
  file.read(reinterpret_cast<char*>(&objHeaderBase), sizeof(BlfObjectHeaderBase));
  if (std::string(objHeaderBase.signature, 4) != "LOBJ") {
    // If we don't find LOBJ, we might be out of sync or at the end
    printf("Missing LOBJ signature. stop.\n");
    print_hex(objHeaderBase.signature, sizeof(objHeaderBase.signature));
  } else {
    std::cout << "Found a \"LOBJ\" header\n";
    std::cout << std::format("headerSize: {}\n", objHeaderBase.headerSize);
    std::cout << std::format("headerVersion: {}\n", objHeaderBase.headerVersion);
    std::cout << std::format("objectSize: {} (0x{:x})\n", objHeaderBase.objectSize, objHeaderBase.objectSize);
    std::cout << std::format("objectType: {} (0x{:x})\n", objHeaderBase.objectType, objHeaderBase.objectType);
  }

  // TODO: handoff to object handler

  if (objHeaderBase.objectType == CONTAINER) {
    std::cout << "found a CONTAINER object\n";
  }

  BlfObjectHeader objHeader2;
  file.read(reinterpret_cast<char*>(&objHeader2), sizeof(BlfObjectHeaderBase));
  std::cout << std::format("objHeader2.flags: {} 0x{:04x}\n", objHeader2.flags, objHeader2.flags);
  std::cout << std::format("objHeader2.staticSize: {}\n", objHeader2.staticSize);
  std::cout << std::format("objHeader2.version: {}\n", objHeader2.version);
  std::cout << std::format("objHeader2.timestamp: {} 0x{:08x}\n", objHeader2.timestamp, objHeader2.timestamp);

  std::vector<char> compressed(objHeaderBase.objectSize - objHeaderBase.headerSize - sizeof(BlfObjectHeader));
  std::vector<char> decompressed(compressed.size()*5);

  file.read(compressed.data(), objHeaderBase.objectSize - objHeaderBase.headerSize - sizeof(BlfObjectHeader));

  size_t currentPos = file.tellg();
  std::cout << std::format("Current position: {} 0x{:x}\n", currentPos, currentPos);
  
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
    for (int i=0; i<0x49f; i++) {
      std::cout << std::format("{:02x} ", decompressed[i]);
    }
    std::cout << std::endl;
  }

  // [LOBJ|HS|HV|OSIZ|OTYP]
  // 4b    2b 2b 4b   4b
  // @0x90 LOBJ
  //  @0x98 sizeof LOBJ = 0x6b22
  // 0x6bb4 LOBJ
  //  @6bbc sizeof LOBJ =  0x71f2


  // TODO
  std::cout << std::endl;
  return;
  while (file.tellg() < fileSize) {
    BlfObjectHeaderBase objHeader;
    file.read(reinterpret_cast<char*>(&objHeader), sizeof(objHeader));

    if (!file) break;
    printf("1\n");

    if (std::string(objHeader.signature, 4) != "LOBJ") {
      // If we don't find LOBJ, we might be out of sync or at the end
      printf("Missing LOBJ signature. stop.\n");
      print_hex(reinterpret_cast<char*>(&objHeader), sizeof(objHeader));
      break;
    }
    printf("2\n");

    switch (objHeader.objectType) {
      case CAN_MESSAGE:
      case CAN_MESSAGE2: {
        std::cout << "Extracted CAN frame (Object Type: " << objHeader.objectType << ")" << std::endl;
        // In a real implementation, we would read the CANMessage struct here
        // and handle versioning/padding based on objHeader.headerSize
        file.seekg(objHeader.objectSize - sizeof(objHeader), std::ios::cur);
        break;
      }
      case ETHERNET_FRAME: {
        std::cout << "Extracted Ethernet frame (Object Type: " << objHeader.objectType << ")" << std::endl;
        file.seekg(objHeader.objectSize - sizeof(objHeader), std::ios::cur);
        break;
      }
      case CONTAINER: {
        // Containers often contain compressed objects. 
        // For this prototype, we skip them or log them as unknown if not handled.
        std::cout << "Found Container object (Object Type: " << objHeader.objectType << ")" << std::endl;
        file.seekg(objHeader.objectSize - sizeof(objHeader), std::ios::cur);
        break;
      }
      default: {
        std::cout << "Unknown object found (Object Type: " << objHeader.objectType << ")" << std::endl;
        file.seekg(objHeader.objectSize - sizeof(objHeader), std::ios::cur);
        break;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  std::string filename = argv[1];
  processFile(filename);

  return 0;
}
