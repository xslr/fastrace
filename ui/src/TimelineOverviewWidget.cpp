/**
 * \file TimelineOverviewWidget.cpp
 * \brief Implementation of the full-trace density heatmap (mini-map) widget.
 *
 * \details TimelineOverviewWidget paints a bird's-eye density view of the
 * entire loaded trace.  Up to two horizontal lanes are drawn:
 *
 *  - **CAN lane** (blue hue)      — message count per time bin for all CAN frames.
 *  - **Ethernet lane** (teal hue) — message count per time bin for Ethernet frames.
 *
 * Each lane's colour intensity is normalised to its own per-bin peak so
 * that sparse and dense traces are rendered comparably.
 *
 * Async histogram model:
 *  restartHistogramJob() launches Analyzer::buildHistogram() via
 *  QtConcurrent::run(), then starts a 100 ms repaint timer to animate a
 *  thin progress bar.  resizeEvent() debounces (200 ms) before restarting
 *  the job so that interactive resizing does not spam background threads.
 *
 * Navigation:
 *  mousePressEvent() converts the click x-coordinate to a trace timestamp
 *  and emits navigateRequested() so the parent view can seek the message
 *  list to that position — unless the click lands inside the visible-window
 *  rectangle, which starts a drag pan operation instead.
 *
 * Visible-window overlay:
 *  setVisibleWindow() stores the current viewport bounds; the next
 *  paintEvent() draws a semi-transparent rectangle over the corresponding
 *  region of the heatmap.  The rectangle is draggable: pressing inside it
 *  and moving the mouse emits windowPanRequested(startUs, endUs) so the
 *  connected TimelineWidget can follow.
 */

#include "TimelineOverviewWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QToolTip>
#include <QtConcurrent>
#include <spdlog/spdlog.h>

#include "Analyzer.h"

static constexpr int kLabelWidth = 50;
static constexpr int kYOffset = 30; // Below checkbox row

TimelineOverviewWidget::TimelineOverviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
    setMouseTracking(true);
    setAutoFillBackground(true);
    setStyleSheet("TimelineOverviewWidget { background-color: white; color: black; } "
                  "QLabel { color: black; } "
                  "QCheckBox { color: black; }");

    auto* topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(16, 8, 16, 0);

    auto* lblHeader = new QLabel("OVERVIEW", this);
    lblHeader->setObjectName("headerLabel");

    m_chkCan = new QCheckBox("CAN", this);
    m_chkCan->setChecked(true);

    m_chkEthernet = new QCheckBox("Ethernet", this);
    m_chkEthernet->setChecked(true);

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

    topLayout->addStretch();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(topLayout);
    mainLayout->addStretch();

    connect(m_chkCan, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);
    connect(m_chkEthernet, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);
    connect(m_chkAnomalies, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);
    connect(m_chkInfoAnomalies, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(200);
    connect(&m_debounceTimer, &QTimer::timeout, this, &TimelineOverviewWidget::restartHistogramJob);

    m_repaintTimer.setInterval(100);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() { update(); });

    connect(&m_histogramWatcher, &QFutureWatcher<void>::finished, this, &TimelineOverviewWidget::onHistogramFinished);
}

void TimelineOverviewWidget::attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer)
{
    m_analyzer = std::move(analyzer);
    restartHistogramJob();
}

void TimelineOverviewWidget::setVisibleWindow(uint64_t startUs, uint64_t endUs)
{
    m_visibleStartUs = startUs;
    m_visibleEndUs = endUs;
    update();
}

void TimelineOverviewWidget::activate() { }

void TimelineOverviewWidget::deactivate() { }

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

uint64_t TimelineOverviewWidget::xToTimestamp(int x) const
{
    if (!m_analyzer) {
        return 0;
    }
    const auto& hist = m_analyzer->histogram();
    const uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;
    if (durationUs == 0) {
        return 0;
    }
    const int w = width() - kLabelWidth;
    if (w <= 0) {
        return 0;
    }
    const int plotX = x - kLabelWidth;
    if (plotX < 0 || plotX >= w) {
        return 0;
    }
    return hist.traceStartUs + static_cast<uint64_t>(plotX) * durationUs / static_cast<uint64_t>(w);
}

int TimelineOverviewWidget::timestampToX(uint64_t ts) const
{
    if (!m_analyzer) {
        return kLabelWidth;
    }
    const auto& hist = m_analyzer->histogram();
    const uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;
    if (durationUs == 0) {
        return kLabelWidth;
    }
    const int w = width() - kLabelWidth;
    return kLabelWidth + static_cast<int>(static_cast<uint64_t>(w) * (ts - hist.traceStartUs) / durationUs);
}

QRect TimelineOverviewWidget::visibleWindowRect() const
{
    if (!m_analyzer || m_visibleEndUs <= m_visibleStartUs) {
        return {};
    }
    const auto& hist = m_analyzer->histogram();
    const uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;
    if (durationUs == 0) {
        return {};
    }
    const int w = width() - kLabelWidth;

    int xStart = kLabelWidth
        + static_cast<int>(static_cast<uint64_t>(w) * (m_visibleStartUs - hist.traceStartUs) / durationUs);
    int xEnd
        = kLabelWidth + static_cast<int>(static_cast<uint64_t>(w) * (m_visibleEndUs - hist.traceStartUs) / durationUs);

    xStart = (std::max)(kLabelWidth, (std::min)(xStart, kLabelWidth + w));
    xEnd = (std::max)(kLabelWidth, (std::min)(xEnd, kLabelWidth + w));

    if (xEnd <= xStart) {
        return {};
    }
    return QRect(xStart, kYOffset, xEnd - xStart, height() - kYOffset);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void TimelineOverviewWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_analyzer) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int w = width() - kLabelWidth;
    if (w <= 0) {
        return;
    }

    int yOffset = kYOffset;

    // Fill background
    painter.fillRect(rect(), Qt::white);

    // Draw async histogram build progress
    if (m_histogramWatcher.isRunning()) {
        size_t totalChunks = m_analyzer->getChunkIndex().size();
        if (totalChunks > 0) {
            size_t processed = m_analyzer->histogramChunksProcessed.load(std::memory_order_relaxed);
            float progress = static_cast<float>(processed) / totalChunks;
            painter.fillRect(kLabelWidth, yOffset - 2, static_cast<int>(w * progress), 2, Qt::blue);
        }
    }

    const auto& hist = m_analyzer->histogram();
    uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;

    // Draw time axis
    painter.setPen(Qt::black);
    int numLabels = 6;
    for (int i = 0; i <= numLabels; ++i) {
        int x = kLabelWidth + (w * i) / numLabels;
        uint64_t t = hist.traceStartUs + (durationUs * i) / numLabels;
        QString timeStr = QString::number(static_cast<float>(t) / 1000000.0, 'f', 1) + "s";
        painter.drawText(x - 20, yOffset, 100, 15, Qt::AlignCenter, timeStr);
    }

    int laneHeight = 18;
    yOffset += 15;

    auto drawLane = [&](size_t groupIdx, const QString& name, float hue) {
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, yOffset, kLabelWidth - 5, laneHeight), Qt::AlignRight | Qt::AlignVCenter, name);

        uint32_t maxCount = 0;
        if (hist.bins[groupIdx].size() > 0) {
            for (int x = 0; x < (std::min<int>)(w, hist.bins[groupIdx].size() * 5); ++x) {
                size_t binIndex = x / 5;
                if (binIndex < hist.bins[groupIdx].size()) {
                    uint32_t count = hist.bins[groupIdx][binIndex];
                    if (count > maxCount) {
                        maxCount = count;
                    }
                }
            }
        }

        // Draw bins (including 0 count)
        for (int x = 0; x < w; ++x) {
            uint32_t count = 0;
            if (hist.bins[groupIdx].size() > 0) {
                size_t binIndex = x / 5;
                if (binIndex < hist.bins[groupIdx].size()) {
                    count = hist.bins[groupIdx][binIndex];
                }
            }
            const float intensity = maxCount > 0 ? static_cast<float>(count) / maxCount : 0.0f;
            const float lightness = 0.95f - intensity * 0.7f;
            painter.fillRect(kLabelWidth + x, yOffset, 1, laneHeight, QColor::fromHslF(hue, 0.7f, lightness));
        }
        yOffset += laneHeight + 1;
    };

    if (m_chkCan->isChecked()) {
        drawLane(static_cast<size_t>(fastrace::ProtocolGroup::CAN), "CAN", 0.6f); // Blue
    }
    if (m_chkEthernet->isChecked()) {
        drawLane(static_cast<size_t>(fastrace::ProtocolGroup::Ethernet), "ETH", 0.45f); // Teal
    }
    // Draw anomalies
    if (m_chkAnomalies->isChecked() || m_chkInfoAnomalies->isChecked()) {
        painter.setPen(Qt::black);
        painter.drawText(
            QRect(0, yOffset, kLabelWidth - 5, laneHeight), Qt::AlignRight | Qt::AlignVCenter, "Anomalies");

        for (const auto& d : m_detections) {
            bool isInfo = (d.severity == Severity::Info);
            if (isInfo && !m_chkInfoAnomalies->isChecked()) {
                continue;
            }
            if (!isInfo && !m_chkAnomalies->isChecked()) {
                continue;
            }

            int x = timestampToX(d.timestampUs);
            if (x >= kLabelWidth && x < kLabelWidth + w) {
                QColor color;
                switch (d.severity) {
                case Severity::Info:
                    color = QColor(100, 100, 255, 180);
                    break;
                case Severity::Warning:
                    color = QColor(255, 165, 0, 180);
                    break; // Orange
                case Severity::Error:
                    color = QColor(255, 0, 0, 180);
                    break; // Red
                }

                painter.setPen(Qt::NoPen);
                painter.setBrush(color);

                // Draw a small diamond or circle
                painter.drawEllipse(QPoint(x, yOffset + laneHeight / 2), 3, 3);
            }
        }
        yOffset += laneHeight + 1;
    }

    // Draw visible window overlay rectangle
    const QRect winRect = visibleWindowRect();
    if (!winRect.isEmpty()) {
        // Semi-transparent fill
        painter.setBrush(QColor(255, 255, 255, 60));
        painter.setPen(QColor(80, 160, 255, 200));
        painter.drawRect(winRect);

        // Subtle drag handle hint: slightly darker border on left/right edges
        painter.setPen(QColor(80, 160, 255, 230));
        painter.drawLine(winRect.left(), winRect.top(), winRect.left(), winRect.bottom());
        painter.drawLine(winRect.right(), winRect.top(), winRect.right(), winRect.bottom());
    }
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void TimelineOverviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_debounceTimer.start();
}

void TimelineOverviewWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    if (!m_analyzer) {
        return;
    }

    const QRect winRect = visibleWindowRect();
    if (!winRect.isEmpty() && winRect.contains(event->pos())) {
        // Start drag-pan inside the visible-window rectangle
        m_dragging = true;
        m_dragStartX = event->pos().x();
        m_dragWinStartUs = m_visibleStartUs;
        m_dragWinEndUs = m_visibleEndUs;
        setCursor(Qt::SizeHorCursor);
        return;
    }

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
            if (m_chkCan->isChecked()) {
                yOffset += laneHeight + 1;
            }
            if (m_chkEthernet->isChecked()) {
                yOffset += laneHeight + 1;
            }

            if (y >= yOffset && y <= yOffset + laneHeight) {
                // Find clicked anomaly
                int bestDist = 10;
                const Detection* bestD = nullptr;
                for (const auto& d : m_detections) {
                    bool isInfo = (d.severity == Severity::Info);
                    if (isInfo && !m_chkInfoAnomalies->isChecked()) {
                        continue;
                    }
                    if (!isInfo && !m_chkAnomalies->isChecked()) {
                        continue;
                    }

                    int dX = timestampToX(d.timestampUs) - kLabelWidth;
                    int dist = std::abs(dX - x);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestD = &d;
                    }
                }

                if (bestD) {
                    uint64_t winWidth = m_visibleEndUs - m_visibleStartUs;
                    if (winWidth > 0 && m_analyzer) {
                        const auto& hist = m_analyzer->histogram();
                        uint64_t halfWin = winWidth / 2;
                        uint64_t newStart = (bestD->timestampUs > hist.traceStartUs + halfWin)
                            ? bestD->timestampUs - halfWin
                            : hist.traceStartUs;
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
            }
        }

        const auto& hist = m_analyzer->histogram();
        uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;

        uint64_t ts = hist.traceStartUs + (durationUs * x) / w;
        emit navigateRequested(ts);
    }
}

void TimelineOverviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
    if (!m_analyzer) {
        return;
    }

    if (m_dragging) {
        const auto& hist = m_analyzer->histogram();
        const uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;
        const int w = width() - kLabelWidth;
        if (w <= 0 || durationUs == 0) {
            return;
        }

        const int dx = event->pos().x() - m_dragStartX;
        // Convert pixel delta to timestamp delta
        const int64_t deltaUs = static_cast<int64_t>(dx) * static_cast<int64_t>(durationUs) / static_cast<int64_t>(w);

        int64_t newStart = static_cast<int64_t>(m_dragWinStartUs) + deltaUs;
        int64_t newEnd = static_cast<int64_t>(m_dragWinEndUs) + deltaUs;
        const int64_t winWidth = static_cast<int64_t>(m_dragWinEndUs - m_dragWinStartUs);

        // Clamp so the window stays within the trace
        if (newStart < static_cast<int64_t>(hist.traceStartUs)) {
            newStart = static_cast<int64_t>(hist.traceStartUs);
            newEnd = newStart + winWidth;
        }
        if (newEnd > static_cast<int64_t>(hist.traceEndUs)) {
            newEnd = static_cast<int64_t>(hist.traceEndUs);
            newStart = newEnd - winWidth;
        }

        m_visibleStartUs = static_cast<uint64_t>(newStart);
        m_visibleEndUs = static_cast<uint64_t>(newEnd);
        update();
        emit windowPanRequested(m_visibleStartUs, m_visibleEndUs);
        return;
    }

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
            if (m_chkCan->isChecked()) {
                anomYOffset += laneHeight + 1;
            }
            if (m_chkEthernet->isChecked()) {
                anomYOffset += laneHeight + 1;
            }

            if (y >= anomYOffset && y <= anomYOffset + laneHeight) {
                // Find hovered anomaly
                int bestDist = 10;
                const Detection* bestD = nullptr;
                for (const auto& d : m_detections) {
                    bool isInfo = (d.severity == Severity::Info);
                    if (isInfo && !m_chkInfoAnomalies->isChecked()) {
                        continue;
                    }
                    if (!isInfo && !m_chkAnomalies->isChecked()) {
                        continue;
                    }

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
                    case Severity::Info:
                        severityStr = "Info";
                        break;
                    case Severity::Warning:
                        severityStr = "Warning";
                        break;
                    case Severity::Error:
                        severityStr = "Error";
                        break;
                    }
                    QToolTip::showText(event->globalPosition().toPoint(),
                        QString("[%1] %2: %3")
                            .arg(severityStr)
                            .arg(QString::fromStdString(bestD->detectorName))
                            .arg(QString::fromStdString(bestD->message)),
                        this);
                    return;
                }
            }
        }

        QString laneName;

        uint32_t count = 0;
        const auto& hist = m_analyzer->histogram();

        if (m_chkCan->isChecked() && y >= yOffset && y < yOffset + laneHeight) {
            laneName = "CAN";
            size_t binIndex = x / 5;
            const auto& vec = hist.bins[static_cast<size_t>(fastrace::ProtocolGroup::CAN)];
            if (binIndex < vec.size()) {
                count = vec[binIndex];
            }
        }
        yOffset += m_chkCan->isChecked() ? (laneHeight + 1) : 0;

        if (m_chkEthernet->isChecked() && y >= yOffset && y < yOffset + laneHeight) {
            laneName = "Ethernet";
            size_t binIndex = x / 5;
            const auto& vec = hist.bins[static_cast<size_t>(fastrace::ProtocolGroup::Ethernet)];
            if (binIndex < vec.size()) {
                count = vec[binIndex];
            }
        }

        if (!laneName.isEmpty()) {
            uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;
            uint64_t ts = hist.traceStartUs + (durationUs * x) / w;
            QString timeStr = QString::number(ts / 1000000.0, 'f', 3) + "s";

            QToolTip::showText(event->globalPosition().toPoint(),
                QString("%1: %2 msgs @ %3").arg(laneName).arg(count).arg(timeStr), this);
            return;
        }
    }
    QToolTip::hideText();
}

void TimelineOverviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    if (m_dragging) {
        m_dragging = false;
        unsetCursor();
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void TimelineOverviewWidget::onLaneToggled() { update(); }

void TimelineOverviewWidget::onHistogramFinished()
{
    m_repaintTimer.stop();
    update();
}

void TimelineOverviewWidget::restartHistogramJob()
{
    if (!m_analyzer) {
        return;
    }

    if (m_histogramWatcher.isRunning()) {
        m_analyzer->histogramCancelled.store(true, std::memory_order_relaxed);
        m_histogramWatcher.waitForFinished();
    }
    m_analyzer->histogramCancelled.store(false, std::memory_order_relaxed);

    const int w = width() - kLabelWidth;
    if (w <= 0) {
        return;
    }
    int numBins = w / 5;
    if (numBins <= 0) {
        numBins = 1;
    }

    auto future = QtConcurrent::run([analyzer = m_analyzer, numBins]() { analyzer->buildHistogram(numBins); });

    m_histogramWatcher.setFuture(future);
    m_repaintTimer.start();
    update();
}

void TimelineOverviewWidget::setDetections(const std::vector<Detection>& detections)
{
    m_detections = detections;
    update();
}
