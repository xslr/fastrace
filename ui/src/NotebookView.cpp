#include "NotebookView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QVariant>

#include "MessageListWidget.h"
#include "NotebookBlockWidget.h"
#include "ScriptEditorWidget.h"
#include "TimelineOverviewWidget.h"
#include "TimelineWidget.h"

// ── Construction ─────────────────────────────────────────────────────────────

NotebookView::NotebookView(TimelineOverviewWidget* timelineOverview, TimelineWidget* sharedTimeline,
    ScriptEditorWidget* scriptEditor, QWidget* parent)
    : QWidget(parent)
    , m_timelineOverview(timelineOverview)
    , m_timeline(sharedTimeline)
    , m_scriptEditor(scriptEditor)
{
    // Create the notebook's own independent MessageListWidget
    m_messageList = new MessageListWidget(this);

    // ── Outer layout: notebook toolbar + scroll area ──────────────────────────
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── Notebook sub-toolbar (shown below the top bar, above the canvas) ──────
    auto* toolbarFrame = new QFrame(this);
    toolbarFrame->setObjectName("notebookToolbar");
    toolbarFrame->setFixedHeight(40);

    auto* toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(16, 0, 16, 0);
    toolbarLayout->setSpacing(6);

    // Notebook name / breadcrumb
    auto* notebookLabel = new QLabel("drive_analysis_01", toolbarFrame);
    notebookLabel->setObjectName("notebookName");
    toolbarLayout->addWidget(notebookLabel);

    auto* unsavedLabel = new QLabel("— unsaved changes", toolbarFrame);
    unsavedLabel->setObjectName("unsavedLabel");
    toolbarLayout->addWidget(unsavedLabel);

    toolbarLayout->addStretch();

    // "Add block" buttons
    m_addSignalViewBtn = new QPushButton("+ Signal View", toolbarFrame);
    m_addSignalViewBtn->setObjectName("addBlockBtn");
    connect(m_addSignalViewBtn, &QPushButton::clicked, this, &NotebookView::addSignalViewBlock);
    toolbarLayout->addWidget(m_addSignalViewBtn);

    m_addMsgListBtn = new QPushButton("+ Message List", toolbarFrame);
    m_addMsgListBtn->setObjectName("addBlockBtn");
    connect(m_addMsgListBtn, &QPushButton::clicked, this, &NotebookView::addMessageListBlock);
    toolbarLayout->addWidget(m_addMsgListBtn);

    m_addScriptBtn = new QPushButton("+ Script", toolbarFrame);
    m_addScriptBtn->setObjectName("addBlockBtn");
    connect(m_addScriptBtn, &QPushButton::clicked, this, &NotebookView::addScriptBlock);
    toolbarLayout->addWidget(m_addScriptBtn);

    m_addMarkdownBtn = new QPushButton("+ Markdown", toolbarFrame);
    m_addMarkdownBtn->setObjectName("addBlockBtn");
    connect(m_addMarkdownBtn, &QPushButton::clicked, this, &NotebookView::addMarkdownBlock);
    toolbarLayout->addWidget(m_addMarkdownBtn);

    m_runAllBtn = new QPushButton("▶ Run All", toolbarFrame);
    m_runAllBtn->setObjectName("runAllBtn");
    toolbarLayout->addWidget(m_runAllBtn);

    outerLayout->addWidget(toolbarFrame);

    // Horizontal separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("toolbarSep");
    outerLayout->addWidget(sep);

    // ── Scroll area ───────────────────────────────────────────────────────────
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("notebookScrollArea");

    m_canvas = new QWidget(m_scrollArea);
    m_canvas->setObjectName("notebookCanvas");

    m_canvasLayout = new QVBoxLayout(m_canvas);
    m_canvasLayout->setContentsMargins(24, 16, 24, 16);
    m_canvasLayout->setSpacing(0);
    m_canvasLayout->addStretch(); // keeps blocks anchored to the top

    m_scrollArea->setWidget(m_canvas);
    outerLayout->addWidget(m_scrollArea, 1);
}

// ── Activation / deactivation
// ─────────────────────────────────────────────────

void NotebookView::activate()
{
    // Overview widget is inserted at the top of the canvas
    if (m_timelineOverview) {
        if (m_timelineOverview->parent() != m_canvas) {
            m_timelineOverview->setParent(m_canvas);
            m_canvasLayout->insertWidget(0, m_timelineOverview);
            m_timelineOverview->show();
        }
    }

    if (m_timeline->parent() == this) {
        return;
    }

    auto* block = wrapInBlock("Signal View", m_timeline);
    m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, block);
    m_blocks.prepend(block);

    bool msgBlockPresent = false;
    for (auto* b : m_blocks) {
        if (b->contentWidget() == m_messageList) {
            msgBlockPresent = true;
            break;
        }
    }
    if (!msgBlockPresent) {
        auto* msgBlock = wrapInBlock("Message List", m_messageList);
        m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, msgBlock);
        m_blocks.append(msgBlock);
    }

    bool scriptBlockPresent = false;
    for (auto* b : m_blocks) {
        if (b->contentWidget() == m_scriptEditor) {
            scriptBlockPresent = true;
            break;
        }
    }
    if (!scriptBlockPresent) {
        auto* scriptBlock = wrapInBlock("Script Editor", m_scriptEditor);
        m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, scriptBlock);
        m_blocks.append(scriptBlock);
    }
}

void NotebookView::deactivate()
{
    if (m_timelineOverview) {
        m_timelineOverview->setParent(nullptr);
    }
    for (int i = 0; i < m_blocks.size(); ++i) {
        auto* block = m_blocks[i];
        if (block->contentWidget() == m_timeline) {
            m_canvasLayout->removeWidget(block);
            block->setParent(nullptr);
            m_blocks.removeAt(i);
            m_timeline->setParent(nullptr);
            block->deleteLater();
            break;
        }
    }
}

// ── Block helpers
// ─────────────────────────────────────────────────────────────

NotebookBlockWidget* NotebookView::wrapInBlock(const QString& title, QWidget* content)
{
    auto* block = new NotebookBlockWidget(title, content, m_canvas);
    connect(block, &NotebookBlockWidget::deleteRequested, this, &NotebookView::onBlockDeleteRequested);
    connect(block, &NotebookBlockWidget::dragStarted, this, &NotebookView::onBlockDragStarted);
    return block;
}

void NotebookView::appendBlock(NotebookBlockWidget* block)
{
    // Insert before the trailing stretch
    m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, block);
    m_blocks.append(block);
}

// ── Slot: block deletion
// ──────────────────────────────────────────────────────

void NotebookView::onBlockDeleteRequested(NotebookBlockWidget* block)
{
    // Do not delete a block whose content we do not own: shared timeline.
    // Just detach and hide it.
    if (block->contentWidget() == m_timeline) {
        m_canvasLayout->removeWidget(block);
        block->setParent(nullptr);
        m_timeline->setParent(nullptr);
        m_blocks.removeOne(block);
        block->deleteLater();
        return;
    }

    m_blocks.removeOne(block);
    m_canvasLayout->removeWidget(block);
    block->deleteLater();
}

// ── Slot: drag-and-drop reordering ───────────────────────────────────────────

void NotebookView::onBlockDragStarted(NotebookBlockWidget* block)
{
    // Basic drag reordering: cycle the block one position up on each drag start.
    // A full drag-and-drop implementation would use QDrag; this provides the
    // structural wiring that can be enhanced later.
    int idx = m_blocks.indexOf(block);
    if (idx <= 0) {
        return;
    }

    // Swap with the block above in the list and in the layout.
    m_blocks.swapItemsAt(idx, idx - 1);

    // Re-insert in the layout at the new position.
    // The trailing stretch is always at count()-1.
    m_canvasLayout->removeWidget(block);
    m_canvasLayout->insertWidget(idx - 1, block);
}

// ── "Add block" actions
// ───────────────────────────────────────────────────────

void NotebookView::addSignalViewBlock()
{
    // If the timeline is already in a block within this view, do nothing.
    for (auto* b : m_blocks) {
        if (b->contentWidget() == m_timeline) {
            return;
        }
    }
    m_timeline->setParent(nullptr);
    auto* block = wrapInBlock("Signal View", m_timeline);
    appendBlock(block);
}

void NotebookView::addMessageListBlock()
{
    // Each "add message list" press creates a new independent instance.
    auto* newMsgList = new MessageListWidget(m_canvas);
    auto* block = wrapInBlock("Message List", newMsgList);
    appendBlock(block);
}

void NotebookView::addScriptBlock()
{
    // The primary script editor is a singleton in this view.
    for (auto* b : m_blocks) {
        if (b->contentWidget() == m_scriptEditor) {
            return;
        }
    }
    m_scriptEditor->setParent(nullptr);
    auto* block = wrapInBlock("Script Editor", m_scriptEditor);
    appendBlock(block);
}

void NotebookView::addMarkdownBlock()
{
    auto* placeholder = new QLabel("[ Markdown block — editor not yet implemented ]", m_canvas);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setFixedHeight(80);
    auto* block = wrapInBlock("Markdown", placeholder);
    appendBlock(block);
}
