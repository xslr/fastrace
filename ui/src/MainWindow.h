#pragma once

#include <QMainWindow>

class TopBarWidget;
class LeftPanelWidget;
class TimelineWidget;
class MessageListWidget;
class MessageDetailsWidget;
class ScriptEditorWidget;
class AnalyzerPreviewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    TopBarWidget          *m_topBar        = nullptr;
    LeftPanelWidget       *m_leftPanel     = nullptr;
    TimelineWidget        *m_timeline      = nullptr;
    MessageListWidget     *m_messageList   = nullptr;
    MessageDetailsWidget  *m_messageDetails = nullptr;
    ScriptEditorWidget    *m_scriptEditor  = nullptr;
    AnalyzerPreviewWidget *m_preview       = nullptr;
};
