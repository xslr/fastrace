#include "TopBarWidget.h"
#include "ui_TopBarWidget.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include "DatabaseComboBox.h"

class CheckboxItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Don't draw checkbox for the Browse... entry
        if (index.row() == 0 && index.data(Qt::UserRole).toString() == "::browse::") {
            QStyledItemDelegate::paint(painter, opt, index);
            return;
        }

        // Draw the background and text
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        // Draw the checkbox
        QStyleOptionButton checkBoxOption;
        QRect checkBoxRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, opt.widget);
        
        checkBoxOption.rect = checkBoxRect;
        checkBoxOption.state = QStyle::State_Enabled;
        if (index.data(Qt::CheckStateRole).value<int>() == Qt::Checked)
            checkBoxOption.state |= QStyle::State_On;
        else
            checkBoxOption.state |= QStyle::State_Off;

        style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &checkBoxOption, painter, opt.widget);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override {
        if (index.row() == 0 && index.data(Qt::UserRole).toString() == "::browse::") {
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            QStyle *style = option.widget ? option.widget->style() : QApplication::style();
            QRect checkBoxRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, option.widget);
            
            if (checkBoxRect.contains(mouseEvent->pos()) || option.rect.contains(mouseEvent->pos())) {
                int state = index.data(Qt::CheckStateRole).value<int>();
                model->setData(index, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
                if (option.widget) const_cast<QWidget*>(option.widget)->update();
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};
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
    , m_recentDbs(fastrace::RecentFiles::defaultDbPath() + ".dbs")
{
    ui->setupUi(this);
    setFixedHeight(46);
    ui->cmbTraceFile->setPlaceholderText(tr("Select trace (BLF, PCAPNG...)"));
    ui->cmbDatabase->setPlaceholderText(tr("Signal databases..."));

    // Install delegate and disable auto-close on select
    ui->cmbDatabase->setItemDelegate(new CheckboxItemDelegate(this));
    ui->cmbDatabase->view()->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->cmbTraceFile, QOverload<int>::of(&QComboBox::activated),
            this, &TopBarWidget::onComboActivated);
    connect(ui->cmbDatabase, QOverload<int>::of(&QComboBox::activated),
            this, &TopBarWidget::onDbComboActivated);

    // Mode toggle
    connect(ui->btnOverview,  &QPushButton::clicked, this, &TopBarWidget::onOverviewClicked);
    connect(ui->btnNotebook,  &QPushButton::clicked, this, &TopBarWidget::onNotebookClicked);

    populateTraceCombo();
    populateDbCombo();

    // Restore persisted active paths
    for (const auto& p : m_recentDbs.getActivePaths()) {
        m_activeDbPaths.insert(QString::fromStdString(p));
    }
    // Re-populate so check states reflect restored active set
    populateDbCombo();
    updateDbComboDisplay();
    updateModeButtons();

    // Emit immediately so any future consumer knows the initial DB set
    QMetaObject::invokeMethod(this, [this]{ emitDbSelectionChanged(); },
                              Qt::QueuedConnection);
}

TopBarWidget::~TopBarWidget()
{
    delete ui;
}

void TopBarWidget::populateTraceCombo()
{
    QSignalBlocker blocker(ui->cmbTraceFile);
    ui->cmbTraceFile->clear();
    
    // Add Browse entry at index 0
    ui->cmbTraceFile->addItem(tr("📂 Browse…"), QString("::browse::"));
    
    for (const auto& entry : m_recentFiles.getRecent(10)) {
        const QString text = QString::fromStdString(entry.filename)
                           + "  ·  " + formatSize(entry.sizeBytes)
                           + "  ·  " + formatDate(entry.modTimeUnix);
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
        const QString path = QString::fromStdString(entry.path);
        ui->cmbDatabase->addItem(QString::fromStdString(entry.filename), path);
        
        int row = ui->cmbDatabase->count() - 1;
        ui->cmbDatabase->setItemData(row, m_activeDbPaths.contains(path) ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
    }
    ui->cmbDatabase->setCurrentIndex(-1);
}

void TopBarWidget::emitDbSelectionChanged()
{
    QStringList paths;
    for (int i = 1; i < ui->cmbDatabase->count(); ++i) {
        if (ui->cmbDatabase->itemData(i, Qt::CheckStateRole).value<int>() == Qt::Checked) {
            paths << ui->cmbDatabase->itemData(i).toString();
        }
    }
    emit databaseSelectionChanged(paths);
}

void TopBarWidget::updateDbComboDisplay()
{
    if (auto *dbCombo = qobject_cast<DatabaseComboBox*>(ui->cmbDatabase)) {
        dbCombo->setActiveCount(static_cast<int>(m_activeDbPaths.size()));
    }
}

void TopBarWidget::openTrace(const QString& path)
{
    m_recentFiles.addFile(path.toStdString());
    populateTraceCombo();

    // Select the entry we just added/bumped without triggering activated
    {
        QSignalBlocker blocker(ui->cmbTraceFile);
        const int idx = ui->cmbTraceFile->findData(path);
        if (idx >= 0) ui->cmbTraceFile->setCurrentIndex(idx);
    }

    emit traceFileChanged(path);
}

void TopBarWidget::onComboActivated(int index)
{
    if (index < 0) return;
    const QString path = ui->cmbTraceFile->itemData(index).toString();
    
    if (path == "::browse::") {
        const QString picked = QFileDialog::getOpenFileName(
            this,
            tr("Open Trace File"),
            QString(),
            tr("BLF Files (*.blf);;All Files (*)"));
            
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
    if (index < 0) return;
    const QString path = ui->cmbDatabase->itemData(index).toString();

    if (path == "::browse::") {
        const QStringList picked = QFileDialog::getOpenFileNames(
            this,
            tr("Open Signal Database"),
            QString(),
            tr("Database Files (*.arxml *.dbc);;All Files (*)"));
            
        if (picked.isEmpty()) {
            ui->cmbDatabase->setCurrentIndex(-1);
            return;
        }

        for (const auto& p : picked) {
            m_recentDbs.addFile(p.toStdString());
            m_recentDbs.setActive(p.toStdString(), true);
            m_activeDbPaths.insert(p);
        }

        populateDbCombo();
        updateDbComboDisplay();
        emitDbSelectionChanged();

        // Show combo again after dialog closes (optional, but good UX)
        ui->cmbDatabase->showPopup();
    } else {
        // Toggle check state for the activated item
        int state = ui->cmbDatabase->itemData(index, Qt::CheckStateRole).value<int>();
        int newState = state == Qt::Checked ? Qt::Unchecked : Qt::Checked;
        ui->cmbDatabase->setItemData(index, newState, Qt::CheckStateRole);

        if (newState == Qt::Checked) {
            m_activeDbPaths.insert(path);
            m_recentDbs.setActive(path.toStdString(), true);
        } else {
            m_activeDbPaths.remove(path);
            m_recentDbs.setActive(path.toStdString(), false);
        }

        updateDbComboDisplay();
        emitDbSelectionChanged();
    }
}

void TopBarWidget::onOverviewClicked()
{
    if (m_mode == ViewMode::Overview) return;
    m_mode = ViewMode::Overview;
    updateModeButtons();
    emit modeChanged(m_mode);
}

void TopBarWidget::onNotebookClicked()
{
    if (m_mode == ViewMode::Notebook) return;
    m_mode = ViewMode::Notebook;
    updateModeButtons();
    emit modeChanged(m_mode);
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
