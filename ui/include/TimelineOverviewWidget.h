/**
 * \file TimelineOverviewWidget.h
 * \brief Full-trace density heatmap / mini-map widget.
 *
 * \details TimelineOverviewWidget renders a compact, full-duration view of
 * the loaded trace as a per-protocol density heatmap.  Its primary
 * purpose is spatial navigation: clicking anywhere in the widget emits
 * navigateRequested() with the corresponding timestamp so the main
 * message-list view can jump to that position.
 *
 * Two histogram lanes are drawn — one for CAN and one for Ethernet —
 * each toggleable via the CAN / Ethernet checkboxes.  The histogram is
 * computed asynchronously (QtConcurrent) and debounced on resize so
 * that the UI thread remains unblocked.  A semi-transparent overlay
 * rectangle marks the currently visible time window.
 *
 * Key signals:
 *  - navigateRequested(uint64_t timestampUs) — emitted on mouse click
 *    outside the visible-window rectangle.
 *  - windowPanRequested(uint64_t startUs, uint64_t endUs) — emitted
 *    while dragging the visible-window rectangle to pan the timeline.
 */
#pragma once

#include <QFutureWatcher>
#include <QTimer>
#include <QWidget>
#include <array>
#include <memory>
#include <vector>

namespace fastrace {
class Analyzer;
}

class QCheckBox;
class QHBoxLayout;

class TimelineOverviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineOverviewWidget(QWidget* parent = nullptr);
    ~TimelineOverviewWidget() override = default;

    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

    void setVisibleWindow(uint64_t startUs, uint64_t endUs);

    void activate();
    void deactivate();

    // ── Test accessors (const, zero-overhead) ────────────────────────────────
    /// Returns the start of the currently highlighted visible window (microseconds).
    uint64_t visibleStartUs() const { return m_visibleStartUs; }
    /// Returns the end of the currently highlighted visible window (microseconds).
    uint64_t visibleEndUs() const { return m_visibleEndUs; }

signals:
    void navigateRequested(uint64_t timestampUs);
    /// Emitted while dragging the visible-window rectangle; the receiver
    /// should pan its timeline view to [startUs, endUs].
    void windowPanRequested(uint64_t startUs, uint64_t endUs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onLaneToggled();
    void onHistogramFinished();

private:
    void restartHistogramJob();

    /// Converts a widget X coordinate to a trace timestamp.
    /// Returns 0 if no analyzer is attached or x is out of range.
    uint64_t xToTimestamp(int x) const;

    /// Returns the pixel X that corresponds to the given timestamp.
    int timestampToX(uint64_t ts) const;

    /// Returns the pixel rect of the visible-window overlay, or an empty rect.
    QRect visibleWindowRect() const;

    std::shared_ptr<fastrace::Analyzer> m_analyzer;

    QCheckBox* m_chkCan = nullptr;
    QCheckBox* m_chkEthernet = nullptr;

    // UI state
    uint64_t m_visibleStartUs = 0;
    uint64_t m_visibleEndUs = 0;

    // Drag state for the visible-window rectangle
    bool m_dragging = false;
    int m_dragStartX = 0; ///< widget X where drag began
    uint64_t m_dragWinStartUs = 0; ///< m_visibleStartUs at drag start
    uint64_t m_dragWinEndUs = 0; ///< m_visibleEndUs at drag start

    // Async jobs
    QFutureWatcher<void> m_histogramWatcher;
    QTimer m_repaintTimer;
    QTimer m_debounceTimer;
};
