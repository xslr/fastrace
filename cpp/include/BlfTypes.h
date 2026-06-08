#pragma once
#include <cstdint>

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

// CAN_MESSAGE (type 1) and CAN_MESSAGE2 (type 86) share the same payload layout.
// Wire order: channel, dlc, flags, id, data[dlc]  (only up to 8 bytes for classic CAN).
struct CANMessage {
  uint16_t channel;  // 1-based channel index
  uint8_t  dlc;      // data length code (0-8)
  uint8_t  flags;    // TX=0x04, remote frame=0x10, etc.
  uint32_t id;       // arbitration id; bit 31 set → extended (29-bit)
  uint8_t  data[8];  // payload (only dlc bytes are valid)
};
using CANMessage2 = CANMessage;  // type 86 shares the same layout

// ETHERNET_FRAME (type 120) fixed header; variable payload follows immediately.
struct EthernetFrameHeader {
  uint8_t  srcAddr[6];    // source MAC
  uint16_t channel;       // logical channel index
  uint8_t  dstAddr[6];    // destination MAC
  uint16_t direction;     // 0=Rx, 1=Tx, 2=Tx-request
  uint16_t ethType;       // EtherType (e.g. 0x0800=IPv4, 0x8100=VLAN)
  uint16_t tpid;          // VLAN tag protocol identifier
  uint16_t tci;           // VLAN tag control information
  uint16_t payloadLength; // byte count of the payload that follows this header
  uint64_t res;           // reserved / padding
  // payloadLength bytes of Ethernet payload follow immediately
};
#pragma pack(pop)

// All known BLF object type IDs (sourced from vector_blf / python-can / Wireshark blf.h).
// Values are assigned by Vector Informatik and are stable across tool versions.
enum BLFObjectType : uint32_t {
  CAN_MESSAGE                   = 1,
  CAN_ERROR                     = 2,
  CAN_OVERLOAD                  = 3,
  CAN_STATISTIC                 = 4,
  APP_TEXT                      = 5,
  CAN_REMOTE_FRAME              = 6,
  LIN_MESSAGE                   = 9,
  CONTAINER                     = 10,
  LIN_RX_ERROR                  = 11,
  LIN_SEND_ERROR                = 12,
  LIN_SLAVE_TIMEOUT             = 13,
  LIN_NOANS                     = 14,
  LIN_WAKEUP                    = 15,
  LIN_SPIKE                     = 16,
  LIN_DLCINFO                   = 17,
  LIN_RCV_ERROR                 = 18,
  LIN_SYNCERROR                 = 19,
  LIN_BAUDRATE                  = 20,
  LIN_SLEEP                     = 21,
  LIN_WAKEUP2                   = 22,
  MOST_SPY                      = 23,
  MOST_CTRL                     = 24,
  MOST_LIGHTLOCK                = 25,
  MOST_STATISTIC                = 26,
  FLEXRAY_DATA                  = 29,
  FLEXRAY_SYNC                  = 30,
  CAN_DRIVER_ERROR              = 31,
  MOST_PKT                      = 32,
  MOST_PKT2                     = 33,
  MOST_HWMODE                   = 34,
  MOST_REG                      = 35,
  MOST_GENREG                   = 36,
  MOST_NETSTATE                 = 37,
  MOST_DATALOST                 = 38,
  MOST_TRIGGER                  = 39,
  FLEXRAY_CYCLE                 = 40,
  FLEXRAY_MESSAGE               = 41,
  LIN_CHECKSUM_INFO             = 42,
  LIN_SPIKE_IGNORE              = 43,
  LIN_WAKEUP_INFO               = 44,
  LIN_IN_PROGRESS               = 45,
  LIN_UNEXPECTED_WAKEUP         = 46,
  LIN_SHORT_OR_SLOW_RESPONSE    = 47,
  LIN_DISTURBANCE_EVENT         = 48,
  SERIAL_EVENT                  = 49,
  OVERRUN_ERROR                 = 50,
  EVENT_COMMENT                 = 51,
  WLAN_FRAME                    = 52,
  WLAN_STATISTIC                = 53,
  MOST_ECL                      = 54,
  SYS_VARIABLE                  = 72,
  CAN_ERROR_EXT                 = 73,
  CAN_DRIVER_ERROR_EXT          = 74,
  LIN_LONG_DOM_SIG              = 75,
  MOST_150_MESSAGE              = 76,
  MOST_150_PKT                  = 77,
  MOST_ETHERNET_PKT             = 78,
  MOST_150_MESSAGE_FRAGMENT     = 79,
  MOST_150_PKT_FRAGMENT         = 80,
  MOST_ETHERNET_PKT_FRAGMENT    = 81,
  MOST_SYSTEM_EVENT             = 82,
  MOST_150_ALLOCTAB             = 83,
  MOST_50_MESSAGE               = 84,
  MOST_50_PKT                   = 85,
  CAN_MESSAGE2                  = 86,
  LIN_UNEXPECTED_WAKEUP2        = 87,
  LIN_SHORT_OR_SLOW_RESPONSE3   = 88,
  LIN_DISTURBANCE_EVENT2        = 89,
  APP_TRIGGER                   = 90,
  ENV_INTEGER                   = 91,
  ENV_DOUBLE                    = 92,
  ENV_STRING                    = 93,
  ENV_DATA                      = 94,
  GRAPHICS_OBJECT               = 95,
  GLOBAL_MARKER                 = 96,
  AFDX_FRAME                    = 97,
  AFDX_STATISTIC                = 98,
  KLINE_STATUSEVENT             = 99,
  CAN_FD_MESSAGE                = 100,
  CAN_FD_MESSAGE_64             = 101,
  ETHERNET_RX_ERROR             = 102,
  ETHERNET_STATUS               = 103,
  CAN_FD_ERROR_64               = 104,
  LIN_SHORT_OR_SLOW_RESPONSE2   = 105,
  AFDX_STATUS                   = 106,
  AFDX_BUS_STATISTIC            = 107,
  AFDX_ERROR_EVENT              = 109,
  A429_ERROR                    = 110,
  A429_STATUS                   = 111,
  A429_BUS_STATISTIC            = 112,
  A429_MESSAGE                  = 113,
  ETHERNET_STATISTIC            = 114,
  RESERVED_1                    = 115,
  RESERVED_2                    = 116,
  RESERVED_3                    = 117,
  TEST_STRUCTURE                = 118,
  DIAG_REQUEST_INTERPRETATION   = 119,
  ETHERNET_FRAME                = 120,
  ETHERNET_FRAME_EX             = 121,
  ETHERNET_ERROR_EX             = 122,
  ETHERNET_ERROR_FORWARDED      = 123,
  FUNC_BUS                      = 124,
  DATA_LOST_BEGIN               = 125,
  DATA_LOST_END                 = 126,
  WATER_MARK_EVENT              = 127,
  TRIGGER_CONDITION             = 128,
  CAN_SETTING_CHANGED           = 129,
  DISTRIBUTED_OBJECT            = 130,
};
