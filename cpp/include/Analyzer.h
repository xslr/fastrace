#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "MappedFile.h"
#include "TraceMessage.h"

namespace fastrace {

struct ChunkEntry {
    size_t fileOffset; // byte offset of container in mmap
    uint64_t containerIndex; // 0-based container sequence number
    uint32_t skipMessages; // messages before chunk start inside the container
};

struct PerfCounters {
    uint64_t containers = 0;
    uint64_t compressedBytes = 0;
    uint64_t decompressedBytes = 0;
    uint64_t splitObjects = 0;
    int64_t pipelineUs = 0;
    size_t nThreads = 0;
    std::unordered_map<uint32_t, uint64_t> objectCounts;
};

std::string_view objectTypeName(uint32_t t);

class Analyzer {
public:
    bool skipDecompress = false;
    bool dumpObjContents = false;
    bool collectMessages = false;
    size_t maxMessages = 100'000;

    // ── Async loading support ────────────────────────────────────────────────
    /// Set to true by the UI thread to request early termination of processFile.
    std::atomic<bool> cancelled { false };
    /// Bytes consumed by the producer so far (updated each container push).
    std::atomic<size_t> bytesRead { 0 };
    /// Total file size in bytes; set once at the start of processFile.
    std::atomic<size_t> totalBytes { 0 };
    /// Messages appended to `messages` so far; incremented lock-free after
    /// push_back.
    std::atomic<size_t> messagesCollected { 0 };

    PerfCounters perf;
    std::vector<TraceMessage> messages;
    std::mutex messagesMu;

    static constexpr size_t CHUNK_SIZE = 10'000;

    size_t buildIndex(const std::string& filename);
    std::vector<TraceMessage> decodeChunk(size_t chunkIndex) const;
    size_t totalMessages() const noexcept { return totalMessages_; }

    void processFile(const std::string& filename);

private:
    std::vector<ChunkEntry> chunkIndex_;
    size_t totalMessages_ = 0;
    MappedFile mf_;
};

} // namespace fastrace
