#pragma once

#include <QFutureWatcher>
#include <QMainWindow>
#include <array>
#include <memory>

#include "TopBarWidget.h" // for ViewMode

namespace fastrace {
class Analyzer;
}

class TopBarWidget;
class TimelineOverviewWidget;
class TimelineWidget;
class MessageListWidget;
class MessageDetailsWidget;
class ScriptEditorWidget;
class DetectionsWidget;
class OverviewView;
class NotebookView;

class QStackedWidget;
class QProgressBar;
class QLabel;
class QTimer;
class QPropertyAnimation;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onModeChanged(TopBarWidget::ViewMode mode);
    void onTraceFileChanged(const QString& path);
    void onLoadFinished();
    void onPollProgress();
    void onTimeBoundsContextMenu(const QPoint& pos);

private:
    void startLoad(const QString& path);
    void cancelCurrentLoad();
    void resetSpeedState();
    void showLoadingState(const QString& filename);
    void showLoadedState(size_t messageCount, size_t finalSpeedMsgPerSec);
    void updateTimeBoundsLabel();

    // ── Shared widgets ────────────────────────────────────────────────────────
    TopBarWidget* m_topBar = nullptr;
    TimelineOverviewWidget* m_timelineOverview = nullptr;
    TimelineWidget* m_timeline = nullptr;

    // ── Overview-exclusive widgets ────────────────────────────────────────────
    MessageDetailsWidget* m_messageDetails = nullptr;
    DetectionsWidget* m_detections = nullptr;

    // ── Notebook-exclusive widgets ────────────────────────────────────────────
    ScriptEditorWidget* m_scriptEditor = nullptr;

    // ── Views ─────────────────────────────────────────────────────────────────
    OverviewView* m_overviewView = nullptr;
    NotebookView* m_notebookView = nullptr;

    // ── Central stacked widget ────────────────────────────────────────────────
    QStackedWidget* m_stack = nullptr;

    // ── Status bar widgets ────────────────────────────────────────────────────
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_timeBoundsLabel = nullptr;
    bool m_absoluteTimestamps = false;

    // ── Async loading ─────────────────────────────────────────────────────────
    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    QFutureWatcher<void>* m_watcher = nullptr;
    QTimer* m_pollTimer = nullptr;

    // ── Msg/s rolling average (N=5 samples at 100 ms each) ───────────────────
    static constexpr int kSpeedSamples = 5;
    std::array<size_t, kSpeedSamples> m_msgSamples {};
    int m_sampleIdx = 0;
    size_t m_prevMsgCount = 0;
    bool m_samplesReady = false; ///< true once ring buffer is full

    // ── Post-load speed fade-out ──────────────────────────────────────────────
    QTimer* m_fadeOutTimer = nullptr; ///< 2 s single-shot after load done
    QPropertyAnimation* m_fadeAnim = nullptr; ///< 500 ms opacity animation
    QString m_loadedBaseLabel; ///< cached "🟢 … messages" text
    QString m_loadingBaseLabel; ///< cached "Loading foo.blf…" text
};
