#include "detectors/SomeIpSdDetector.h"

void SomeIpSdDetector::inspect(const ProtocolMessage& msg)
{
    if (msg.type != ProtocolType::SomeIpSd) {
        return;
    }

    // Simulate anomaly for demo
    if (msg.data.someIpSd.flags != 0xC0 && msg.data.someIpSd.flags != 0x80 && msg.data.someIpSd.flags != 0x00) {
        emitDetection({ msg.timestampUs, name(), Severity::Warning, "Unexpected SD flags",
            msg.sourceIndices.empty() ? 0 : msg.sourceIndices[0], { 0, 0 }, {} });
    }
}

void SomeIpSdDetector::finalize() { }

void SomeIpSdDetector::reset() { m_lastSessionIds.clear(); }

std::unique_ptr<Detector> SomeIpSdDetector::clone() const { return std::make_unique<SomeIpSdDetector>(); }

void SomeIpSdDetector::merge(const Detector& other)
{
    if (auto o = dynamic_cast<const SomeIpSdDetector*>(&other)) {
        for (const auto& [k, v] : o->m_lastSessionIds) {
            m_lastSessionIds[k] = v;
        }
    }
}
