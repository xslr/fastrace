#include "MainWindow.h"
#include "TopBarWidget.h"
#include "TimelineWidget.h"
#include "MessageListWidget.h"
#include "MessageDetailsWidget.h"
#include "DetectionsWidget.h"
#include "ScriptEditorWidget.h"
#include "OverviewView.h"
#include "NotebookView.h"

#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AutoTrace Analyzer");
    resize(1400, 900);

    // ── Top bar ───────────────────────────────────────────────────────────────
    m_topBar = new TopBarWidget;
    setMenuWidget(m_topBar);

    // ── Shared widgets (created once, reparented between views) ───────────────
    m_timeline   = new TimelineWidget;

    // ── Overview-exclusive widgets ────────────────────────────────────────────
    m_messageDetails = new MessageDetailsWidget;
    m_detections     = new DetectionsWidget;

    // ── Notebook-exclusive widgets ────────────────────────────────────────────
    m_scriptEditor = new ScriptEditorWidget;

    // ── Construct the two views ───────────────────────────────────────────────
    m_overviewView = new OverviewView(m_timeline,
                                      m_messageDetails,
                                      m_detections);

    m_notebookView = new NotebookView(m_timeline, m_scriptEditor);

    // ── Stacked widget: index 0 = Overview, index 1 = Notebook ───────────────
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_overviewView);   // index 0
    m_stack->addWidget(m_notebookView);   // index 1

    setCentralWidget(m_stack);

    // ── Activate the default view (Overview) ──────────────────────────────────
    m_overviewView->activate();

    // ── Connect trace-file loading to both message lists ─────────────────────
    connect(m_topBar, &TopBarWidget::traceFileChanged,
            m_overviewView->messageList(), &MessageListWidget::loadFile);
    connect(m_topBar, &TopBarWidget::traceFileChanged,
            m_notebookView->messageList(), &MessageListWidget::loadFile);

    // ── Connect mode toggle ───────────────────────────────────────────────────
    connect(m_topBar, &TopBarWidget::modeChanged,
            this, &MainWindow::onModeChanged);

    // ── Status bar ────────────────────────────────────────────────────────────
    auto *statusBar = new QStatusBar;
    statusBar->addWidget(
        new QLabel("🟢 Trace Loaded     12,358 messages     18 ECUs     No Filters"));
    statusBar->addPermanentWidget(
        new QLabel("Window: 10 s     Cursor: 00:02:19.350"));
    setStatusBar(statusBar);
}

void MainWindow::onModeChanged(TopBarWidget::ViewMode mode)
{
    if (mode == TopBarWidget::ViewMode::Overview) {
        // Deactivate notebook first (detaches shared timeline)
        m_notebookView->deactivate();
        m_stack->setCurrentIndex(0);
        m_overviewView->activate();
    } else {
        // Deactivate overview first (detaches shared timeline)
        m_overviewView->deactivate();
        m_stack->setCurrentIndex(1);
        m_notebookView->activate();
    }
}
