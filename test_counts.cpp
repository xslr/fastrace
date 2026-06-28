#include "Analyzer.h"
#include <iostream>

using namespace fastrace;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        return 1;
    }
    Analyzer analyzer;
    size_t count = analyzer.buildIndex(argv[1]);
    std::cout << "buildIndex returned " << count << "\n";
    return 0;
}
