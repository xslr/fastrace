/**
 * \file TimelineWidget.h
 * \brief Signal-timeline panel that renders one horizontal lane per ISignal.
 *
 * \details TimelineWidget is a QWidget-based panel that lets the user add
 * named ISignals from the loaded AR database and displays each as a
 * min/max value histogram over the full trace duration.  Each signal
 * occupies a fixed-height lane; hovering reveals a remove button.
 *
 * Histogram data is computed asynchronously via QtConcurrent so the UI
 * thread is never blocked.  A QTimer drives periodic repaints while a
 * background job is in progress.
 *
 * Interaction summary:
 *  - "+ Signal" button → opens a searchable signal-picker dialog.
 *  - Hover over a lane → shows a remove (\c ✖) button on the left label.
 *  - Click the remove button → deletes the lane and emits signalsChanged().
 */
#pragma once
#include "Analyzer.h"
#include <QFutureWatcher>
#include <QPainter>
#include <QRect>
#include <QTimer>
#include <QWidget>
#include <memory>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class TimelineWidget;
}
QT_END_NAMESPACE

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget() override;

    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

    // Add lanes for signal names not already present.
    void restoreSignals(const QStringList& names);

    // Called by SignalLanesWidget::paintEvent
    void paintLanesWidget(QPainter& p, QRect rect);

    // Called by SignalLanesWidget for interaction
    void handleLanesMouseMove(QPoint pos);
    void handleLanesLeave();
    void handleLanesMousePress(QPoint pos);

    // ── Test accessors (const, zero-overhead) ────────────────────────────────
    /// Returns the index of the currently hovered lane, or -1 if none.
    int hoveredLaneIndex() const { return m_hoveredLaneIndex; }
    /// Returns the number of signal lanes currently displayed.
    int laneCount() const { return static_cast<int>(m_lanes.size()); }

protected:
    void paintEvent(QPaintEvent* event) override;

signals:
    void signalsChanged(QStringList names);

private slots:
    void onBtnAddSignalClicked();
    void onSignalJobFinished(int laneIndex);

private:
    struct SignalLane {
        std::string iSignalName;
        std::vector<fastrace::SignalBin> bins;
        std::shared_ptr<std::vector<fastrace::SignalBin>> pendingBins;
        QFutureWatcher<void>* watcher = nullptr;
        bool loading = false;
        uint32_t bitLength = 0;
        uint64_t maxRaw { 0 };
        uint64_t minRaw { 0 };
    };

    QStringList currentSignalNames() const;
    void addSignalLane(const QString& name);
    void startSignalJob(int laneIdx);
    void paintLane(QPainter& p, const SignalLane& lane, QRect rect, int laneColorIdx);

    Ui::TimelineWidget* ui;
    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    std::vector<SignalLane> m_lanes;
    QWidget* m_signalLanesWidget = nullptr;
    QTimer* m_repaintTimer = nullptr;
    int m_hoveredLaneIndex = -1;
};
