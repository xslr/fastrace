#pragma once
#include "ArxmlTypes.h"
#include "Detector.h"
#include <unordered_map>
#include <vector>

class PduDetector : public Detector {
public:
    explicit PduDetector(const fastrace::ArDatabase* db = nullptr);
    std::string name() const override { return "PduDetector"; }
    bool isStateful() const override { return true; }
    void inspect(const ProtocolMessage& msg) override;
    void finalize() override;
    void reset() override;
    std::unique_ptr<Detector> clone() const override;
    void merge(const Detector& other) override;

private:
    const fastrace::ArDatabase* m_db;
    std::unordered_map<uint32_t, std::vector<uint64_t>> m_timestamps;
};
