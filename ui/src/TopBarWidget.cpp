#include "TopBarWidget.h"
#include "ui_TopBarWidget.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QSignalBlocker>

static QString formatSize(int64_t bytes) {
    if (bytes < 0) return "?";
    if (bytes < 1024LL * 1024)
        return QString::number(bytes / 1024) + " KB";
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024 * 1024)) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

static QString formatDate(int64_t unixSec) {
    if (unixSec == 0) return "?";
    return QDateTime::fromSecsSinceEpoch(unixSec).toString("yyyy-MM-dd");
}

TopBarWidget::TopBarWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TopBarWidget)
    , m_recentFiles()
{
    ui->setupUi(this);
    setFixedHeight(54);
    ui->cmbTraceFile->setPlaceholderText(tr("No file loaded"));

    connect(ui->btnOpen, &QPushButton::clicked, this, &TopBarWidget::onBtnOpenClicked);
    connect(ui->cmbTraceFile, QOverload<int>::of(&QComboBox::activated),
            this, &TopBarWidget::onComboActivated);

    populateRecentCombo();
}

TopBarWidget::~TopBarWidget()
{
    delete ui;
}

void TopBarWidget::populateRecentCombo()
{
    QSignalBlocker blocker(ui->cmbTraceFile);
    ui->cmbTraceFile->clear();
    for (const auto& entry : m_recentFiles.getRecent(10)) {
        const QString text = QString::fromStdString(entry.filename)
                           + "  ·  " + formatSize(entry.sizeBytes)
                           + "  ·  " + formatDate(entry.modTimeUnix);
        ui->cmbTraceFile->addItem(text, QString::fromStdString(entry.path));
    }
    ui->cmbTraceFile->setCurrentIndex(-1);
}

void TopBarWidget::openTrace(const QString& path)
{
    m_recentFiles.addFile(path.toStdString());
    populateRecentCombo();

    // Select the entry we just added/bumped without triggering activated
    {
        QSignalBlocker blocker(ui->cmbTraceFile);
        const int idx = ui->cmbTraceFile->findData(path);
        if (idx >= 0) ui->cmbTraceFile->setCurrentIndex(idx);
    }

    emit traceFileChanged(path);
}

void TopBarWidget::onBtnOpenClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Trace File"),
        QString(),
        tr("BLF Files (*.blf);;All Files (*)"));
    if (path.isEmpty()) return;
    openTrace(path);
}

void TopBarWidget::onComboActivated(int index)
{
    if (index < 0) return;
    const QString path = ui->cmbTraceFile->itemData(index).toString();
    if (!path.isEmpty())
        openTrace(path);
}
