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
 *  - "+" Signal" button → opens a searchable signal-picker dialog.
 *  - Hover over a lane → shows a remove (\c ✖) button on the left label.
 *  - Click the remove button → deletes the lane and emits signalsChanged().
 *  - Scroll wheel on the plot area → zooms the X (time) axis toward the
 *    cursor position at 1.2× per notch; clamped to [1 ms, full trace].
 *    Immediately subsamples for smooth feedback, then re-fetches at full
 *    resolution after 200 ms of inactivity.
 */
#pragma once
#include "Analyzer.h"
#include <QFutureWatcher>
#include <QPainter>
#include <QRect>
#include <QTimer>
#include <QWheelEvent>
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
    void paintTimeAxisWidget(QPainter& p, QRect rect);

    // Called by SignalLanesWidget for interaction
    void handleLanesMouseMove(QPoint pos);
    void handleLanesLeave();
    void handleLanesMousePress(QPoint pos);
    void handleLanesMouseRelease(QPoint pos);
    void handleLanesWheel(QWheelEvent* event);

    // ── Test accessors (const, zero-overhead) ────────────────────────────────
    /// Returns the index of the currently hovered lane, or -1 if none.
    int hoveredLaneIndex() const { return m_hoveredLaneIndex; }
    /// Returns the number of signal lanes currently displayed.
    int laneCount() const { return static_cast<int>(m_lanes.size()); }
    /// Returns the start of the currently visible time window (microseconds).
    /// Returns 0 when no analyzer is attached or no zoom has been applied.
    uint64_t visibleStartUs() const { return m_visibleStartUs; }
    /// Returns the end of the currently visible time window (microseconds).
    uint64_t visibleEndUs() const { return m_visibleEndUs; }

public slots:
    /// Sets the visible time window externally (e.g. driven by the overview
    /// minimap drag).  Re-fetches bins for the new range immediately.
    void setVisibleWindow(uint64_t startUs, uint64_t endUs);

signals:
    void signalsChanged(QStringList names);
    /// Emitted whenever the visible time window changes due to zoom or pan.
    void visibleWindowChanged(uint64_t startUs, uint64_t endUs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onBtnAddSignalClicked();
    void onAutoFitToggled(bool checked);
    void onSignalJobFinished(int laneIndex);
    void onZoomDebounceTimeout();

private:
    struct SignalLane {
        std::string iSignalName;
        std::vector<fastrace::SignalBin> bins;
        std::vector<fastrace::SignalBin> fullTraceBins; ///< full-resolution full-trace bins for subsampling
        std::shared_ptr<std::vector<fastrace::SignalBin>> pendingBins;
        QFutureWatcher<void>* watcher = nullptr;
        bool loading = false;
        uint32_t bitLength = 0;
        uint64_t maxRaw { 0 };
        uint64_t minRaw { 0 };
        int height = 40;
    };

    QStringList currentSignalNames() const;
    void addSignalLane(const QString& name);
    void startSignalJob(int laneIdx);
    void startRangeJob(int laneIdx);
    void applySubsampledView();
    void computeLaneStats(SignalLane& lane);
    void paintLane(QPainter& p, const SignalLane& lane, QRect rect, int laneColorIdx);

    // Returns the effective [startUs, endUs] — falls back to full trace if no
    // zoom window is set.
    std::pair<uint64_t, uint64_t> effectiveWindow() const;

    void updateLanesLayout();

    Ui::TimelineWidget* ui;
    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    std::vector<SignalLane> m_lanes;
    QWidget* m_signalLanesWidget = nullptr;
    QWidget* m_timeAxisWidget = nullptr;
    QTimer* m_repaintTimer = nullptr;
    QTimer* m_zoomDebounceTimer = nullptr;
    int m_hoveredLaneIndex = -1;

    int m_dragResizeLaneIndex = -1;
    int m_dragResizeStartY = 0;
    int m_dragResizeStartHeight = 0;

    // Visible time window (0/0 = full trace)
    uint64_t m_visibleStartUs = 0;
    uint64_t m_visibleEndUs = 0;
};
