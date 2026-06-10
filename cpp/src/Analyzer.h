#pragma once

#include <string>
#include <unordered_map>
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
  bool skipDecompress = false;
  bool dumpObjContents = false;

  PerfCounters perf;

  void processFile(const std::string& filename);
};

} // namespace fastrace
