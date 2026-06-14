#include "MessageListWidget.h"

#include <QHeaderView>

#include "MessageTableModel.h"
#include "ui_MessageListWidget.h"

MessageListWidget::MessageListWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MessageListWidget)
    , m_model(new MessageTableModel(this))
{
    ui->setupUi(this);

    ui->btnColumns->setObjectName("iconBtn");

    ui->cmbChannel->addItem("All");
    ui->cmbView->addItem("Default");

    ui->msgTable->setModel(m_model);
    ui->msgTable->horizontalHeader()->setStretchLastSection(true);
    ui->msgTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    // Set fixed row height for performance
    ui->msgTable->verticalHeader()->setDefaultSectionSize(24);
    ui->msgTable->verticalHeader()->hide(); // Usually we hide row headers if we use our own ID/Index

    connect(ui->msgTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        &MessageListWidget::onSelectionChanged);
}

MessageListWidget::~MessageListWidget() { delete ui; }

void MessageListWidget::attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer) { m_model->setAnalyzer(analyzer); }

void MessageListWidget::populateFrom(const std::vector<fastrace::TraceMessage>& messages)
{
    // Legacy method. No longer used directly for UI population,
    // as attachAnalyzer does it lazy-loaded.
    // Kept here if still needed by some other parts, but we can leave it empty
    // or log a warning since we use MessageTableModel now.
}

void MessageListWidget::clearTable() { m_model->clear(); }

void MessageListWidget::onSelectionChanged()
{
    QModelIndexList selectedRows = ui->msgTable->selectionModel()->selectedRows();
    if (!selectedRows.isEmpty()) {
        int row = selectedRows.first().row();
        if (auto msgOpt = m_model->getMessage(row)) {
            emit messageSelected(*msgOpt);
        }
    }
}
