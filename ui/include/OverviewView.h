#pragma once
#include <QWidget>

class QSplitter;
class TimelineOverviewWidget;
class TimelineWidget;
class MessageListWidget;
class MessageDetailsWidget;
class DetectionsWidget;

/**
 * OverviewView implements the "2-Panel layout" described in the wireframe:
 *
 *   ┌────────────────────────────────────┬────────────────────┐
 *   │  CENTRE (vertical splitter)        │  RIGHT SIDEBAR     │
 *   │  ─ TimelineWidget (shared)         │  ─ DetectionsWidget│
 *   │  ─ MessageListWidget (own instance)│  ─ MessageDetails  │
 *   └────────────────────────────────────┴────────────────────┘
 *
 * TimelineWidget is shared; OverviewView re-parents it when activated.
 */
class OverviewView : public QWidget {
    Q_OBJECT
public:
    explicit OverviewView(TimelineOverviewWidget* timelineOverview, TimelineWidget* sharedTimeline,
        MessageDetailsWidget* messageDetails, DetectionsWidget* detectionsWidget, QWidget* parent = nullptr);

    /**
     * Call when the Overview becomes the active view.
     * Re-parents the shared TimelineWidget into the centre splitter.
     */
    void activate();

    /**
     * Call before switching away.
     * Detaches the shared TimelineWidget so it can be re-parented elsewhere.
     */
    void deactivate();

    /** The Overview's own MessageListWidget (filters independent of Notebook). */
    MessageListWidget* messageList() const { return m_messageList; }

private:
    TimelineOverviewWidget* m_timelineOverview;
    TimelineWidget* m_timeline; ///< shared, re-parented here
    MessageListWidget* m_messageList; ///< owned by this view
    MessageDetailsWidget* m_messageDetails;
    DetectionsWidget* m_detections;

    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_centreSplitter = nullptr;
    QSplitter* m_rightSplitter = nullptr;
};
