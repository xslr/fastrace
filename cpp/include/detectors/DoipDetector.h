#pragma once
#include "ArxmlTypes.h"
#include "Detector.h"

class DoipDetector : public Detector {
public:
    explicit DoipDetector(const fastrace::ArDatabase* db = nullptr);
    std::string name() const override { return "DoipDetector"; }
    bool isStateful() const override { return true; }
    void inspect(const ProtocolMessage& msg) override;
    void finalize() override;
    void reset() override;
    std::unique_ptr<Detector> clone() const override;
    void merge(const Detector& other) override;

private:
    const fastrace::ArDatabase* m_db;
};
