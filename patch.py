import re

header_path = "ui/include/TimelineOverviewWidget.h"
with open(header_path, "r") as f:
    header = f.read()

# Add #include "Detection.h"
if "Detection.h" not in header:
    header = header.replace("#include <vector>", "#include <vector>\n#include \"Detection.h\"")

# Add setDetections
if "void setDetections(" not in header:
    header = header.replace("void attachAnalyzer(", "void setDetections(const std::vector<Detection>& detections);\n    void attachAnalyzer(")

# Add new members
if "m_chkAnomalies" not in header:
    header = header.replace("QCheckBox* m_chkEthernet = nullptr;", "QCheckBox* m_chkEthernet = nullptr;\n    QCheckBox* m_chkAnomalies = nullptr;\n    QCheckBox* m_chkInfoAnomalies = nullptr;\n    std::vector<Detection> m_detections;")

with open(header_path, "w") as f:
    f.write(header)
