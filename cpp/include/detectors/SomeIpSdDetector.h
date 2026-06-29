#pragma once
#include "Detector.h"
#include <unordered_map>

class SomeIpSdDetector : public Detector {
public:
    std::string name() const override { return "SomeIpSdDetector"; }
    bool isStateful() const override { return true; }
    void inspect(const ProtocolMessage& msg) override;
    void finalize() override;
    void reset() override;
    std::unique_ptr<Detector> clone() const override;
    void merge(const Detector& other) override;

private:
    std::unordered_map<uint16_t, uint16_t> m_lastSessionIds;
};
