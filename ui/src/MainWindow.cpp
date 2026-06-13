#include "MainWindow.h"
#include "TopBarWidget.h"
#include "TimelineWidget.h"
#include "MessageListWidget.h"
#include "MessageDetailsWidget.h"
#include "DetectionsWidget.h"
#include "ScriptEditorWidget.h"
#include "OverviewView.h"
#include "NotebookView.h"
#include "Analyzer.h"

#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QFileInfo>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AutoTrace Analyzer");
    resize(1400, 900);

    // ── Top bar ───────────────────────────────────────────────────────────────
    m_topBar = new TopBarWidget;
    setMenuWidget(m_topBar);

    // ── Shared widgets ────────────────────────────────────────────────────────
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

    // ── Stacked widget ────────────────────────────────────────────────────────
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_overviewView);   // index 0
    m_stack->addWidget(m_notebookView);   // index 1

    setCentralWidget(m_stack);

    // ── Activate the default view ─────────────────────────────────────────────
    m_overviewView->activate();

    // ── Status bar setup ──────────────────────────────────────────────────────
    m_statusLabel  = new QLabel("No trace loaded");
    m_progressBar  = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);

    // Attach an opacity effect to m_statusLabel so we can animate it.
    auto *opacityEffect = new QGraphicsOpacityEffect(m_statusLabel);
    opacityEffect->setOpacity(1.0);
    m_statusLabel->setGraphicsEffect(opacityEffect);

    auto *sb = new QStatusBar;
    sb->addWidget(m_progressBar);
    sb->addWidget(m_statusLabel);
    sb->addPermanentWidget(new QLabel("Window: 10 s     Cursor: 00:00:00.000000"));
    setStatusBar(sb);

    // ── Async loading: future watcher ─────────────────────────────────────────
    m_watcher = new QFutureWatcher<void>(this);
    connect(m_watcher, &QFutureWatcher<void>::finished,
            this, &MainWindow::onLoadFinished);

    // ── Async loading: progress poll timer (100 ms) ───────────────────────────
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout,
            this, &MainWindow::onPollProgress);

    // ── Post-load fade-out: 2 s hold then 500 ms opacity animation ────────────
    m_fadeOutTimer = new QTimer(this);
    m_fadeOutTimer->setSingleShot(true);
    m_fadeOutTimer->setInterval(2000);
    connect(m_fadeOutTimer, &QTimer::timeout, this, [this]() {
        // Animate opacity from 1 → 0 over 500 ms on the status label.
        auto *effect = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
        if (!effect) return;

        if (m_fadeAnim) {
            m_fadeAnim->stop();
            m_fadeAnim->deleteLater();
        }
        m_fadeAnim = new QPropertyAnimation(effect, "opacity", this);
        m_fadeAnim->setDuration(500);
        m_fadeAnim->setStartValue(1.0);
        m_fadeAnim->setEndValue(0.0);
        m_fadeAnim->setEasingCurve(QEasingCurve::InQuad);
        connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
            // After fade: show just the base loaded label at full opacity.
            m_statusLabel->setText(m_loadedBaseLabel);
            auto *eff = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
            if (eff) eff->setOpacity(1.0);
        });
        m_fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        m_fadeAnim = nullptr;   // will self-delete
    });

    // ── Connect mode toggle ───────────────────────────────────────────────────
    connect(m_topBar, &TopBarWidget::modeChanged,
            this, &MainWindow::onModeChanged);

    // ── Connect trace file selection ──────────────────────────────────────────
    connect(m_topBar, &TopBarWidget::traceFileChanged,
            this, &MainWindow::onTraceFileChanged);
}

// ── Mode switch ───────────────────────────────────────────────────────────────
void MainWindow::onModeChanged(TopBarWidget::ViewMode mode)
{
    if (mode == TopBarWidget::ViewMode::Overview) {
        m_notebookView->deactivate();
        m_stack->setCurrentIndex(0);
        m_overviewView->activate();
    } else {
        m_overviewView->deactivate();
        m_stack->setCurrentIndex(1);
        m_notebookView->activate();
    }
}

// ── Trace file selected ───────────────────────────────────────────────────────
void MainWindow::onTraceFileChanged(const QString& path)
{
    cancelCurrentLoad();
    startLoad(path);
}

// ── Cancel any in-flight load ─────────────────────────────────────────────────
void MainWindow::cancelCurrentLoad()
{
    if (m_analyzer) {
        // Signal the producer loop and any blocked queue.push() to exit.
        m_analyzer->cancelled.store(true, std::memory_order_relaxed);
    }
    m_pollTimer->stop();
    m_fadeOutTimer->stop();
    // Don't wait for the future here — it will finish on its own thread.
    // m_watcher will still fire finished(), but onLoadFinished() checks cancelled.
}

// ── Reset rolling-average state ───────────────────────────────────────────────
void MainWindow::resetSpeedState()
{
    m_msgSamples.fill(0);
    m_sampleIdx    = 0;
    m_prevMsgCount = 0;
    m_samplesReady = false;
}

// ── Start a new async load ────────────────────────────────────────────────────
void MainWindow::startLoad(const QString& path)
{
    // Restore label opacity in case a fade was in progress.
    auto *effect = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
    if (effect) effect->setOpacity(1.0);

    // Clear both message lists immediately so stale data isn't shown.
    m_overviewView->messageList()->clearTable();
    m_notebookView->messageList()->clearTable();

    resetSpeedState();

    auto analyzer = std::make_shared<fastrace::Analyzer>();
    analyzer->collectMessages = true;
    m_analyzer = analyzer;          // store as current

    showLoadingState(QFileInfo(path).fileName());

    // Launch the blocking processFile on a thread pool thread.
    auto future = QtConcurrent::run([analyzer, path]() {
        analyzer->processFile(path.toStdString());
    });

    m_watcher->setFuture(future);
    m_pollTimer->start();
}

// ── Progress poll (100 ms timer) ──────────────────────────────────────────────
void MainWindow::onPollProgress()
{
    if (!m_analyzer) return;

    // ── Progress bar ─────────────────────────────────────────────────────────
    const size_t total = m_analyzer->totalBytes.load(std::memory_order_relaxed);
    const size_t read  = m_analyzer->bytesRead.load(std::memory_order_relaxed);
    if (total > 0)
        m_progressBar->setValue(static_cast<int>(read * 100 / total));

    // ── Rolling-average msg/s ─────────────────────────────────────────────────
    const size_t currentCount = m_analyzer->messagesCollected.load(std::memory_order_relaxed);
    const size_t delta = currentCount - m_prevMsgCount;
    m_prevMsgCount = currentCount;

    // Store delta (msgs in last 100 ms) into ring buffer.
    m_msgSamples[m_sampleIdx] = delta;
    m_sampleIdx = (m_sampleIdx + 1) % kSpeedSamples;
    if (m_sampleIdx == 0) m_samplesReady = true;

    // Compute rolling average only once the ring buffer has at least one full lap.
    size_t speedMsgPerSec = 0;
    if (m_samplesReady || m_sampleIdx > 0) {
        size_t sum = 0;
        const int filled = m_samplesReady ? kSpeedSamples : m_sampleIdx;
        for (int i = 0; i < filled; ++i) sum += m_msgSamples[i];
        // Each sample = 100 ms window → multiply by 10 to get per-second rate.
        speedMsgPerSec = (sum * 10) / static_cast<size_t>(filled);
    }

    // Update label: "Loading foo.blf…  12,450 msg/s"
    const QString speedStr = speedMsgPerSec > 0
        ? QString("  %L1 msg/s").arg(speedMsgPerSec)
        : QString();
    m_statusLabel->setText(m_loadingBaseLabel + speedStr);
}

// ── Load finished ─────────────────────────────────────────────────────────────
void MainWindow::onLoadFinished()
{
    m_pollTimer->stop();

    // If this was a cancelled load, discard the partial results.
    if (!m_analyzer || m_analyzer->cancelled.load(std::memory_order_relaxed)) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Load cancelled");
        return;
    }

    // Take a final speed snapshot before zeroing state.
    const size_t currentCount = m_analyzer->messagesCollected.load(std::memory_order_relaxed);
    const size_t finalDelta = currentCount - m_prevMsgCount;
    m_msgSamples[m_sampleIdx] = finalDelta;
    const int filled = m_samplesReady ? kSpeedSamples : (m_sampleIdx + 1);
    size_t sum = 0;
    for (int i = 0; i < filled; ++i) sum += m_msgSamples[i];
    const size_t finalSpeed = filled > 0 ? (sum * 10) / static_cast<size_t>(filled) : 0;

    // Populate both views from the shared Analyzer results.
    const auto& msgs = m_analyzer->messages;
    m_overviewView->messageList()->populateFrom(msgs);
    m_notebookView->messageList()->populateFrom(msgs);

    showLoadedState(msgs.size(), finalSpeed);
}

// ── Status bar helpers ────────────────────────────────────────────────────────
void MainWindow::showLoadingState(const QString& filename)
{
    m_loadingBaseLabel = QString("Loading %1…").arg(filename);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(m_loadingBaseLabel);

    // Restore full opacity in case a previous fade is still running.
    auto *effect = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
    if (effect) effect->setOpacity(1.0);
}

void MainWindow::showLoadedState(size_t messageCount, size_t finalSpeedMsgPerSec)
{
    m_progressBar->setVisible(false);

    m_loadedBaseLabel = QString("🟢 Trace loaded    %L1 messages").arg(messageCount);

    const QString speedStr = finalSpeedMsgPerSec > 0
        ? QString("  %L1 msg/s").arg(finalSpeedMsgPerSec)
        : QString();

    // Show "🟢 Trace loaded  N messages  X,XXX msg/s" immediately.
    m_statusLabel->setText(m_loadedBaseLabel + speedStr);

    // After 2 s, fade the msg/s portion out over 500 ms.
    m_fadeOutTimer->start();
}
