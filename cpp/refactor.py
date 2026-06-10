import re
import sys

with open('src/Analyzer.cpp', 'r') as f:
    code = f.read()

# Remove benchmarkRawRead and everything after it
idx = code.find('void benchmarkRawRead(const std::string& filename) {')
if idx != -1:
    code = code[:idx]

# Add includes and namespace
code = '#include "Analyzer.h"\n\nnamespace fastrace {\n\n' + code + '\n} // namespace fastrace\n'

# Remove PerfCounters struct and g_perf
code = re.sub(r'struct PerfCounters \{[^}]+\};\nstatic PerfCounters g_perf;\n', '', code, flags=re.MULTILINE)

# Remove bool dumpObjContents = false;
code = code.replace('bool dumpObjContents = false;', '')

# Change objectTypeName to return string_view instead of static std::string_view
code = code.replace('static std::string_view objectTypeName(uint32_t t) {', 'std::string_view objectTypeName(uint32_t t) {')

# processInnerObjects: add self parameter and change dumpObjContents to self->dumpObjContents
code = code.replace(
    'static void processInnerObjects(const char* data, size_t dataLen,',
    'static void processInnerObjects(Analyzer* self, const char* data, size_t dataLen,'
)
code = code.replace('if (dumpObjContents)', 'if (self->dumpObjContents)')

# runStitcher: add self parameter and pass to processInnerObjects
code = code.replace(
    'static void runStitcher(FragmentQueue& fragQ,',
    'static void runStitcher(Analyzer* self, FragmentQueue& fragQ,'
)
code = code.replace(
    'processInnerObjects(localBuf.data(), localBuf.size(), localCounts, localSplits);',
    'processInnerObjects(self, localBuf.data(), localBuf.size(), localCounts, localSplits);'
)

# runConsumer: add self parameter and pass to processInnerObjects
code = code.replace(
    'static void runConsumer(WorkQueue& queue,',
    'static void runConsumer(Analyzer* self, WorkQueue& queue,'
)
code = code.replace(
    'skipDecompress)',
    ')'
)
code = code.replace(
    'if (skipDecompress) continue;',
    'if (self->skipDecompress) continue;'
)
code = code.replace(
    'processInnerObjects(localBuf.data(), actualOut, localCounts, localSplits,\n                          &headFrag, &tailFrag);',
    'processInnerObjects(self, localBuf.data(), actualOut, localCounts, localSplits,\n                          &headFrag, &tailFrag);'
)

# runPipeline: change to runPipeline(Analyzer* self, ...)
code = code.replace(
    'static void runPipeline(Cursor cursor, const BlfFileHeader& hdr,\n                        size_t nWorkers, bool skipDecompress) {',
    'static void runPipeline(Analyzer* self, Cursor cursor, const BlfFileHeader& hdr,\n                        size_t nWorkers) {'
)
code = code.replace(
    'std::thread stitcher(runStitcher, std::ref(fragQ)',
    'std::thread stitcher(runStitcher, self, std::ref(fragQ)'
)
code = code.replace(
    'consumers.emplace_back(runConsumer, std::ref(queue),',
    'consumers.emplace_back(runConsumer, self, std::ref(queue),'
)
code = code.replace(
    'skipDecompress);\n  }',
    ');\n  }'
)
code = code.replace('g_perf.containers', 'self->perf.containers')
code = code.replace('g_perf.compressedBytes', 'self->perf.compressedBytes')
code = code.replace('g_perf.decompressedBytes', 'self->perf.decompressedBytes')
code = code.replace('g_perf.objectCounts', 'self->perf.objectCounts')
code = code.replace('g_perf.splitObjects', 'self->perf.splitObjects')
code = code.replace('g_perf.nThreads', 'self->perf.nThreads')
code = code.replace('g_perf.pipelineUs', 'self->perf.pipelineUs')

# processFile: change to Analyzer::processFile
code = code.replace(
    'void processFile(const std::string& filename, bool skipDecompress) {',
    'void Analyzer::processFile(const std::string& filename) {'
)
code = code.replace(
    'runPipeline(cursor, *fileHeader, nWorkers, skipDecompress);',
    'runPipeline(this, cursor, *fileHeader, nWorkers);'
)

# Replace the runPipeline definition at the top of the file if needed.
# wait, runPipeline and other static functions are forward declared? No, they are just defined in order.
# processInnerObjects is declared at the top?
code = code.replace(
    'static void processInnerObjects(const char* data, size_t dataLen,',
    'static void processInnerObjects(Analyzer* self, const char* data, size_t dataLen,'
)

with open('src/Analyzer.cpp', 'w') as f:
    f.write(code)
