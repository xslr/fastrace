#include "MainWindow.h"
#include "TopBarWidget.h"
#include "LeftPanelWidget.h"
#include "TimelineWidget.h"
#include "MessageListWidget.h"
#include "MessageDetailsWidget.h"
#include "ScriptEditorWidget.h"
#include "AnalyzerPreviewWidget.h"

#include <QSplitter>
#include <QStatusBar>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
  setWindowTitle("AutoTrace Analyzer");
  resize(1400, 900);

  setStyleSheet(R"(
      QMainWindow { background-color: #181921; color: #dcdcdc; }
      QWidget { background-color: #181921; color: #dcdcdc;
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto,
                              Helvetica, Arial, sans-serif; font-size: 13px; }
      QSplitter::handle { background-color: #272a35; }
      QPushButton { background-color: #2a2d3d; border: 1px solid #3b3f55;
                    border-radius: 4px; padding: 4px 10px; color: #ffffff; }
      QPushButton:hover { background-color: #3b3f55; }
      QPushButton#primaryBtn { background-color: #2563eb; color: white; border: none; }
      QPushButton#primaryBtn:hover { background-color: #1d4ed8; }
      QPushButton#iconBtn { background-color: transparent; border: none;
                            padding: 4px; border-radius: 4px; }
      QPushButton#iconBtn:hover { background-color: #2a2d3d; }
      QComboBox { background-color: #1e1f2b; border: 1px solid #3b3f55;
                  border-radius: 4px; padding: 2px 10px; color: #dcdcdc; }
      QComboBox::drop-down { border-left: 1px solid #3b3f55; width: 20px; }
      QTabWidget::pane { border: 1px solid #272a35; background-color: #181921; }
      QTabBar::tab { background-color: #181921; border-bottom: 2px solid transparent;
                     padding: 8px 16px; color: #8b8b99; }
      QTabBar::tab:selected { border-bottom: 2px solid #3b82f6; color: #ffffff; }
      QTabBar::tab:hover { color: #ffffff; }
      QTableWidget { background-color: #1e1f2b; alternate-background-color: #181921;
                     border: 1px solid #272a35; gridline-color: #272a35;
                     selection-background-color: #2a2d3d; color: #dcdcdc; }
      QHeaderView::section { background-color: #1e1f2b; border: none;
                             border-bottom: 1px solid #272a35;
                             border-right: 1px solid #272a35;
                             padding: 4px 8px; color: #8b8b99;
                             font-weight: bold; font-size: 11px; }
      QTreeWidget { background-color: #181921; border: none; }
      QTreeWidget::item { padding: 4px; }
      QTreeWidget::item:selected { background-color: #2a2d3d; }
      QTextEdit { background-color: #111218; border: 1px solid #272a35;
                  font-family: 'Consolas', 'Courier New', monospace;
                  font-size: 13px; selection-background-color: #3b3f55; }
      QLabel { color: #dcdcdc; }
      QLabel#headerLabel { font-size: 11px; font-weight: bold; color: #8b8b99;
                           text-transform: uppercase; padding-bottom: 4px; }
      QLabel#titleLabel  { font-size: 16px; font-weight: bold; color: #ffffff; }
      QLabel#valueLabel  { color: #ffffff; }
      QLabel#codeLabel   { font-family: 'Consolas', 'Courier New', monospace;
                           background-color: #111218; padding: 8px;
                           border: 1px solid #272a35; border-radius: 4px; }
      QScrollBar:vertical { background: #181921; width: 12px; margin: 0; }
      QScrollBar::handle:vertical { background: #3b3f55; min-height: 20px;
                                    border-radius: 6px; margin: 2px; }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
      QScrollBar:horizontal { background: #181921; height: 12px; margin: 0; }
      QScrollBar::handle:horizontal { background: #3b3f55; min-width: 20px;
                                      border-radius: 6px; margin: 2px; }
      QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
  )");

  // Top bar (shown above central widget via setMenuWidget)
  m_topBar = new TopBarWidget;
  setMenuWidget(m_topBar);

  // Component widgets
  m_leftPanel      = new LeftPanelWidget;
  m_timeline       = new TimelineWidget;
  m_messageList    = new MessageListWidget;
  m_messageDetails = new MessageDetailsWidget;
  m_scriptEditor   = new ScriptEditorWidget;
  m_preview        = new AnalyzerPreviewWidget;

  // Middle column: timeline / message list / message details stacked vertically
  auto *middleSplitter = new QSplitter(Qt::Vertical);
  middleSplitter->setChildrenCollapsible(false);
  middleSplitter->addWidget(m_timeline);
  middleSplitter->addWidget(m_messageList);
  middleSplitter->addWidget(m_messageDetails);
  middleSplitter->setSizes({400, 250, 250});

  // Right column: script editor / preview stacked vertically
  auto *rightSplitter = new QSplitter(Qt::Vertical);
  rightSplitter->setChildrenCollapsible(false);
  rightSplitter->addWidget(m_scriptEditor);
  rightSplitter->addWidget(m_preview);
  rightSplitter->setSizes({500, 400});

  // Main horizontal splitter: left | middle | right
  auto *mainSplitter = new QSplitter(Qt::Horizontal);
  mainSplitter->setChildrenCollapsible(false);
  mainSplitter->addWidget(m_leftPanel);
  mainSplitter->addWidget(middleSplitter);
  mainSplitter->addWidget(rightSplitter);
  mainSplitter->setSizes({250, 750, 400});

  setCentralWidget(mainSplitter);

  // Status bar
  auto *statusBar = new QStatusBar;
  statusBar->setStyleSheet(
      "background-color: #181921; border-top: 1px solid #272a35;"
      " color: #8b8b99; padding: 4px;");
  statusBar->addWidget(
      new QLabel("🟢 Trace Loaded     12,358 messages     18 ECUs     No Filters"));
  statusBar->addPermanentWidget(
      new QLabel("Window: 10 s     Cursor: 00:02:19.350"));
  setStatusBar(statusBar);
}
