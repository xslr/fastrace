#pragma once
#include "ArxmlTypes.h"
#include <string>
#include <vector>

namespace fastrace {

class ArxmlParser {
public:
    static ArDatabase parseFile(const std::string& path);
    static ArDatabase parseFiles(const std::vector<std::string>& paths);
};

} // namespace fastrace
