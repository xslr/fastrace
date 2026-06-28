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
    return 0;
}
