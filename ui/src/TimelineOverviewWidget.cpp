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

#include "Analyzer.h"

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

    topLayout->addWidget(lblHeader);
    topLayout->addSpacing(16);
    topLayout->addWidget(m_chkCan);
    topLayout->addWidget(m_chkEthernet);
    topLayout->addStretch();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(topLayout);
    mainLayout->addStretch();

    connect(m_chkCan, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);
    connect(m_chkEthernet, &QCheckBox::toggled, this, &TimelineOverviewWidget::onLaneToggled);

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

void TimelineOverviewWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_analyzer) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int labelWidth = 50;
    int w = width() - labelWidth;
    if (w <= 0) {
        return;
    }

    int yOffset = 30; // Below checkbox row

    // Fill background
    painter.fillRect(rect(), Qt::white);

    const auto& hist = m_analyzer->histogram();
    uint64_t durationUs = hist.traceEndUs - hist.traceStartUs;

    // Draw time axis
    painter.setPen(Qt::black);
    int numLabels = 6;
    for (int i = 0; i <= numLabels; ++i) {
        int x = labelWidth + (w * i) / numLabels;
        uint64_t t = hist.traceStartUs + (durationUs * i) / numLabels;
        QString timeStr = QString::number(static_cast<float>(t) / 1000000.0, 'f', 1) + "s";
        painter.drawText(x - 20, yOffset, 100, 15, Qt::AlignCenter, timeStr);
    }

    int laneHeight = 18;
    yOffset += 15;

    auto drawLane = [&](size_t groupIdx, const QString& name, float hue) {
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, yOffset, labelWidth - 5, laneHeight), Qt::AlignRight | Qt::AlignVCenter, name);

        uint32_t maxCount = 0;
        if (hist.bins[groupIdx].size() > 0) {
            for (int x = 0; x < std::min<int>(w, hist.bins[groupIdx].size() * 5); ++x) {
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
            float intensity = maxCount > 0 ? static_cast<float>(count) / maxCount : 0.0f;
            float lightness = 0.95f - intensity * 0.7f;
            painter.fillRect(labelWidth + x, yOffset, 1, laneHeight, QColor::fromHslF(hue, 0.7f, lightness));
        }
        yOffset += laneHeight + 1;
    };

    if (m_chkCan->isChecked()) {
        drawLane(static_cast<size_t>(fastrace::ProtocolGroup::CAN), "CAN", 0.6f); // Blue
    }
    if (m_chkEthernet->isChecked()) {
        drawLane(static_cast<size_t>(fastrace::ProtocolGroup::Ethernet), "ETH", 0.45f); // Teal
    }

    // Draw visible window overlay
    if (m_visibleEndUs > m_visibleStartUs && durationUs > 0) {
        int xStart = labelWidth + (w * (m_visibleStartUs - hist.traceStartUs)) / durationUs;
        int xEnd = labelWidth + (w * (m_visibleEndUs - hist.traceStartUs)) / durationUs;
        xStart = std::max(labelWidth, std::min(xStart, labelWidth + w));
        xEnd = std::max(labelWidth, std::min(xEnd, labelWidth + w));

        if (xEnd > xStart) {
            painter.setPen(QColor(255, 255, 255, 150));
            painter.setBrush(QColor(255, 255, 255, 80));
            painter.drawRect(xStart, 30, xEnd - xStart, yOffset - 30);
        }
    }
}

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

    int labelWidth = 50;
    int w = width() - labelWidth;
    if (w <= 0) {
        return;
    }

    int x = event->pos().x() - labelWidth;
    if (x >= 0 && x < w) {
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

    int labelWidth = 50;
    int w = width() - labelWidth;
    if (w <= 0) {
        return;
    }

    int x = event->pos().x() - labelWidth;
    int y = event->pos().y();

    if (x >= 0 && x < w) {
        int yOffset = 30;
        int laneHeight = 18;

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

    int labelWidth = 50;
    int w = width() - labelWidth;
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
