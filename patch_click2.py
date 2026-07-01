import re

cpp_path = "ui/src/TimelineOverviewWidget.cpp"
with open(cpp_path, "r") as f:
    cpp = f.read()

# Add window centering on click
click_logic = """
                if (bestD) {
                    uint64_t winWidth = m_visibleEndUs - m_visibleStartUs;
                    if (winWidth > 0 && m_analyzer) {
                        const auto& hist = m_analyzer->histogram();
                        uint64_t halfWin = winWidth / 2;
                        uint64_t newStart = (bestD->timestampUs > hist.traceStartUs + halfWin) ? bestD->timestampUs - halfWin : hist.traceStartUs;
                        uint64_t newEnd = newStart + winWidth;
                        if (newEnd > hist.traceEndUs) {
                            newEnd = hist.traceEndUs;
                            newStart = (newEnd > winWidth + hist.traceStartUs) ? newEnd - winWidth : hist.traceStartUs;
                        }
                        emit windowPanRequested(newStart, newEnd);
                    }

                    emit navigateRequested(bestD->timestampUs);
                    return;
                }
"""

cpp = re.sub(r"                if \(bestD\) \{\n                    emit navigateRequested\(bestD->timestampUs\);\n                    return;\n                \}", click_logic, cpp)

with open(cpp_path, "w") as f:
    f.write(cpp)
