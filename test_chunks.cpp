#include "Analyzer.h"
#include <iostream>
#include <vector>

int main(int argc, char* argv[])
{
    fastrace::Analyzer analyzer;
    analyzer.buildIndex(argv[1]);

    auto index = analyzer.getChunkIndex();

    size_t total = 0;
    for (size_t i = 0; i < index.size(); ++i) {
        auto msgs = analyzer.decodeChunk(i);
        total += msgs.size();
        if (msgs.size() != 10000 && i != index.size() - 1) {
            std::cout << "Chunk " << i << " returned " << msgs.size() << "\n";
        }
    }
    std::cout << "Total: " << total << "\n";
    return 0;
}
