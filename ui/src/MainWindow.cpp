#include "MainWindow.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "Analyzer.h"
#include "DetectionEngine.h"
#include "DetectionsWidget.h"
#include "MessageDetailsWidget.h"
#include "MessageListWidget.h"
#include "NotebookView.h"
#include "OverviewView.h"
#include "ScriptEditorWidget.h"
#include "TimelineOverviewWidget.h"
#include "TimelineWidget.h"
#include "TopBarWidget.h"
#include "detectors/DoipDetector.h"
#include "detectors/PduDetector.h"
#include "detectors/SomeIpSdDetector.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("AutoTrace Analyzer");
    resize(1400, 900);

    // ── Top bar ───────────────────────────────────────────────────────────────
    m_topBar = new TopBarWidget;
    setMenuWidget(m_topBar);

    // ── Shared widgets ────────────────────────────────────────────────────────
    m_timelineOverview = new TimelineOverviewWidget(this);
    m_timeline = new TimelineWidget;

    // ── Overview-exclusive widgets ────────────────────────────────────────────
    m_messageDetails = new MessageDetailsWidget;
    m_detections = new DetectionsWidget;

    // ── Notebook-exclusive widgets ────────────────────────────────────────────
    m_scriptEditor = new ScriptEditorWidget;

    // ── Construct the two views ───────────────────────────────────────────────
    m_overviewView = new OverviewView(m_timelineOverview, m_timeline, m_messageDetails, m_detections);

    m_notebookView = new NotebookView(m_timelineOverview, m_timeline, m_scriptEditor);

    // ── Stacked widget ────────────────────────────────────────────────────────
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_overviewView); // index 0
    m_stack->addWidget(m_notebookView); // index 1

    setCentralWidget(m_stack);

    // ── Activate the default view ─────────────────────────────────────────────
    m_overviewView->activate();

    // ── Status bar setup ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel("No trace loaded");
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);

    // Attach an opacity effect to m_statusLabel so we can animate it.
    auto* opacityEffect = new QGraphicsOpacityEffect(m_statusLabel);
    opacityEffect->setOpacity(1.0);
    m_statusLabel->setGraphicsEffect(opacityEffect);

    auto* sb = new QStatusBar;
    sb->addWidget(m_progressBar);
    sb->addWidget(m_statusLabel);

    m_timeBoundsLabel = new QLabel("");
    m_timeBoundsLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_timeBoundsLabel, &QWidget::customContextMenuRequested, this, &MainWindow::onTimeBoundsContextMenu);
    sb->addPermanentWidget(m_timeBoundsLabel);

    setStatusBar(sb);

    // ── Async loading: future watcher ─────────────────────────────────────────
    m_watcher = new QFutureWatcher<void>(this);
    connect(m_watcher, &QFutureWatcher<void>::finished, this, &MainWindow::onLoadFinished);

    // ── Async loading: progress poll timer (100 ms) ───────────────────────────
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::onPollProgress);

    // ── Post-load fade-out: 2 s hold then 500 ms opacity animation ────────────
    m_fadeOutTimer = new QTimer(this);
    m_fadeOutTimer->setSingleShot(true);
    m_fadeOutTimer->setInterval(2000);
    connect(m_fadeOutTimer, &QTimer::timeout, this, [this]() {
        // Animate opacity from 1 → 0 over 500 ms on the status label.
        auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
        if (!effect) {
            return;
        }

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
            auto* eff = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
            if (eff) {
                eff->setOpacity(1.0);
            }
        });
        m_fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        m_fadeAnim = nullptr; // will self-delete
    });

    // ── DB loading: future watcher + poll timer ───────────────────────────────
    m_dbWatcher = new QFutureWatcher<void>(this);
    connect(m_dbWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onDbLoadFinished);

    m_dbPollTimer = new QTimer(this);
    m_dbPollTimer->setInterval(100);
    connect(m_dbPollTimer, &QTimer::timeout, this, &MainWindow::onPollDbProgress);

    // ── Create bare analyzer so timeline/DB widgets work before trace is loaded
    m_analyzer = std::make_shared<fastrace::Analyzer>();
    m_timeline->attachAnalyzer(m_analyzer);

    // ── Detection: future watcher + poll timer ───────────────────────────────
    m_detectionWatcher = new QFutureWatcher<void>(this);
    connect(m_detectionWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onDetectionFinished);

    m_detectionPollTimer = new QTimer(this);
    m_detectionPollTimer->setInterval(100);
    connect(m_detectionPollTimer, &QTimer::timeout, this, &MainWindow::onPollDetectionProgress);

    m_continuousDetectionTimer = new QTimer(this);
    m_continuousDetectionTimer->setInterval(1000);
    connect(m_continuousDetectionTimer, &QTimer::timeout, this, &MainWindow::onContinuousDetectionTimer);

    // ── Signal connections ────────────────────────────────────────────────────
    connect(m_topBar, &TopBarWidget::modeChanged, this, &MainWindow::onModeChanged);
    connect(m_topBar, &TopBarWidget::traceFileChanged, this, &MainWindow::onTraceFileChanged);
    connect(m_topBar, &TopBarWidget::databaseSelectionChanged, this, &MainWindow::onDatabaseSelectionChanged);
    connect(m_topBar, &TopBarWidget::runDetectorsRequested, this, &MainWindow::runDetectors);
    connect(m_topBar, &TopBarWidget::cancelDetectionRequested, this, &MainWindow::cancelDetectors);
    connect(m_topBar, &TopBarWidget::continuousDetectionToggled, this, &MainWindow::onContinuousDetectionToggled);

    connect(m_detections, &DetectionsWidget::detectionSelected, m_overviewView->messageList(),
        &MessageListWidget::scrollToMessage);

    connect(m_timeline, &TimelineWidget::signalsChanged, this,
        [](const QStringList& names) { QSettings().setValue("timeline/selectedSignals", names); });

    connect(m_timelineOverview, &TimelineOverviewWidget::navigateRequested, this, [](uint64_t timestampUs) {
        // spdlog::info("TimelineOverviewWidget requested navigation to {} us", timestampUs);
        // TODO: m_timeline->setCursor(timestampUs);
    });
}

// ── Mode switch
// ───────────────────────────────────────────────────────────────
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

// ── Trace file selected
// ───────────────────────────────────────────────────────
void MainWindow::onTraceFileChanged(const QString& path)
{
    cancelCurrentLoad();
    startLoad(path);
}

// ── Cancel any ongoing load
// ─────────────────────────────────────────────────
void MainWindow::cancelCurrentLoad()
{
    if (m_analyzer) {
        // Signal the producer loop and any blocked queue.push() to exit.
        m_analyzer->cancelled.store(true, std::memory_order_relaxed);
    }
    m_pollTimer->stop();
    m_fadeOutTimer->stop();
    // Don't wait for the future here — it will finish on its own thread.
    // m_watcher will still fire finished(), but onLoadFinished() checks
    // cancelled.
}

// ── Reset rolling-average state
// ───────────────────────────────────────────────
void MainWindow::resetSpeedState()
{
    m_msgSamples.fill(0);
    m_sampleIdx = 0;
    m_prevMsgCount = 0;
    m_samplesReady = false;
}

// ── Start a new async load
// ────────────────────────────────────────────────────
void MainWindow::startLoad(const QString& path)
{
    // Cancel any ongoing DB load on the current analyzer before replacing it.
    if (m_dbWatcher->isRunning() && m_analyzer) {
        m_analyzer->dbLoadCancelled.store(true, std::memory_order_relaxed);
        m_dbWatcher->waitForFinished();
        m_dbPollTimer->stop();
        m_topBar->setDatabaseComboEnabled(true);
    }

    // Restore label opacity in case a fade was in progress.
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
    if (effect) {
        effect->setOpacity(1.0);
    }

    // Clear both message lists immediately so stale data isn't shown.
    m_overviewView->messageList()->clearTable();
    m_notebookView->messageList()->clearTable();
    m_detections->setDetections({});

    resetSpeedState();

    m_topBar->setTraceComboEnabled(false);

    m_analyzer->reset();
    m_analyzer->collectMessages = false; // We use lazy loading now

    showLoadingState(QFileInfo(path).fileName());

    // Launch the index builder on a thread pool thread.
    auto future = QtConcurrent::run([analyzer = m_analyzer, path]() { analyzer->buildIndex(path.toStdString()); });

    m_watcher->setFuture(future);
    m_pollTimer->start();
}

// ── Progress poll (100 ms timer)
// ──────────────────────────────────────────────
void MainWindow::onPollProgress()
{
    if (!m_analyzer) {
        return;
    }

    // ── Progress bar ─────────────────────────────────────────────────────────
    const size_t total = m_analyzer->totalBytes.load(std::memory_order_relaxed);
    const size_t read = m_analyzer->bytesRead.load(std::memory_order_relaxed);
    if (total > 0) {
        m_progressBar->setValue(static_cast<int>(read * 100 / total));
    }

    // ── Rolling-average msg/s ─────────────────────────────────────────────────
    const size_t currentCount = m_analyzer->messagesCollected.load(std::memory_order_relaxed);
    const size_t delta = currentCount - m_prevMsgCount;
    m_prevMsgCount = currentCount;

    // Store delta (msgs in last 100 ms) into ring buffer.
    m_msgSamples[m_sampleIdx] = delta;
    m_sampleIdx = (m_sampleIdx + 1) % kSpeedSamples;
    if (m_sampleIdx == 0) {
        m_samplesReady = true;
    }

    // Compute rolling average only once the ring buffer has at least one full
    // lap.
    size_t speedMsgPerSec = 0;
    if (m_samplesReady || m_sampleIdx > 0) {
        size_t sum = 0;
        const int filled = m_samplesReady ? kSpeedSamples : m_sampleIdx;
        for (int i = 0; i < filled; ++i) {
            sum += m_msgSamples[i];
        }
        // Each sample = 100 ms window → multiply by 10 to get per-second rate.
        speedMsgPerSec = (sum * 10) / static_cast<size_t>(filled);
    }

    // Update label: "Loading foo.blf…  12,450 msg/s"
    const QString speedStr = speedMsgPerSec > 0 ? QString("  %L1 msg/s").arg(speedMsgPerSec) : QString();
    m_statusLabel->setText(m_loadingBaseLabel + speedStr);
}

// ── Trace load finished
// ─────────────────────────────────────────────────────────────
void MainWindow::onLoadFinished()
{
    m_pollTimer->stop();
    m_topBar->setTraceComboEnabled(true);

    // If this was a cancelled load, discard the partial results.
    if (!m_analyzer || m_analyzer->cancelled.load(std::memory_order_relaxed)) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Load cancelled");
        return;
    }

    if (m_analyzer->totalMessages() == 0) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Failed to load trace or file is empty");
        return;
    }

    // Take a final speed snapshot before zeroing state.
    const size_t currentCount = m_analyzer->messagesCollected.load(std::memory_order_relaxed);
    const size_t finalDelta = currentCount - m_prevMsgCount;
    m_msgSamples[m_sampleIdx] = finalDelta;
    const int filled = m_samplesReady ? kSpeedSamples : (m_sampleIdx + 1);
    size_t sum = 0;
    for (int i = 0; i < filled; ++i) {
        sum += m_msgSamples[i];
    }
    const size_t finalSpeed = filled > 0 ? (sum * 10) / static_cast<size_t>(filled) : 0;

    // Attach the shared Analyzer to the message lists for lazy loading.
    m_overviewView->messageList()->attachAnalyzer(m_analyzer);
    m_notebookView->messageList()->attachAnalyzer(m_analyzer);
    m_timelineOverview->attachAnalyzer(m_analyzer);
    m_timeline->attachAnalyzer(m_analyzer);
    m_messageDetails->attachAnalyzer(m_analyzer);

    // Reload the signal database into the new analyzer (if one was selected).
    if (!m_currentDbPath.isEmpty()) {
        onDatabaseSelectionChanged(m_currentDbPath);
    }

    updateTimeBoundsLabel();

    showLoadedState(m_analyzer->totalMessages(), finalSpeed);
}

// ── Status bar helpers
// ────────────────────────────────────────────────────────
void MainWindow::showLoadingState(const QString& filename)
{
    m_loadingBaseLabel = QString("Loading %1…").arg(filename);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(m_loadingBaseLabel);

    // Restore full opacity in case a previous fade is still running.
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_statusLabel->graphicsEffect());
    if (effect) {
        effect->setOpacity(1.0);
    }
}

void MainWindow::showLoadedState(size_t messageCount, size_t finalSpeedMsgPerSec)
{
    m_progressBar->setVisible(false);

    m_loadedBaseLabel = QString("🟢 Trace loaded    %L1 messages").arg(messageCount);

    const QString speedStr = finalSpeedMsgPerSec > 0 ? QString("  %L1 msg/s").arg(finalSpeedMsgPerSec) : QString();

    // Show "🟢 Trace loaded  N messages  X,XXX msg/s" immediately.
    m_statusLabel->setText(m_loadedBaseLabel + speedStr);

    // After 2 s, fade the msg/s portion out over 500 ms.
    m_fadeOutTimer->start();
}

void MainWindow::onTimeBoundsContextMenu(const QPoint& /*pos*/)
{
    m_absoluteTimestamps = !m_absoluteTimestamps;
    updateTimeBoundsLabel();
}

void MainWindow::onDatabaseSelectionChanged(const QString& path)
{
    m_currentDbPath = path;

    if (path.isEmpty()) {
        m_analyzer->clearDatabase();
        m_messageDetails->refreshSignalDecode();
        return;
    }

    if (m_dbWatcher->isRunning()) {
        m_analyzer->dbLoadCancelled.store(true, std::memory_order_relaxed);
        m_dbWatcher->waitForFinished();
    }
    m_analyzer->dbLoadCancelled.store(false, std::memory_order_relaxed);

    m_topBar->setDatabaseComboEnabled(false);
    m_topBar->setDbLoadProgress(0.0f);
    m_dbPollTimer->start();

    const std::string stdPath = path.toStdString();
    auto future = QtConcurrent::run([analyzer = m_analyzer, stdPath]() { analyzer->loadDatabase(stdPath); });
    m_dbWatcher->setFuture(future);
}

void MainWindow::onDbLoadFinished()
{
    m_dbPollTimer->stop();
    m_topBar->setDbLoadProgress(1.0f);
    m_topBar->setDatabaseComboEnabled(true);
    m_messageDetails->refreshSignalDecode();

    const QStringList saved = QSettings().value("timeline/selectedSignals").toStringList();
    if (!saved.isEmpty()) {
        m_timeline->restoreSignals(saved);
    }
}

void MainWindow::onPollDbProgress()
{
    if (!m_analyzer) {
        return;
    }
    float progress = m_analyzer->dbLoadProgress.load(std::memory_order_relaxed);
    m_topBar->setDbLoadProgress(progress);
}

void MainWindow::updateTimeBoundsLabel()
{
    if (!m_analyzer || m_analyzer->histogram().traceEndUs <= m_analyzer->histogram().traceStartUs) {
        m_timeBoundsLabel->setText("");
        return;
    }

    const auto& hist = m_analyzer->histogram();

    auto formatTs = [](uint64_t us) -> QString {
        const int h = static_cast<int>(us / 3'600'000'000ull);
        const int m = static_cast<int>((us % 3'600'000'000ull) / 60'000'000ull);
        const int s = static_cast<int>((us % 60'000'000ull) / 1'000'000ull);
        const int us6 = static_cast<int>(us % 1'000'000ull);
        return QString("%1:%2:%3.%4")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'))
            .arg(us6, 6, 10, QChar('0'));
    };

    if (m_absoluteTimestamps) {
        m_timeBoundsLabel->setText(
            QString("Start: %1     End: %2").arg(formatTs(hist.traceStartUs)).arg(formatTs(hist.traceEndUs)));
    } else {
        m_timeBoundsLabel->setText(
            QString("Start: 00:00:00.000000     End: %1").arg(formatTs(hist.traceEndUs - hist.traceStartUs)));
    }
}

void MainWindow::runDetectors()
{
    if (!m_analyzer || m_analyzer->totalMessages() == 0) {
        return;
    }

    m_lastProcessedChunk = 0;
    m_detections->setDetections({});
    m_detectionCancelled.store(false, std::memory_order_relaxed);
    m_detectionChunksProcessed.store(0, std::memory_order_relaxed);

    m_detectionEngine = std::make_shared<DetectionEngine>();
    const fastrace::ArDatabase* db = nullptr;
    if (!m_currentDbPath.isEmpty()) {
        db = &m_analyzer->arDatabase();
    }

    m_detectionEngine->addDetector(std::make_unique<PduDetector>(db));
    m_detectionEngine->addDetector(std::make_unique<SomeIpSdDetector>());
    m_detectionEngine->addDetector(std::make_unique<DoipDetector>(db));

    m_topBar->setDetectionRunning(true);
    m_detectionPollTimer->start();

    auto future = QtConcurrent::run([engine = m_detectionEngine, analyzer = m_analyzer, db, this]() {
        engine->run(analyzer.get(), db, m_detectionCancelled, m_detectionChunksProcessed);
    });

    m_detectionWatcher->setFuture(future);
}

void MainWindow::cancelDetectors()
{
    m_detectionCancelled.store(true, std::memory_order_relaxed);
    m_detectionPollTimer->stop();
    m_topBar->setDetectionRunning(false);
}

void MainWindow::onDetectionFinished()
{
    m_detectionPollTimer->stop();
    m_topBar->setDetectionRunning(false);

    if (m_analyzer && !m_detectionCancelled.load(std::memory_order_relaxed)) {
        m_lastProcessedChunk = m_analyzer->getChunkIndex().size();
    }

    if (m_detectionCancelled.load(std::memory_order_relaxed)) {
        return;
    }

    m_detections->setDetections(m_detectionEngine->getResults());
}

void MainWindow::onPollDetectionProgress()
{
    if (!m_detectionEngine) {
        return;
    }
    size_t processed = m_detectionChunksProcessed.load(std::memory_order_relaxed);
    size_t total = m_detectionEngine->chunkCount();
    if (total == 0 && m_analyzer) {
        total = m_analyzer->getChunkIndex().size();
    }

    m_topBar->setDetectionProgress(processed, total);
}

void MainWindow::onContinuousDetectionToggled(bool enabled)
{
    if (enabled && m_analyzer && m_analyzer->totalMessages() > 0) {
        if (!m_detectionEngine) {
            runDetectors();
        }
        m_continuousDetectionTimer->start();
    } else {
        m_continuousDetectionTimer->stop();
    }
}

void MainWindow::onContinuousDetectionTimer()
{
    if (!m_analyzer || !m_detectionEngine) {
        return;
    }
    if (m_detectionWatcher->isRunning()) {
        return; // Still processing
    }

    size_t currentChunks = m_analyzer->getChunkIndex().size();
    if (currentChunks > m_lastProcessedChunk) {
        m_detectionCancelled.store(false, std::memory_order_relaxed);
        m_detectionChunksProcessed.store(0, std::memory_order_relaxed);

        m_topBar->setDetectionRunning(true);
        m_detectionPollTimer->start();

        const fastrace::ArDatabase* db = m_currentDbPath.isEmpty() ? nullptr : &m_analyzer->arDatabase();

        auto future = QtConcurrent::run(
            [engine = m_detectionEngine, analyzer = m_analyzer, db, startChunk = m_lastProcessedChunk, this]() {
                engine->run(analyzer.get(), db, m_detectionCancelled, m_detectionChunksProcessed, startChunk);
            });

        m_detectionWatcher->setFuture(future);
    }
}
