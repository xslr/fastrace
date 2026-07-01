import re

cpp_path = "ui/src/TimelineOverviewWidget.cpp"
with open(cpp_path, "r") as f:
    cpp = f.read()

# Add setDetections
if "void TimelineOverviewWidget::setDetections(" not in cpp:
    cpp += """
void TimelineOverviewWidget::setDetections(const std::vector<Detection>& detections)
{
    m_detections = detections;
    update();
}
"""

# Add the checkboxes
setup_ui_code = """
    m_chkAnomalies = new QCheckBox("Anomalies (Warn/Err)", this);
    m_chkAnomalies->setChecked(true);
    m_chkInfoAnomalies = new QCheckBox("Anomalies (Info)", this);
    m_chkInfoAnomalies->setChecked(false);

    topLayout->addWidget(lblHeader);
    topLayout->addSpacing(16);
    topLayout->addWidget(m_chkCan);
    topLayout->addWidget(m_chkEthernet);
    topLayout->addWidget(m_chkAnomalies);
    topLayout->addWidget(m_chkInfoAnomalies);
"""
if "m_chkAnomalies = new QCheckBox" not in cpp:
    cpp = re.sub(r"    topLayout->addWidget\(lblHeader\);\s+topLayout->addSpacing\(16\);\s+topLayout->addWidget\(m_chkCan\);\s+topLayout->addWidget\(m_chkEthernet\);", setup_ui_code, cpp)

if "m_chkAnomalies, &QCheckBox::toggled" not in cpp:
    cpp = cpp.replace("connect(m_chkEthernet, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);",
                      "connect(m_chkEthernet, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);\n    connect(m_chkAnomalies, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);\n    connect(m_chkInfoAnomalies, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);")

# Paint logic
anomaly_paint_code = """
    // Draw anomalies
    if (m_chkAnomalies->isChecked() || m_chkInfoAnomalies->isChecked()) {
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, yOffset, kLabelWidth - 5, laneHeight), Qt::AlignRight | Qt::AlignVCenter, "Anomalies");

        for (const auto& d : m_detections) {
            bool isInfo = (d.severity == Severity::Info);
            if (isInfo && !m_chkInfoAnomalies->isChecked()) continue;
            if (!isInfo && !m_chkAnomalies->isChecked()) continue;

            int x = timestampToX(d.timestampUs);
            if (x >= kLabelWidth && x < kLabelWidth + w) {
                QColor color;
                switch (d.severity) {
                    case Severity::Info: color = QColor(100, 100, 255, 180); break;
                    case Severity::Warning: color = QColor(255, 165, 0, 180); break; // Orange
                    case Severity::Error: color = QColor(255, 0, 0, 180); break; // Red
                }

                painter.setPen(Qt::NoPen);
                painter.setBrush(color);

                // Draw a small diamond or circle
                painter.drawEllipse(QPoint(x, yOffset + laneHeight / 2), 3, 3);
            }
        }
        yOffset += laneHeight + 1;
    }
"""

if "// Draw anomalies" not in cpp:
    cpp = re.sub(r"(drawLane\(static_cast<size_t>\(fastrace::ProtocolGroup::Ethernet\), \"ETH\", 0.45f\); // Teal\n\s+\})", r"\1" + anomaly_paint_code, cpp)

# Click logic
click_logic = """
    // Navigate to clicked position (outside the rectangle)
    const int w = width() - kLabelWidth;
    if (w <= 0) {
        return;
    }

    const int x = event->pos().x() - kLabelWidth;
    const int y = event->pos().y();
    if (x >= 0 && x < w) {
        // Check if we clicked on an anomaly
        if (m_chkAnomalies->isChecked() || m_chkInfoAnomalies->isChecked()) {
            int yOffset = kYOffset + 15;
            int laneHeight = 18;
            if (m_chkCan->isChecked()) yOffset += laneHeight + 1;
            if (m_chkEthernet->isChecked()) yOffset += laneHeight + 1;

            if (y >= yOffset && y <= yOffset + laneHeight) {
                // Find clicked anomaly
                int bestDist = 10;
                const Detection* bestD = nullptr;
                for (const auto& d : m_detections) {
                    bool isInfo = (d.severity == Severity::Info);
                    if (isInfo && !m_chkInfoAnomalies->isChecked()) continue;
                    if (!isInfo && !m_chkAnomalies->isChecked()) continue;

                    int dX = timestampToX(d.timestampUs) - kLabelWidth;
                    int dist = std::abs(dX - x);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestD = &d;
                    }
                }

                if (bestD) {
                    emit navigateRequested(bestD->timestampUs);
                    return;
                }
            }
        }

        const auto& hist = m_analyzer->histogram();
        uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;
"""

if "// Check if we clicked on an anomaly" not in cpp:
    cpp = re.sub(r"    // Navigate to clicked position \(outside the rectangle\)\n    const int w = width\(\) - kLabelWidth;\n    if \(w <= 0\) \{\n        return;\n    \}\n\n    const int x = event->pos\(\).x\(\) - kLabelWidth;\n    if \(x >= 0 && x < w\) \{\n        const auto& hist = m_analyzer->histogram\(\);\n        uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;", click_logic, cpp)

# Hover logic
hover_logic = """
    // Tooltip on hover
    const int w = width() - kLabelWidth;
    if (w <= 0) {
        return;
    }

    const int x = event->pos().x() - kLabelWidth;
    const int y = event->pos().y();

    if (x >= 0 && x < w) {
        int yOffset = kYOffset;
        int laneHeight = 18;

        // Check if we hover over anomaly
        if (m_chkAnomalies->isChecked() || m_chkInfoAnomalies->isChecked()) {
            int anomYOffset = yOffset + 15;
            if (m_chkCan->isChecked()) anomYOffset += laneHeight + 1;
            if (m_chkEthernet->isChecked()) anomYOffset += laneHeight + 1;

            if (y >= anomYOffset && y <= anomYOffset + laneHeight) {
                // Find hovered anomaly
                int bestDist = 10;
                const Detection* bestD = nullptr;
                for (const auto& d : m_detections) {
                    bool isInfo = (d.severity == Severity::Info);
                    if (isInfo && !m_chkInfoAnomalies->isChecked()) continue;
                    if (!isInfo && !m_chkAnomalies->isChecked()) continue;

                    int dX = timestampToX(d.timestampUs) - kLabelWidth;
                    int dist = std::abs(dX - x);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestD = &d;
                    }
                }
                if (bestD) {
                    QString severityStr;
                    switch (bestD->severity) {
                        case Severity::Info: severityStr = "Info"; break;
                        case Severity::Warning: severityStr = "Warning"; break;
                        case Severity::Error: severityStr = "Error"; break;
                    }
                    QToolTip::showText(event->globalPosition().toPoint(),
                        QString("[%1] %2: %3").arg(severityStr).arg(QString::fromStdString(bestD->detectorName)).arg(QString::fromStdString(bestD->message)), this);
                    return;
                }
            }
        }

        QString laneName;
"""
if "// Check if we hover over anomaly" not in cpp:
    cpp = re.sub(r"    // Tooltip on hover\n    const int w = width\(\) - kLabelWidth;\n    if \(w <= 0\) \{\n        return;\n    \}\n\n    const int x = event->pos\(\).x\(\) - kLabelWidth;\n    const int y = event->pos\(\).y\(\);\n\n    if \(x >= 0 && x < w\) \{\n        int yOffset = kYOffset;\n        int laneHeight = 18;\n\n        QString laneName;", hover_logic, cpp)


with open(cpp_path, "w") as f:
    f.write(cpp)
