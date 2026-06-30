#include "detectors/PduDetector.h"

PduDetector::PduDetector(const fastrace::ArDatabase* db)
    : m_db(db)
{
}

void PduDetector::inspect(const ProtocolMessage& msg)
{
    if (msg.type != ProtocolType::Pdu) {
        return;
    }

    if (msg.data.pdu.length == 0) {
        emitDetection({ msg.timestampUs, name(), Severity::Error, "Zero length PDU",
            msg.sourceIndices.empty() ? 0 : msg.sourceIndices[0], { 0, 0 }, {} });
    }

    // DB Check (FR-018)
    if (m_db && !m_db->empty()) {
        bool found = false;
        for (const auto& arMsg : m_db->messages) {
            if (arMsg.busType == fastrace::ArBusType::ETHERNET && arMsg.pduId == msg.data.pdu.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            emitDetection({ msg.timestampUs, name(), Severity::Warning, "Unknown PDU ID",
                msg.sourceIndices.empty() ? 0 : msg.sourceIndices[0], { 0, 0 }, {} });
        }
    }

    // Track timestamps for finalize
    m_timestamps[msg.data.pdu.id].push_back(msg.timestampUs);
}

void PduDetector::finalize()
{
    // Basic timing anomaly check: just an example
    for (const auto& [id, times] : m_timestamps) {
        if (times.size() > 1000) {
            emitDetection({ times.back(), name(), Severity::Warning, "High frequency PDU detected", 0, { 0, 0 }, {} });
        }
    }
}

void PduDetector::reset() { m_timestamps.clear(); }

std::unique_ptr<Detector> PduDetector::clone() const { return std::make_unique<PduDetector>(m_db); }

void PduDetector::merge(const Detector& other)
{
    if (auto o = dynamic_cast<const PduDetector*>(&other)) {
        for (const auto& [id, times] : o->m_timestamps) {
            m_timestamps[id].insert(m_timestamps[id].end(), times.begin(), times.end());
        }
    }
}
