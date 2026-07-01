import re

cpp_path = "ui/src/MainWindow.cpp"
with open(cpp_path, "r") as f:
    cpp = f.read()

# Add setDetections calls for TimelineOverviewWidget
if "m_timelineOverview->setDetections(" not in cpp:
    cpp = re.sub(r"    m_detections->setDetections\(\{\}\);", "    m_detections->setDetections({});\n    m_timelineOverview->setDetections({});", cpp)
    cpp = re.sub(r"    m_detections->setDetections\(m_detectionEngine->getResults\(\)\);", "    m_detections->setDetections(m_detectionEngine->getResults());\n    m_timelineOverview->setDetections(m_detectionEngine->getResults());", cpp)

with open(cpp_path, "w") as f:
    f.write(cpp)
