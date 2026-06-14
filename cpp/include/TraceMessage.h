#pragma once
#include <cstdint>

namespace fastrace {

struct TraceMessage {
    uint64_t timestampUs = 0; // microseconds from trace start
    uint32_t objectType = 0; // BLFObjectType value
    uint16_t channel = 0; // 1-based channel index
    uint32_t arbId = 0; // decoded CAN arbitration ID; 0 for Ethernet
    bool extendedId = false;
    uint8_t dlc = 0; // CAN DLC code (0-15); 0 for Ethernet
    uint8_t dataLen = 0; // actual byte count in data[] (0-64)
    uint8_t data[64] = {};
};

} // namespace fastrace
