#include "DetectionEngine.h"
#include "ProtocolParser.h"
#include <algorithm>
#include <mutex>
#include <thread>

void DetectionEngine::addDetector(std::unique_ptr<Detector> d) { m_detectors.push_back(std::move(d)); }

void DetectionEngine::run(fastrace::Analyzer* analyzer, const fastrace::ArDatabase* db, std::atomic<bool>& cancelled,
    std::atomic<size_t>& chunksProcessed, size_t startChunkIndex)
{
    if (startChunkIndex == 0) {
        clearResults();
    }
    if (!analyzer) {
        return;
    }

    size_t totalChunks = analyzer->getChunkIndex().size();
    if (startChunkIndex >= totalChunks) {
        return;
    }
    m_chunkCount = totalChunks;

    std::vector<std::unique_ptr<Detector>> statelessDetectors;
    std::vector<std::unique_ptr<Detector>> statefulDetectors;

    for (auto& d : m_detectors) {
        if (startChunkIndex == 0) {
            d->reset();
        }
        if (d->isStateful()) {
            statefulDetectors.push_back(d->clone());
        } else {
            statelessDetectors.push_back(d->clone());
        }
    }

    std::mutex resultsMu;
    std::vector<std::vector<ProtocolMessage>> allChunkMessages(totalChunks - startChunkIndex);

    auto processChunk = [&](size_t chunkIdx) {
        if (cancelled.load(std::memory_order_relaxed)) {
            return;
        }

        ProtocolParser parser(db);
        auto msgs = analyzer->decodeChunk(chunkIdx);

        std::vector<Detection> localDetections;
        std::vector<ProtocolMessage> parsedMessages;

        size_t globalBase = chunkIdx * fastrace::Analyzer::CHUNK_SIZE;

        for (size_t i = 0; i < msgs.size(); ++i) {
            auto chunkMsgs = parser.parse(msgs[i], globalBase + i);
            parsedMessages.insert(parsedMessages.end(), std::make_move_iterator(chunkMsgs.begin()),
                std::make_move_iterator(chunkMsgs.end()));
        }

        for (auto& pm : parsedMessages) {
            for (auto& sd : statelessDetectors) {
                sd->inspect(pm);
            }
        }

        for (auto& sd : statelessDetectors) {
            auto d = sd->takeDetections();
            localDetections.insert(
                localDetections.end(), std::make_move_iterator(d.begin()), std::make_move_iterator(d.end()));
        }

        auto parserDetections = parser.takeDetections();
        localDetections.insert(localDetections.end(), std::make_move_iterator(parserDetections.begin()),
            std::make_move_iterator(parserDetections.end()));

        if (!localDetections.empty()) {
            std::lock_guard<std::mutex> lock(resultsMu);
            m_results.insert(m_results.end(), std::make_move_iterator(localDetections.begin()),
                std::make_move_iterator(localDetections.end()));
        }

        allChunkMessages[chunkIdx - startChunkIndex] = std::move(parsedMessages);
        chunksProcessed.fetch_add(1, std::memory_order_relaxed);
    };

    std::vector<std::thread> workers;
    size_t numThreads = std::min<size_t>(std::thread::hardware_concurrency(), 8);
    std::atomic<size_t> currentChunk { startChunkIndex };

    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([&]() {
            while (true) {
                size_t chunkIdx = currentChunk.fetch_add(1, std::memory_order_relaxed);
                if (chunkIdx >= totalChunks || cancelled.load(std::memory_order_relaxed)) {
                    break;
                }
                processChunk(chunkIdx);
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    if (cancelled.load(std::memory_order_relaxed)) {
        return;
    }

    for (size_t chunkIdx = startChunkIndex; chunkIdx < totalChunks; ++chunkIdx) {
        for (auto& pm : allChunkMessages[chunkIdx - startChunkIndex]) {
            for (auto& sd : statefulDetectors) {
                sd->inspect(pm);
            }
        }
    }

    for (auto& sd : statefulDetectors) {
        // Only finalize if we are processing all the way to the end, but wait, finalize emits missing alive checks!
        // In continuous mode we should probably NOT finalize until the stream stops, or just not emit in finalize.
        // We will finalize every run for now, but in reality we shouldn't unless trace is complete.
        sd->finalize();
        auto d = sd->takeDetections();
        m_results.insert(m_results.end(), std::make_move_iterator(d.begin()), std::make_move_iterator(d.end()));

        // Write state back to master detectors!
        // We need to merge them so next run() preserves state.
    }

    for (size_t i = 0; i < statefulDetectors.size(); ++i) {
        // Find the matching detector in m_detectors
        for (auto& md : m_detectors) {
            if (md->isStateful() && md->name() == statefulDetectors[i]->name()) {
                // Not ideal but works for now
                md->merge(*statefulDetectors[i]);
            }
        }
    }

    std::sort(m_results.begin(), m_results.end(),
        [](const Detection& a, const Detection& b) { return a.timestampUs < b.timestampUs; });
}
