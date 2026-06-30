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

void MessageListWidget::scrollToMessage(size_t index)
{
    if (!m_model) {
        return;
    }
    if (static_cast<int>(index) >= m_model->rowCount()) {
        return;
    }

    QModelIndex modelIndex = m_model->index(static_cast<int>(index), 0);
    ui->msgTable->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
    ui->msgTable->selectRow(static_cast<int>(index));
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
