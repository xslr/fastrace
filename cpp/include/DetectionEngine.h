#pragma once
#include "Analyzer.h"
#include "ArxmlTypes.h"
#include "Detection.h"
#include "Detector.h"
#include <atomic>
#include <memory>
#include <vector>

class DetectionEngine {
public:
    void addDetector(std::unique_ptr<Detector> d);
    void run(fastrace::Analyzer* analyzer, const fastrace::ArDatabase* db, std::atomic<bool>& cancelled,
        std::atomic<size_t>& chunksProcessed, size_t startChunkIndex = 0);

    const std::vector<Detection>& getResults() const { return m_results; }
    void clearResults() { m_results.clear(); }
    size_t chunkCount() const { return m_chunkCount; }

private:
    std::vector<std::unique_ptr<Detector>> m_detectors;
    std::vector<Detection> m_results;
    size_t m_chunkCount = 0;
};
