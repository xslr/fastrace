#pragma once
#include "ArxmlTypes.h"
#include "TraceMessage.h"
#include <cstdint>
#include <string>
#include <vector>

namespace fastrace {

struct DecodedSignal {
    std::string name;
    uint64_t rawValue = 0;
    uint32_t bitLength = 0;
    uint32_t startBit = 0;
    bool isBigEndian = false;
};

// Extract raw unsigned value from CAN data bytes.
//
// Intel (isBigEndian=false): startBit = LSB bit position.
// Motorola (isBigEndian=true): startBit = MSB bit position.
// Returns 0 if startBit/bitLength exceed dataLen.
uint64_t extractSignalRaw(const uint8_t* data, size_t dataLen, uint32_t startBit, uint32_t bitLength, bool isBigEndian);

// Look up ArMessage for msg.arbId in db, decode all its signals.
// Returns empty vector if no matching message or no signal definitions.
std::vector<DecodedSignal> decodeAllSignals(const ArDatabase& db, const TraceMessage& msg);

} // namespace fastrace
