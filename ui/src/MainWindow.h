#pragma once

#include <QMainWindow>
#include "TopBarWidget.h"   // for ViewMode

class TopBarWidget;
class TimelineWidget;
class MessageListWidget;
class MessageDetailsWidget;
class ScriptEditorWidget;
class DetectionsWidget;
class OverviewView;
class NotebookView;

class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onModeChanged(TopBarWidget::ViewMode mode);

private:
    // ── Shared widgets ────────────────────────────────────────────────────────
    TopBarWidget          *m_topBar         = nullptr;
    TimelineWidget        *m_timeline       = nullptr;   ///< shared, reparented

    // ── Overview-exclusive widgets ────────────────────────────────────────────
    MessageDetailsWidget  *m_messageDetails = nullptr;
    DetectionsWidget      *m_detections     = nullptr;

    // ── Notebook-exclusive widgets ────────────────────────────────────────────
    ScriptEditorWidget    *m_scriptEditor   = nullptr;

    // ── Views ─────────────────────────────────────────────────────────────────
    OverviewView          *m_overviewView   = nullptr;
    NotebookView          *m_notebookView   = nullptr;

    // ── Central stacked widget ────────────────────────────────────────────────
    QStackedWidget        *m_stack          = nullptr;
};
