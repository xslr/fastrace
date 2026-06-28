#include "OverviewView.h"

#include <QSplitter>
#include <QVBoxLayout>

#include "DetectionsWidget.h"
#include "MessageDetailsWidget.h"
#include "MessageListWidget.h"
#include "TimelineOverviewWidget.h"
#include "TimelineWidget.h"

OverviewView::OverviewView(TimelineOverviewWidget* timelineOverview, TimelineWidget* sharedTimeline,
    MessageDetailsWidget* messageDetails, DetectionsWidget* detectionsWidget, QWidget* parent)
    : QWidget(parent)
    , m_timelineOverview(timelineOverview)
    , m_timeline(sharedTimeline)
    , m_messageDetails(messageDetails)
    , m_detections(detectionsWidget)
{
    // The overview has its own independent MessageListWidget.
    m_messageList = new MessageListWidget(this);

    // ── Right sidebar: DetectionsWidget (top) + MessageDetailsWidget (bottom) ─
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->setChildrenCollapsible(false);
    m_detections->setParent(m_rightSplitter);
    m_rightSplitter->addWidget(m_detections);
    m_messageDetails->setParent(m_rightSplitter);
    m_rightSplitter->addWidget(m_messageDetails);
    m_rightSplitter->setSizes({ 350, 300 });

    // ── Centre column: Timeline (top) + MessageList (bottom) ─────────────────
    // The timeline will be added during activate(), because it is shared and
    // may start life in the Notebook view.  We build the splitter now and
    // insert the timeline on activate().
    m_centreSplitter = new QSplitter(Qt::Vertical, this);
    m_centreSplitter->setChildrenCollapsible(false);
    m_centreSplitter->addWidget(m_messageList);
    // Timeline placeholder until activate() is called; setSizes deferred too.

    // ── Main horizontal splitter: Centre | Right ───────────────────────
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->addWidget(m_centreSplitter);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setSizes({ 750, 300 });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_mainSplitter);

    connect(
        m_messageList, &MessageListWidget::messageSelected, m_messageDetails, &MessageDetailsWidget::updateFromMessage);

    // ── Zoom ↔ overview synchronisation ──────────────────────────────────────
    // TimelineWidget emits visibleWindowChanged when the user scrolls to zoom.
    // TimelineOverviewWidget shows the resulting rectangle.
    connect(m_timeline, &TimelineWidget::visibleWindowChanged, m_timelineOverview,
        &TimelineOverviewWidget::setVisibleWindow);

    // Dragging the rectangle in the overview pans the timeline.
    connect(
        m_timelineOverview, &TimelineOverviewWidget::windowPanRequested, m_timeline, &TimelineWidget::setVisibleWindow);
}

void OverviewView::activate()
{
    // Insert overview at the very top of centre splitter (index 0)
    if (m_timelineOverview) {
        m_centreSplitter->insertWidget(0, m_timelineOverview);
        m_timelineOverview->show();
    }
    // Shared TimelineWidget becomes a child of the centre splitter (index 1)
    if (m_timeline) {
        m_centreSplitter->insertWidget(1, m_timeline);
        m_timeline->show();
    }
}

void OverviewView::deactivate()
{
    if (m_timelineOverview) {
        m_timelineOverview->setParent(nullptr);
    }
    if (m_timeline) {
        m_timeline->setParent(nullptr);
    }
}
