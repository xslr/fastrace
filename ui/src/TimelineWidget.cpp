/**
 * \file TimelineWidget.cpp
 * \brief Implementation of the signal-timeline panel.
 *
 * \details This file implements two cooperating classes:
 *
 *  - **SignalLanesWidget** (internal, not exposed in headers): a plain
 *    QWidget that fills the scroll area and delegates all painting and
 *    mouse events back to TimelineWidget.  Separating it from the outer
 *    widget simplifies scroll-area management.
 *
 *  - **TimelineWidget**: the public panel.  It owns a list of SignalLane
 *    structs, one per added ISignal.  Each lane holds the computed
 *    histogram bins (min/max raw value per pixel-wide slot) together
 *    with the QFutureWatcher that tracks the asynchronous job.
 *
 * Async model:
 *  startSignalJob() launches QtConcurrent::run() and starts a 100 ms
 *  repaint timer so the progress bar animates.  onSignalJobFinished()
 *  swaps in the completed bin vector and stops the timer once all lanes
 *  have finished loading.
 *
 * Zoom model:
 *  handleLanesWheel() adjusts [m_visibleStartUs, m_visibleEndUs] at 1.2×
 *  per scroll notch toward the cursor time position.  After the adjustment
 *  it immediately calls applySubsampledView() (instant repaint from the
 *  cached full-trace bins) and arms a 200 ms debounce timer.  When the
 *  timer fires, startRangeJob() launches a high-resolution re-fetch for
 *  the zoomed window via buildSignalTimeSeriesRange().
 *
 * Lane geometry:
 *  Each lane is 40 px tall.  The left 120 px is the label area
 *  (signal name + hover-to-reveal remove button); the remainder is
 *  the plot area.
 */

#include "TimelineWidget.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
#include <algorithm>
#include <spdlog/spdlog.h>

#include "ui_TimelineWidget.h"

// Zoom factor applied per scroll-wheel notch (1.2 = 20 % per step).
static constexpr double kZoomFactor = 1.2;
// Minimum visible window width in microseconds (1 ms).
static constexpr uint64_t kMinWindowUs = 1'000;
// Debounce interval in ms before a full-resolution re-fetch fires.
static constexpr int kZoomDebounceMs = 200;

// ---------------------------------------------------------------------------
// TimeAxisWidget
// ---------------------------------------------------------------------------
class TimeAxisWidget : public QWidget {
public:
    explicit TimeAxisWidget(TimelineWidget* parent)
        : QWidget(parent)
        , m_timeline(parent)
    {
        setMinimumHeight(20);
        setMaximumHeight(20);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        m_timeline->paintTimeAxisWidget(p, rect());
    }

private:
    TimelineWidget* m_timeline;
};

// ---------------------------------------------------------------------------
// SignalLanesWidget — paints lane content inside the scroll area
// ---------------------------------------------------------------------------
class SignalLanesWidget : public QWidget {
public:
    explicit SignalLanesWidget(TimelineWidget* parent)
        : QWidget(parent)
        , m_timeline(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        m_timeline->paintLanesWidget(p, rect());
    }

    void mouseMoveEvent(QMouseEvent* event) override { m_timeline->handleLanesMouseMove(event->pos()); }

    void leaveEvent(QEvent* event) override
    {
        m_timeline->handleLanesLeave();
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_timeline->handleLanesMousePress(event->pos());
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_timeline->handleLanesMouseRelease(event->pos());
        }
    }

    void wheelEvent(QWheelEvent* event) override { m_timeline->handleLanesWheel(event); }

private:
    TimelineWidget* m_timeline;
};

// ---------------------------------------------------------------------------
// TimelineWidget
// ---------------------------------------------------------------------------

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::TimelineWidget)
{
    ui->setupUi(this);

    ui->timelineHeader->setObjectName("headerLabel");

    for (auto* btn : { ui->btnAnomalies, ui->btnBookmarks }) {
        btn->setObjectName("iconBtn");
    }

    // Create the time axis widget and insert it above the scroll area
    m_timeAxisWidget = new TimeAxisWidget(this);
    ui->verticalLayout->insertWidget(1, m_timeAxisWidget);

    // Create the lanes widget and set it as the scroll area's content
    m_signalLanesWidget = new SignalLanesWidget(this);
    m_signalLanesWidget->setMinimumHeight(0);
    ui->signalScrollArea->setWidget(m_signalLanesWidget);
    ui->signalScrollArea->setWidgetResizable(true);

    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setInterval(100);
    connect(m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (m_signalLanesWidget) {
            m_signalLanesWidget->update();
        }
    });

    m_zoomDebounceTimer = new QTimer(this);
    m_zoomDebounceTimer->setSingleShot(true);
    m_zoomDebounceTimer->setInterval(kZoomDebounceMs);
    connect(m_zoomDebounceTimer, &QTimer::timeout, this, &TimelineWidget::onZoomDebounceTimeout);

    connect(ui->btnAddSignal, &QPushButton::clicked, this, &TimelineWidget::onBtnAddSignalClicked);
    connect(ui->btnAutoFit, &QPushButton::toggled, this, &TimelineWidget::onAutoFitToggled);
}

TimelineWidget::~TimelineWidget() { delete ui; }

void TimelineWidget::attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer)
{
    m_analyzer = std::move(analyzer);
    // Reset the visible window so the full trace is shown on reload.
    m_visibleStartUs = 0;
    m_visibleEndUs = 0;
    // Re-run all existing lanes with the new analyzer (e.g. after trace reload).
    if (m_analyzer && !m_analyzer->getChunkIndex().empty()) {
        for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
            startSignalJob(i);
        }
    }
}

void TimelineWidget::paintEvent(QPaintEvent* event) { QWidget::paintEvent(event); }

void TimelineWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (ui->btnAutoFit->isChecked()) {
        updateLanesLayout();
    }
}

void TimelineWidget::updateLanesLayout()
{
    if (m_lanes.empty() || !m_signalLanesWidget) {
        return;
    }

    if (ui->btnAutoFit->isChecked()) {
        int totalAvailableHeight = ui->signalScrollArea->viewport()->height();
        int heightPerLane = std::max(40, totalAvailableHeight / static_cast<int>(m_lanes.size()));
        for (auto& lane : m_lanes) {
            lane.height = heightPerLane;
        }
    }

    int totalHeight = 0;
    for (const auto& lane : m_lanes) {
        totalHeight += lane.height;
    }
    m_signalLanesWidget->setFixedHeight(totalHeight);
    m_signalLanesWidget->update();
}

void TimelineWidget::onAutoFitToggled(bool checked)
{
    if (checked) {
        updateLanesLayout();
    }
}

void TimelineWidget::handleLanesMouseMove(QPoint pos)
{
    if (m_dragResizeLaneIndex != -1) {
        int dy = pos.y() - m_dragResizeStartY;
        int newHeight = std::max(40, m_dragResizeStartHeight + dy);
        if (newHeight != m_lanes[m_dragResizeLaneIndex].height) {
            m_lanes[m_dragResizeLaneIndex].height = newHeight;
            updateLanesLayout();
        }
        return;
    }

    int laneIdx = -1;
    int currentY = 0;
    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        if (pos.y() >= currentY && pos.y() < currentY + m_lanes[i].height) {
            laneIdx = i;
            break;
        }
        currentY += m_lanes[i].height;
    }

    if (m_hoveredLaneIndex != laneIdx) {
        m_hoveredLaneIndex = laneIdx;
        if (m_signalLanesWidget) {
            m_signalLanesWidget->update();
        }
    }

    // Check if near bottom border of any lane
    const int resizeBorderWidth = 5;
    currentY = 0;
    bool nearBorder = false;
    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        currentY += m_lanes[i].height;
        if (std::abs(pos.y() - currentY) <= resizeBorderWidth) {
            nearBorder = true;
            break;
        }
    }

    if (nearBorder) {
        m_signalLanesWidget->setCursor(Qt::SplitVCursor);
    } else {
        m_signalLanesWidget->unsetCursor();
    }
}

void TimelineWidget::handleLanesLeave()
{
    if (m_dragResizeLaneIndex != -1) {
        return;
    }
    if (m_hoveredLaneIndex != -1) {
        m_hoveredLaneIndex = -1;
        if (m_signalLanesWidget) {
            m_signalLanesWidget->unsetCursor();
            m_signalLanesWidget->update();
        }
    }
}

void TimelineWidget::handleLanesMousePress(QPoint pos)
{
    int currentY = 0;
    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        currentY += m_lanes[i].height;
        if (std::abs(pos.y() - currentY) <= 3) {
            m_dragResizeLaneIndex = i;
            m_dragResizeStartY = pos.y();
            m_dragResizeStartHeight = m_lanes[i].height;
            if (ui->btnAutoFit->isChecked()) {
                ui->btnAutoFit->setChecked(false);
            }
            return;
        }
    }

    int laneIdx = -1;
    currentY = 0;
    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        if (pos.y() >= currentY && pos.y() < currentY + m_lanes[i].height) {
            laneIdx = i;
            break;
        }
        currentY += m_lanes[i].height;
    }

    if (laneIdx >= 0 && laneIdx < static_cast<int>(m_lanes.size())) {
        constexpr int kLabelWidth = 120;
        constexpr int kBtnSize = 16;
        constexpr int kBtnMargin = 4;

        // Calculate lane start Y
        int laneStartY = 0;
        for (int i = 0; i < laneIdx; ++i) {
            laneStartY += m_lanes[i].height;
        }

        QRect btnRect(kLabelWidth - kBtnMargin - kBtnSize, laneStartY + (m_lanes[laneIdx].height - kBtnSize) / 2,
            kBtnSize, kBtnSize);
        if (btnRect.contains(pos)) {
            if (m_lanes[laneIdx].watcher) {
                delete m_lanes[laneIdx].watcher;
            }
            m_lanes.erase(m_lanes.begin() + laneIdx);

            m_hoveredLaneIndex = -1;
            updateLanesLayout();
            emit signalsChanged(currentSignalNames());
        }
    }
}

void TimelineWidget::handleLanesMouseRelease(QPoint) { m_dragResizeLaneIndex = -1; }

void TimelineWidget::handleLanesWheel(QWheelEvent* event)
{
    if (!m_analyzer) {
        event->ignore();
        return;
    }

    const auto& hist = m_analyzer->histogram();
    const uint64_t traceStart = hist.traceStartUs;
    const uint64_t traceEnd = hist.traceEndUs;
    if (traceEnd <= traceStart) {
        event->ignore();
        return;
    }

    // Determine current window (default = full trace)
    uint64_t winStart = (m_visibleStartUs == 0 && m_visibleEndUs == 0) ? traceStart : m_visibleStartUs;
    uint64_t winEnd = (m_visibleStartUs == 0 && m_visibleEndUs == 0) ? traceEnd : m_visibleEndUs;

    // Map cursor X to a timestamp in the current window
    constexpr int kLabelWidth = 120;
    const int plotW = m_signalLanesWidget ? m_signalLanesWidget->width() - kLabelWidth : width() - kLabelWidth;
    const int cursorX = event->position().x() - kLabelWidth;

    const uint64_t cursorUs = (plotW > 0 && cursorX >= 0)
        ? winStart + static_cast<uint64_t>(cursorX) * (winEnd - winStart) / static_cast<uint64_t>(plotW)
        : (winStart + winEnd) / 2;

    // Scroll up (positive angleDelta.y) = zoom in = shrink window
    const bool zoomIn = event->angleDelta().y() > 0;
    const double factor = zoomIn ? (1.0 / kZoomFactor) : kZoomFactor;

    const double newHalfSpan = static_cast<double>(winEnd - winStart) * factor / 2.0;
    const double cursorFrac
        = (winEnd > winStart) ? static_cast<double>(cursorUs - winStart) / static_cast<double>(winEnd - winStart) : 0.5;

    // Anchor the cursor: the fraction of the window at the cursor stays fixed
    int64_t newStart = static_cast<int64_t>(cursorUs) - static_cast<int64_t>(newHalfSpan * 2.0 * cursorFrac);
    int64_t newEnd = newStart + static_cast<int64_t>(newHalfSpan * 2.0);

    // Clamp to [traceStart, traceEnd]
    if (newStart < static_cast<int64_t>(traceStart)) {
        newEnd += static_cast<int64_t>(traceStart) - newStart;
        newStart = static_cast<int64_t>(traceStart);
    }
    if (newEnd > static_cast<int64_t>(traceEnd)) {
        newStart -= newEnd - static_cast<int64_t>(traceEnd);
        newEnd = static_cast<int64_t>(traceEnd);
    }
    newStart = (std::max)(newStart, static_cast<int64_t>(traceStart));
    newEnd = (std::min)(newEnd, static_cast<int64_t>(traceEnd));

    // Enforce minimum window
    if (static_cast<uint64_t>(newEnd - newStart) < kMinWindowUs) {
        const int64_t centre = (newStart + newEnd) / 2;
        newStart = centre - static_cast<int64_t>(kMinWindowUs / 2);
        newEnd = centre + static_cast<int64_t>(kMinWindowUs / 2);
        newStart = (std::max)(newStart, static_cast<int64_t>(traceStart));
        newEnd = (std::min)(newEnd, static_cast<int64_t>(traceEnd));
    }

    m_visibleStartUs = static_cast<uint64_t>(newStart);
    m_visibleEndUs = static_cast<uint64_t>(newEnd);

    // Immediate feedback: subsample existing full-trace bins
    applySubsampledView();

    // Arm debounce for high-resolution re-fetch
    m_zoomDebounceTimer->start();

    if (m_timeAxisWidget) {
        m_timeAxisWidget->update();
    }

    emit visibleWindowChanged(m_visibleStartUs, m_visibleEndUs);
    event->accept();
}

void TimelineWidget::setVisibleWindow(uint64_t startUs, uint64_t endUs)
{
    if (!m_analyzer) {
        return;
    }
    const auto& hist = m_analyzer->histogram();
    m_visibleStartUs = startUs;
    m_visibleEndUs = endUs;
    // Snap back to full trace if the window covers it entirely
    if (m_visibleStartUs <= hist.traceStartUs && m_visibleEndUs >= hist.traceEndUs) {
        m_visibleStartUs = 0;
        m_visibleEndUs = 0;
    }
    applySubsampledView();
    m_zoomDebounceTimer->start();

    if (m_timeAxisWidget) {
        m_timeAxisWidget->update();
    }

    emit visibleWindowChanged(m_visibleStartUs, m_visibleEndUs);
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void TimelineWidget::paintTimeAxisWidget(QPainter& p, QRect rect)
{
    p.fillRect(rect, QColor(40, 40, 40));
    if (!m_analyzer || (m_visibleStartUs == 0 && m_visibleEndUs == 0)) {
        return;
    }
    const uint64_t winStart = m_visibleStartUs;
    const uint64_t winEnd = m_visibleEndUs;
    constexpr int kLabelWidth = 120;
    const int plotW = rect.width() - kLabelWidth;
    p.setPen(QColor(150, 150, 150));
    const int numLabels = 4;
    for (int i = 0; i <= numLabels; ++i) {
        uint64_t ts = winStart + (winEnd - winStart) * i / numLabels;
        int x = kLabelWidth + plotW * i / numLabels;
        QString label = QString::number(static_cast<double>(ts) / 1e6, 'f', 3) + "s";
        p.drawText(x - 25, 0, 50, rect.height(), Qt::AlignCenter, label);
    }
}

void TimelineWidget::paintLanesWidget(QPainter& p, QRect rect)
{
    p.fillRect(rect, QColor(30, 30, 30));

    if (m_lanes.empty()) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect, Qt::AlignCenter, "No signals added. Click '+ Signal' to add one.");
        return;
    }

    int currentY = 0;
    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        QRect laneRect(rect.left(), currentY, rect.width(), m_lanes[i].height);
        paintLane(p, m_lanes[i], laneRect, i);
        currentY += m_lanes[i].height;
    }
}

void TimelineWidget::paintLane(QPainter& p, const SignalLane& lane, QRect rect, int laneColorIdx)
{
    constexpr int kLabelWidth = 120;

    // pick a colour for the lane
    static const QColor kColors[] = {
        QColor(80, 140, 255),
        QColor(80, 220, 120),
        QColor(255, 160, 50),
        QColor(220, 80, 80),
        QColor(180, 80, 220),
    };
    QColor dotColor = kColors[laneColorIdx % 5];

    // Separator
    p.setPen(QColor(60, 60, 60));
    p.drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());

    // Label
    p.setPen(dotColor);
    p.drawText(QRect(rect.left() + 4, rect.top(), kLabelWidth - 8, rect.height()), Qt::AlignLeft | Qt::AlignVCenter,
        QString::fromStdString(lane.iSignalName));

    // Remove button if hovered
    if (laneColorIdx == m_hoveredLaneIndex) {
        constexpr int kBtnSize = 16;
        constexpr int kBtnMargin = 4;
        QRect btnRect(rect.left() + kLabelWidth - kBtnMargin - kBtnSize, rect.top() + (rect.height() - kBtnSize) / 2,
            kBtnSize, kBtnSize);

        p.setPen(QColor(200, 80, 80));
        p.drawText(btnRect, Qt::AlignCenter, "✖");
    }

    QRect plotRect(rect.left() + kLabelWidth, rect.top(), rect.width() - kLabelWidth, rect.height());

    // loading progress indicator
    if (lane.loading && m_analyzer) {
        // Thin progress bar at top of lane
        size_t totalChunks = m_analyzer->getChunkIndex().size();
        if (totalChunks > 0) {
            size_t processed = m_analyzer->histogramChunksProcessed.load(std::memory_order_relaxed);
            float progress = static_cast<float>(processed) / static_cast<float>(totalChunks);
            p.fillRect(plotRect.left(), plotRect.top(), static_cast<int>(plotRect.width() * progress), 2, Qt::blue);
        }
        return;
    }

    if (lane.bins.empty()) {
        return;
    }

    // TODO: When sufficiently zoomed in, render data points instead of lines indicating min-max of each bin.
    // This requires Analyzer::buildSignalTimeSeries to return a list of sample points
    int numBins = static_cast<int>(lane.bins.size());
    float plotW = plotRect.width();
    float plotH = plotRect.height();
    float plotBottom = plotRect.bottom();
    const float laneH = (float)lane.maxRaw - (float)lane.minRaw;

    QVector<QLineF> lines;
    lines.reserve(numBins);

    for (int i = 0; i < numBins; ++i) {
        const auto& bin = lane.bins[i];
        if (!bin.hasData) {
            continue;
        }

#if 0
        float x = float(plotRect.left()) + static_cast<float>(static_cast<int64_t>(i) * (float)plotW / (float)numBins);
        float h = float(plotRect.height()) * (float(bin.maxRaw - bin.minRaw)) / laneH;
        float y = plotRect.top() + (plotH * (float)bin.minRaw / laneH);
        float w = float(plotRect.width()) / float(numBins);
        p.setPen(dotColor);
        p.drawRect(x, y, w, h);
#else
        float x1 = float(plotRect.left()) + static_cast<float>(static_cast<int64_t>(i) * (float)plotW / (float)numBins);
        float x2 = x1;
        float y1 = plotRect.bottom() - (plotH * (float)bin.maxRaw / laneH);
        float y2 = plotRect.bottom() - (plotH * (float)bin.minRaw / laneH);

        lines.push_back(QLineF(x1, y1, x2, y2));
#endif
    }

    if (!lines.isEmpty()) {
        p.setPen(dotColor);
        p.drawLines(lines);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::pair<uint64_t, uint64_t> TimelineWidget::effectiveWindow() const
{
    if (!m_analyzer) {
        return { 0, 0 };
    }
    if (m_visibleStartUs == 0 && m_visibleEndUs == 0) {
        const auto& hist = m_analyzer->histogram();
        return { hist.traceStartUs, hist.traceEndUs };
    }
    return { m_visibleStartUs, m_visibleEndUs };
}

void TimelineWidget::computeLaneStats(SignalLane& lane)
{
    lane.minRaw = 0;
    lane.maxRaw = 0;
    for (auto& bin : lane.bins) {
        lane.minRaw = (std::min)(bin.minRaw, lane.minRaw);
        lane.maxRaw = (std::max)(bin.maxRaw, lane.maxRaw);
    }
}

/// Subsample the full-trace bins into lane.bins for immediate zoomed display.
/// Only bins whose timestamps fall inside the visible window are kept.
void TimelineWidget::applySubsampledView()
{
    if (!m_analyzer) {
        return;
    }
    auto [winStart, winEnd] = effectiveWindow();
    if (winEnd <= winStart) {
        return;
    }

    for (auto& lane : m_lanes) {
        if (lane.loading || lane.fullTraceBins.empty()) {
            continue;
        }

        // Determine which full-trace bins fall inside the window
        std::vector<fastrace::SignalBin> visible;
        visible.reserve(lane.fullTraceBins.size());
        for (const auto& bin : lane.fullTraceBins) {
            if (bin.timestampUs >= winStart && bin.timestampUs < winEnd) {
                visible.push_back(bin);
            }
        }
        if (visible.empty()) {
            // Fall back to all bins so the lane isn't blank while zoomed in
            lane.bins = lane.fullTraceBins;
        } else {
            lane.bins = std::move(visible);
        }
        computeLaneStats(lane);
    }

    if (m_signalLanesWidget) {
        m_signalLanesWidget->update();
    }
}

QStringList TimelineWidget::currentSignalNames() const
{
    QStringList names;
    for (const auto& lane : m_lanes) {
        names << QString::fromStdString(lane.iSignalName);
    }
    return names;
}

void TimelineWidget::addSignalLane(const QString& name)
{
    if (!m_analyzer) {
        return;
    }

    // Skip duplicates
    for (const auto& lane : m_lanes) {
        if (lane.iSignalName == name.toStdString()) {
            return;
        }
    }

    // Find bitLength for this signal
    uint32_t bitLength = 0;
    for (const auto& msg : m_analyzer->arDatabase().messages) {
        for (const auto& sig : msg.signalDefs) {
            if (sig.name == name.toStdString()) {
                bitLength = sig.bitLength;
                break;
            }
        }
    }

    m_lanes.push_back({});
    auto& lane = m_lanes.back();
    lane.iSignalName = name.toStdString();
    lane.bitLength = bitLength;
    lane.watcher = new QFutureWatcher<void>(this);
    if (ui->btnAutoFit->isChecked()) {
        // If autofit is on, this newly added lane will cause a recalculation in updateLanesLayout
        // We set a default height first.
        lane.height = 40;
    } else {
        lane.height = 40;
    }

    auto* w = lane.watcher;
    connect(lane.watcher, &QFutureWatcher<void>::finished, this, [this, w]() {
        for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
            if (m_lanes[i].watcher == w) {
                onSignalJobFinished(i);
                break;
            }
        }
    });

    startSignalJob(static_cast<int>(m_lanes.size()) - 1);

    updateLanesLayout();
}

void TimelineWidget::restoreSignals(const QStringList& names)
{
    for (const auto& name : names) {
        addSignalLane(name);
    }
}

void TimelineWidget::onBtnAddSignalClicked()
{
    if (!m_analyzer || m_analyzer->arDatabase().empty()) {
        return;
    }

    // Collect all signal names
    QStringList signalNames;
    for (const auto& msg : m_analyzer->arDatabase().messages) {
        for (const auto& sig : msg.signalDefs) {
            if (!sig.name.empty()) {
                signalNames << QString::fromStdString(sig.name);
            }
        }
    }
    signalNames.sort();

    if (signalNames.isEmpty()) {
        return;
    }

    // Signal search dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Add Signal");
    dialog.resize(400, 500);
    auto* dlgLayout = new QVBoxLayout(&dialog);

    auto* searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText("Search signals…");
    dlgLayout->addWidget(searchEdit);

    auto* listWidget = new QListWidget(&dialog);
    listWidget->addItems(signalNames);
    dlgLayout->addWidget(listWidget);

    connect(searchEdit, &QLineEdit::textChanged, this, [listWidget, signalNames](const QString& text) {
        listWidget->clear();
        for (const auto& name : signalNames) {
            if (name.contains(text, Qt::CaseInsensitive)) {
                listWidget->addItem(name);
            }
        }
    });

    QString selectedSignal;
    connect(listWidget, &QListWidget::itemDoubleClicked, this, [&](QListWidgetItem* item) {
        selectedSignal = item->text();
        dialog.accept();
    });
    connect(listWidget, &QListWidget::itemActivated, this, [&](QListWidgetItem* item) {
        selectedSignal = item->text();
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || selectedSignal.isEmpty()) {
        return;
    }

    addSignalLane(selectedSignal);
    emit signalsChanged(currentSignalNames());
}

// ---------------------------------------------------------------------------
// Background jobs
// ---------------------------------------------------------------------------

/// Full-trace job — always fetches bins for the whole trace and caches them
/// in fullTraceBins so applySubsampledView() can work without hitting disk.
void TimelineWidget::startSignalJob(int laneIdx)
{
    // lane width allows grouping of samples on x axis. for now, we use 1px wide bins since it works well visually.
    // TODO: this might be something to let the user decide.
    const int laneWidth = 1;
    if (!m_analyzer || laneIdx < 0 || laneIdx >= static_cast<int>(m_lanes.size())) {
        return;
    }

    m_analyzer->histogramCancelled.store(false, std::memory_order_relaxed);
    m_analyzer->histogramChunksProcessed.store(0, std::memory_order_relaxed);

    auto& lane = m_lanes[laneIdx];
    lane.loading = true;

    int numBins = (std::max)(1, m_signalLanesWidget ? m_signalLanesWidget->width() / laneWidth : 200);

    auto pendingBins = std::make_shared<std::vector<fastrace::SignalBin>>(numBins);
    lane.pendingBins = pendingBins;

    std::string name = lane.iSignalName;
    auto future = QtConcurrent::run([analyzer = m_analyzer, name, numBins, pendingBins]() mutable {
        analyzer->buildSignalTimeSeries(name, numBins, *pendingBins);
    });
    lane.watcher->setFuture(future);
    m_repaintTimer->start();
    m_signalLanesWidget->update();
}

/// Range job — fetches high-resolution bins for [m_visibleStartUs, m_visibleEndUs].
/// Called by the zoom debounce timer.
void TimelineWidget::startRangeJob(int laneIdx)
{
    const int laneWidth = 5;
    if (!m_analyzer || laneIdx < 0 || laneIdx >= static_cast<int>(m_lanes.size())) {
        return;
    }

    auto [winStart, winEnd] = effectiveWindow();
    if (winEnd <= winStart) {
        return;
    }

    m_analyzer->histogramCancelled.store(false, std::memory_order_relaxed);
    m_analyzer->histogramChunksProcessed.store(0, std::memory_order_relaxed);

    auto& lane = m_lanes[laneIdx];
    lane.loading = true;

    int numBins = (std::max)(1, m_signalLanesWidget ? m_signalLanesWidget->width() / laneWidth : 200);

    auto pendingBins = std::make_shared<std::vector<fastrace::SignalBin>>(numBins);
    lane.pendingBins = pendingBins;

    std::string name = lane.iSignalName;
    auto future = QtConcurrent::run([analyzer = m_analyzer, name, numBins, pendingBins, s = winStart, e = winEnd]() mutable {
        analyzer->buildSignalTimeSeriesRange(name, numBins, *pendingBins, s, e);
    });
    lane.watcher->setFuture(future);
    m_repaintTimer->start();
    m_signalLanesWidget->update();
}

void TimelineWidget::onZoomDebounceTimeout()
{
    // Launch a high-resolution re-fetch for all lanes using the current window.
    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        startRangeJob(i);
    }
}

void TimelineWidget::onSignalJobFinished(int laneIdx)
{
    if (laneIdx < 0 || laneIdx >= static_cast<int>(m_lanes.size())) {
        return;
    }
    auto& lane = m_lanes[laneIdx];
    if (lane.pendingBins) {
        lane.bins = std::move(*lane.pendingBins);
        lane.pendingBins.reset();

        // Cache full-trace bins only when we just did a full-trace job (i.e.,
        // no zoom window is set).  Range jobs should not overwrite the cache.
        if (m_visibleStartUs == 0 && m_visibleEndUs == 0) {
            lane.fullTraceBins = lane.bins;
        }
    }
    lane.loading = false;

    computeLaneStats(lane);

    // Stop repaint timer if no lanes are still loading
    bool anyLoading = false;
    for (const auto& l : m_lanes) {
        if (l.loading) {
            anyLoading = true;
            break;
        }
    }
    if (!anyLoading) {
        m_repaintTimer->stop();
    }

    m_signalLanesWidget->update();
}
