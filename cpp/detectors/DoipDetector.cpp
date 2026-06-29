#include "detectors/DoipDetector.h"

DoipDetector::DoipDetector(const fastrace::ArDatabase* db)
    : m_db(db)
{
}

void DoipDetector::inspect(const ProtocolMessage& msg)
{
    if (msg.type != ProtocolType::DoIp) {
        return;
    }

    if (msg.data.doip.protocolVersion != 0x02 && msg.data.doip.protocolVersion != 0x03) {
        emitDetection({ msg.timestampUs, name(), Severity::Error, "Invalid DoIP Protocol Version",
            msg.sourceIndices.empty() ? 0 : msg.sourceIndices[0], { 0, 0 }, {} });
    }

    if (msg.data.doip.inverseVersion != (uint8_t)~msg.data.doip.protocolVersion) {
        emitDetection({ msg.timestampUs, name(), Severity::Warning, "DoIP Inverse Version mismatch",
            msg.sourceIndices.empty() ? 0 : msg.sourceIndices[0], { 0, 0 }, {} });
    }

    if (m_db && !m_db->ecus.empty() && msg.data.doip.payloadType == 0x8001) {
        // Mock FR-032: Check target address
    }
}

void DoipDetector::finalize() { }

void DoipDetector::reset() { }

std::unique_ptr<Detector> DoipDetector::clone() const { return std::make_unique<DoipDetector>(m_db); }

void DoipDetector::merge(const Detector& other) { }
