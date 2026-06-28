#include "Analyzer.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        return 1;
    }
    fastrace::Analyzer analyzer;
    size_t msgs = analyzer.buildIndex(argv[1]);
    std::cout << "buildIndex detected: " << msgs << " messages" << std::endl;
    auto chunkMsgs = analyzer.decodeChunk(400);
    std::cout << "chunk 400 has: " << chunkMsgs.size() << " messages" << std::endl;
    return 0;
}
