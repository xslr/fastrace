#include "TopBarWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>

#include <QSignalBlocker>

#include "ui_TopBarWidget.h"

static QString formatSize(int64_t bytes)
{
    if (bytes < 0) {
        return "?";
    }
    if (bytes < 1024LL * 1024) {
        return QString::number(bytes / 1024) + " KB";
    }
    if (bytes < 1024LL * 1024 * 1024) {
        return QString::number(bytes / (1024 * 1024)) + " MB";
    }
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

static QString formatDate(int64_t unixSec)
{
    if (unixSec == 0) {
        return "?";
    }
    return QDateTime::fromSecsSinceEpoch(unixSec).toString("yyyy-MM-dd");
}

TopBarWidget::TopBarWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::TopBarWidget)
    , m_recentFiles()
    , m_recentDbs(fastrace::RecentFiles::defaultDbPath() + ".dbs")
{
    ui->setupUi(this);
    setFixedHeight(46);
    ui->cmbTraceFile->setPlaceholderText(tr("Select trace (BLF, PCAPNG...)"));
    ui->cmbDatabase->setPlaceholderText(tr("Select database (ARXML, DBC…)"));

    connect(ui->cmbTraceFile, QOverload<int>::of(&QComboBox::activated), this, &TopBarWidget::onComboActivated);
    connect(ui->cmbDatabase, QOverload<int>::of(&QComboBox::activated), this, &TopBarWidget::onDbComboActivated);

    // Mode toggle
    connect(ui->btnOverview, &QPushButton::clicked, this, &TopBarWidget::onOverviewClicked);
    connect(ui->btnNotebook, &QPushButton::clicked, this, &TopBarWidget::onNotebookClicked);

    populateTraceCombo();
    populateDbCombo();

    // Restore last selected database (most recent entry)
    const auto recent = m_recentDbs.getRecent(1);
    if (!recent.empty()) {
        const QString lastPath = QString::fromStdString(recent[0].path);
        if (QFileInfo::exists(lastPath)) {
            QMetaObject::invokeMethod(this, [this, lastPath] { openDatabase(lastPath); }, Qt::QueuedConnection);
        }
    }
}

TopBarWidget::~TopBarWidget() { delete ui; }

void TopBarWidget::populateTraceCombo()
{
    QSignalBlocker blocker(ui->cmbTraceFile);
    ui->cmbTraceFile->clear();

    // Add Browse entry at index 0
    ui->cmbTraceFile->addItem(tr("📂 Browse…"), QString("::browse::"));

    for (const auto& entry : m_recentFiles.getRecent(10)) {
        const QString text = QString::fromStdString(entry.filename) + "  ·  " + formatSize(entry.sizeBytes) + "  ·  "
            + formatDate(entry.modTimeUnix);
        ui->cmbTraceFile->addItem(text, QString::fromStdString(entry.path));
    }
    ui->cmbTraceFile->setCurrentIndex(-1);
}

void TopBarWidget::populateDbCombo()
{
    QSignalBlocker blocker(ui->cmbDatabase);
    ui->cmbDatabase->clear();

    // Add Browse entry at index 0
    ui->cmbDatabase->addItem(tr("📂 Browse…"), QString("::browse::"));

    for (const auto& entry : m_recentDbs.getRecent(10)) {
        const QString text = QString::fromStdString(entry.filename) + "  ·  " + formatSize(entry.sizeBytes) + "  ·  "
            + formatDate(entry.modTimeUnix);
        ui->cmbDatabase->addItem(text, QString::fromStdString(entry.path));
    }
    ui->cmbDatabase->setCurrentIndex(-1);
}

void TopBarWidget::openTrace(const QString& path)
{
    m_recentFiles.addFile(path.toStdString());
    populateTraceCombo();

    // Select the entry we just added/bumped without triggering activated
    {
        QSignalBlocker blocker(ui->cmbTraceFile);
        const int idx = ui->cmbTraceFile->findData(path);
        if (idx >= 0) {
            ui->cmbTraceFile->setCurrentIndex(idx);
        }
    }

    emit traceFileChanged(path);
}

void TopBarWidget::openDatabase(const QString& path)
{
    m_recentDbs.addFile(path.toStdString());
    populateDbCombo();

    // Select the entry without triggering activated
    {
        QSignalBlocker blocker(ui->cmbDatabase);
        const int idx = ui->cmbDatabase->findData(path);
        if (idx >= 0) {
            ui->cmbDatabase->setCurrentIndex(idx);
        }
    }

    emit databaseSelectionChanged(path);
}

void TopBarWidget::onComboActivated(int index)
{
    if (index < 0) {
        return;
    }
    const QString path = ui->cmbTraceFile->itemData(index).toString();

    if (path == "::browse::") {
        const QString picked = QFileDialog::getOpenFileName(
            this, tr("Open Trace File"), QString(), tr("BLF Files (*.blf);;All Files (*)"));

        if (picked.isEmpty()) {
            // Reset to previous selection (or -1 if none)
            ui->cmbTraceFile->setCurrentIndex(-1); // Simplification, could be improved to remember previous
            return;
        }
        openTrace(picked);
    } else if (!path.isEmpty()) {
        openTrace(path);
    }
}

void TopBarWidget::onDbComboActivated(int index)
{
    if (index < 0) {
        return;
    }
    const QString path = ui->cmbDatabase->itemData(index).toString();

    if (path == "::browse::") {
        const QString picked = QFileDialog::getOpenFileName(
            this, tr("Open Signal Database"), QString(), tr("Database Files (*.arxml *.dbc);;All Files (*)"));

        if (picked.isEmpty()) {
            ui->cmbDatabase->setCurrentIndex(-1);
            return;
        }
        openDatabase(picked);
    } else if (!path.isEmpty()) {
        openDatabase(path);
    }
}

void TopBarWidget::onOverviewClicked()
{
    if (m_mode == ViewMode::Overview) {
        return;
    }
    m_mode = ViewMode::Overview;
    updateModeButtons();
    emit modeChanged(m_mode);
}

void TopBarWidget::onNotebookClicked()
{
    if (m_mode == ViewMode::Notebook) {
        return;
    }
    m_mode = ViewMode::Notebook;
    updateModeButtons();
    emit modeChanged(m_mode);
}

void TopBarWidget::setDatabaseComboEnabled(bool enabled) { ui->cmbDatabase->setEnabled(enabled); }

void TopBarWidget::setDbLoadProgress(float fraction)
{
    ui->dbLoadProgress->setValue(static_cast<int>(fraction * 100));
    ui->dbLoadProgress->setVisible(fraction < 1.0f);
}

void TopBarWidget::updateModeButtons()
{
    // Mark the active mode button with a property so QSS can style it
    ui->btnOverview->setProperty("active", m_mode == ViewMode::Overview);
    ui->btnNotebook->setProperty("active", m_mode == ViewMode::Notebook);
    // Force style re-evaluation
    ui->btnOverview->style()->unpolish(ui->btnOverview);
    ui->btnOverview->style()->polish(ui->btnOverview);
    ui->btnNotebook->style()->unpolish(ui->btnNotebook);
    ui->btnNotebook->style()->polish(ui->btnNotebook);
}
