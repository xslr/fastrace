#include "TimelineWidget.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
#include <spdlog/spdlog.h>

#include "ui_TimelineWidget.h"

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

    connect(ui->btnAddSignal, &QPushButton::clicked, this, &TimelineWidget::onBtnAddSignalClicked);
}

TimelineWidget::~TimelineWidget() { delete ui; }

void TimelineWidget::attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer)
{
    m_analyzer = std::move(analyzer);
    // Re-run all existing lanes with the new analyzer (e.g. after trace reload).
    if (m_analyzer && !m_analyzer->getChunkIndex().empty()) {
        for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
            startSignalJob(i);
        }
    }
}

void TimelineWidget::paintEvent(QPaintEvent* event) { QWidget::paintEvent(event); }

void TimelineWidget::handleLanesMouseMove(QPoint pos)
{
    int laneIdx = pos.y() / 40;
    if (laneIdx >= static_cast<int>(m_lanes.size())) {
        laneIdx = -1;
    }
    if (m_hoveredLaneIndex != laneIdx) {
        m_hoveredLaneIndex = laneIdx;
        if (m_signalLanesWidget) {
            m_signalLanesWidget->update();
        }
    }
}

void TimelineWidget::handleLanesLeave()
{
    if (m_hoveredLaneIndex != -1) {
        m_hoveredLaneIndex = -1;
        if (m_signalLanesWidget) {
            m_signalLanesWidget->update();
        }
    }
}

void TimelineWidget::handleLanesMousePress(QPoint pos)
{
    int laneIdx = pos.y() / 40;
    if (laneIdx >= 0 && laneIdx < static_cast<int>(m_lanes.size())) {
        constexpr int kLabelWidth = 120;
        constexpr int kBtnSize = 16;
        constexpr int kBtnMargin = 4;

        QRect btnRect(kLabelWidth - kBtnMargin - kBtnSize, laneIdx * 40 + (40 - kBtnSize) / 2, kBtnSize, kBtnSize);
        if (btnRect.contains(pos)) {
            if (m_lanes[laneIdx].watcher) {
                delete m_lanes[laneIdx].watcher;
            }
            m_lanes.erase(m_lanes.begin() + laneIdx);

            if (m_signalLanesWidget) {
                m_signalLanesWidget->setFixedHeight(static_cast<int>(m_lanes.size()) * 40);
                m_hoveredLaneIndex = -1;
                m_signalLanesWidget->update();
            }
            emit signalsChanged(currentSignalNames());
        }
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

    for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
        QRect laneRect(rect.left(), i * 40, rect.width(), 40);
        paintLane(p, m_lanes[i], laneRect, i);
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

    // autofit bin values on y scale
    uint64_t maxVal = lane.maxRaw;
    if (maxVal == 0) {
        maxVal = 1;
    }

    int numBins = static_cast<int>(lane.bins.size());
    int plotW = plotRect.width();
    int plotH = plotRect.height();
    int plotBottom = plotRect.bottom();

    for (int i = 0; i < numBins; ++i) {
        const auto& bin = lane.bins[i];
        if (!bin.hasData) {
            continue;
        }

        float x = float(plotRect.left()) + static_cast<float>(static_cast<int64_t>(i) * (float)plotW / (float)numBins);

        int yMax
            = plotBottom - static_cast<int>(static_cast<int64_t>(bin.maxRaw) * plotH / static_cast<int64_t>(maxVal));
        int yMin
            = plotBottom - static_cast<int>(static_cast<int64_t>(bin.minRaw) * plotH / static_cast<int64_t>(maxVal));
        yMax = std::max(plotRect.top(), std::min(yMax, plotBottom));
        yMin = std::max(plotRect.top(), std::min(yMin, plotBottom));

        p.setPen(dotColor);
        // p.drawPoint(x, yMax);
        // TODO: use drawRects for performance
        // p.drawRect(x, yMin, x + plotW/numBins, yMax);
        // int xx = plotRect.x(), yy = plotRect.y(), ww=plotRect.width(), hh=plotRect.height();
        float xx = x;
        float hh = float(plotRect.height()) * (float(bin.maxRaw - bin.minRaw)) / float(lane.maxRaw);
        float yy = plotBottom - hh;
        float ww = float(plotRect.width()) / float(numBins);
        p.drawRect(xx, yy, ww, hh);
        // spdlog::info("xx:{} yy:{} ww:{} hh:{}", xx, yy, ww, hh);
        // p.fillRect(xx+5, yy+5, ww - 10, hh - 10, dotColor);
        // p.fillRect(x, yMin, x + plotW/numBins, yMax, dotColor);
        // if (yMax != yMin) {
        //     p.drawPoint(x, yMin);
        // }
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

    int laneIdx = static_cast<int>(m_lanes.size()) - 1;
    auto* w = lane.watcher;
    connect(lane.watcher, &QFutureWatcher<void>::finished, this, [this, w]() {
        for (int i = 0; i < static_cast<int>(m_lanes.size()); ++i) {
            if (m_lanes[i].watcher == w) {
                onSignalJobFinished(i);
                break;
            }
        }
    });

    startSignalJob(laneIdx);

    m_signalLanesWidget->setFixedHeight(static_cast<int>(m_lanes.size()) * 40);
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
    // signalNames.removeDuplicates();
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

void TimelineWidget::startSignalJob(int laneIdx)
{
    const int laneWidth = 5;
    if (!m_analyzer || laneIdx < 0 || laneIdx >= static_cast<int>(m_lanes.size())) {
        return;
    }

    m_analyzer->histogramCancelled.store(false, std::memory_order_relaxed);
    m_analyzer->histogramChunksProcessed.store(0, std::memory_order_relaxed);

    auto& lane = m_lanes[laneIdx];
    lane.loading = true;

    int numBins = std::max(1, m_signalLanesWidget ? m_signalLanesWidget->width() / laneWidth : 200);

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

void TimelineWidget::onSignalJobFinished(int laneIdx)
{
    if (laneIdx < 0 || laneIdx >= static_cast<int>(m_lanes.size())) {
        return;
    }
    auto& lane = m_lanes[laneIdx];
    if (lane.pendingBins) {
        lane.bins = std::move(*lane.pendingBins);
        lane.pendingBins.reset();
    }
    lane.loading = false;

    lane.minRaw = 0;
    lane.maxRaw = 0;

    for (auto& bin : lane.bins) {
        lane.minRaw = std::min(bin.minRaw, lane.minRaw);
        lane.maxRaw = std::max(bin.maxRaw, lane.maxRaw);
    }

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
