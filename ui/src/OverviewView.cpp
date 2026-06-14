#include "OverviewView.h"

#include "TimelineWidget.h"
#include "MessageListWidget.h"
#include "MessageDetailsWidget.h"
#include "DetectionsWidget.h"

#include <QSplitter>
#include <QVBoxLayout>

OverviewView::OverviewView(TimelineWidget       *sharedTimeline,
                           MessageDetailsWidget *messageDetails,
                           DetectionsWidget     *detectionsWidget,
                           QWidget              *parent)
    : QWidget(parent)
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
    m_rightSplitter->setSizes({350, 300});

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
    m_mainSplitter->setSizes({750, 300});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_mainSplitter);

    connect(m_messageList, &MessageListWidget::messageSelected,
            m_messageDetails, &MessageDetailsWidget::updateFromMessage);
}

void OverviewView::activate()
{
    if (m_timeline->parent() == m_centreSplitter) return;

    // Insert the shared timeline at index 0 (above the message list)
    m_timeline->setParent(m_centreSplitter);
    m_centreSplitter->insertWidget(0, m_timeline);
    m_centreSplitter->setSizes({400, 250});
}

void OverviewView::deactivate()
{
    if (m_timeline->parent() != m_centreSplitter) return;

    // Detach without destroying
    m_centreSplitter->widget(
        m_centreSplitter->indexOf(m_timeline)
    ); // just access to confirm it's there — no-op
    m_timeline->setParent(nullptr);
}
