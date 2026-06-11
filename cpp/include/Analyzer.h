#pragma once

#include "TraceMessage.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string_view>

namespace fastrace {

struct PerfCounters {
  uint64_t containers        = 0;
  uint64_t compressedBytes   = 0;
  uint64_t decompressedBytes = 0;
  uint64_t splitObjects      = 0;
  int64_t  pipelineUs        = 0;
  size_t   nThreads          = 0;
  std::unordered_map<uint32_t, uint64_t> objectCounts;
};

std::string_view objectTypeName(uint32_t t);

class Analyzer {
public:
  bool   skipDecompress  = false;
  bool   dumpObjContents = false;
  bool   collectMessages = false;
  size_t maxMessages     = 100'000;

  PerfCounters perf;
  std::vector<TraceMessage> messages;
  std::mutex messagesMu;

  void processFile(const std::string& filename);
};

} // namespace fastrace
