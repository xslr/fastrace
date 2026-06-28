#include "Analyzer.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        return 1;
    }
    fastrace::Analyzer analyzer;
    size_t msgs = analyzer.buildIndex(argv[1]);

    std::cout << "Total index messages: " << msgs << std::endl;

    // Simulate what decodeChunk does for all chunks, and sum the messages!
    size_t totalDecoded = 0;
    for (size_t i = 0; i <= 400; ++i) {
        auto chunkMsgs = analyzer.decodeChunk(i);
        totalDecoded += chunkMsgs.size();
        if (i == 400) {
            std::cout << "Chunk 400 yielded: " << chunkMsgs.size() << std::endl;
        }
    }

    std::cout << "Total decoded across all chunks: " << totalDecoded << std::endl;
    std::cout << "Difference: " << msgs - totalDecoded << std::endl;

    return 0;
}
