#include "SignalDecoder.h"

namespace fastrace {

uint64_t extractSignalRaw(const uint8_t* data, uint8_t dataLen, uint32_t startBit, uint32_t bitLength, bool isBigEndian)
{
    if (bitLength == 0 || bitLength > 64) {
        return 0;
    }

    uint64_t rawValue = 0;

    if (!isBigEndian) {
        // Intel: startBit = LSB. Bits 0..bitLength-1 placed LSB-first.
        for (uint32_t i = 0; i < bitLength; ++i) {
            uint32_t bitPos = startBit + i;
            uint32_t byteIdx = bitPos / 8;
            uint32_t bitInByte = bitPos % 8;
            if (byteIdx < dataLen) {
                rawValue |= uint64_t((data[byteIdx] >> bitInByte) & 1u) << i;
            }
        }
    } else {
        // Motorola: startBit = MSB. i=0 is MSB → placed at bit (bitLength-1-i).
        uint32_t byteIdx = startBit / 8;
        uint32_t bitInByte = startBit % 8;
        for (uint32_t i = 0; i < bitLength; ++i) {
            if (byteIdx < dataLen) {
                rawValue |= uint64_t((data[byteIdx] >> bitInByte) & 1u) << (bitLength - 1u - i);
            }
            if (bitInByte == 0) {
                byteIdx += 1;
                bitInByte = 7;
            } else {
                bitInByte -= 1;
            }
        }
    }

    return rawValue;
}

std::vector<DecodedSignal> decodeAllSignals(const ArDatabase& db, const TraceMessage& msg)
{
    auto it = db.messageByCanId.find(msg.arbId);
    if (it == db.messageByCanId.end()) {
        return {};
    }
    const auto& arMsg = db.messages[it->second];
    std::vector<DecodedSignal> result;
    result.reserve(arMsg.signalDefs.size());
    for (const auto& sig : arMsg.signalDefs) {
        DecodedSignal ds;
        ds.name = sig.name;
        ds.bitLength = sig.bitLength;
        ds.startBit = sig.startBit;
        ds.isBigEndian = sig.isBigEndian;
        ds.rawValue = extractSignalRaw(msg.data, msg.dataLen, sig.startBit, sig.bitLength, sig.isBigEndian);
        result.push_back(std::move(ds));
    }
    return result;
}

} // namespace fastrace
