#include "Analyzer.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        return 1;
    }
    fastrace::Analyzer analyzer;
    size_t msgs = analyzer.buildIndex(argv[1]);

    auto index = analyzer.getChunkIndex();
    std::cout << "Index chunks: " << index.size() << std::endl;
    return 0;
}
