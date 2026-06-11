#include "LeftPanelWidget.h"
#include "ui_LeftPanelWidget.h"
#include "ArxmlParser.h"
#include <QFileDialog>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <filesystem>
#include <map>

LeftPanelWidget::LeftPanelWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LeftPanelWidget)
    , m_signalDbs()
{
    ui->setupUi(this);

    ui->traceHeader->setObjectName("headerLabel");

    ui->traceSummary->horizontalHeader()->setVisible(false);
    ui->traceSummary->verticalHeader()->setVisible(false);
    ui->traceSummary->setStyleSheet("background-color: transparent; border: none;");
    ui->traceSummary->setFixedHeight(130);
    ui->traceSummary->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->traceSummary->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui->busTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->busTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui->sigTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->serviceTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->serviceTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(ui->btnAddDb,    &QPushButton::clicked, this, &LeftPanelWidget::onBtnAddDbClicked);
    connect(ui->btnRemoveDb, &QPushButton::clicked, this, &LeftPanelWidget::onBtnRemoveDbClicked);

    populateTraceSummary();
    loadDatabases();
}

LeftPanelWidget::~LeftPanelWidget()
{
    delete ui;
}

void LeftPanelWidget::onBtnAddDbClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open ARXML Signal Database"),
        QString(),
        tr("ARXML Files (*.arxml);;All Files (*)"));
    if (path.isEmpty()) return;

    m_signalDbs.addDatabase(path.toStdString());
    loadDatabases();
}

void LeftPanelWidget::onBtnRemoveDbClicked()
{
    auto* item = ui->lstDatabases->currentItem();
    if (!item) return;

    const std::string path = item->data(Qt::UserRole).toString().toStdString();
    m_signalDbs.removeDatabase(path);
    loadDatabases();
}

void LeftPanelWidget::loadDatabases()
{
    const auto paths = m_signalDbs.getActiveDatabases();
    m_arxmlDb = fastrace::ArxmlParser::parseFiles(paths);
    populateDatabasesList();
    populateSignalsTab();
    populateMessagesTab();
    populateEcusTab();
    populateSomeIpTab();
}

void LeftPanelWidget::populateDatabasesList()
{
    ui->lstDatabases->clear();
    for (const auto& path : m_signalDbs.getActiveDatabases()) {
        const QString display = QString::fromStdString(
            std::filesystem::path(path).filename().string());
        auto* item = new QListWidgetItem(display, ui->lstDatabases);
        item->setData(Qt::UserRole, QString::fromStdString(path));
        item->setToolTip(QString::fromStdString(path));
    }
}

void LeftPanelWidget::populateTraceSummary()
{
    struct Row { const char *key; const char *value; };
    static const Row rows[] = {
        {"Messages",   "—"},
        {"ECUs",       "—"},
        {"Start time", "—"},
        {"Duration",   "—"},
        {"Bus",        "—"},
    };

    ui->traceSummary->setRowCount(5);
    for (int i = 0; i < 5; ++i) {
        auto* kItem = new QTableWidgetItem(rows[i].key);
        auto* vItem = new QTableWidgetItem(rows[i].value);
        vItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->traceSummary->setItem(i, 0, kItem);
        ui->traceSummary->setItem(i, 1, vItem);
    }
}

void LeftPanelWidget::populateMessagesTab()
{
    ui->busTree->clear();

    if (m_arxmlDb.messages.empty()) return;

    // Group by cluster
    std::map<std::string, std::vector<const fastrace::ArMessage*>> byCluster;
    for (const auto& msg : m_arxmlDb.messages)
        byCluster[msg.cluster].push_back(&msg);

    for (const auto& [cluster, msgs] : byCluster) {
        auto* clNode = new QTreeWidgetItem(ui->busTree,
            QStringList{QString::fromStdString(cluster)});
        for (const auto* msg : msgs) {
            const QString hexId = msg->isExtended
                ? QString("0x%1").arg(msg->canId, 8, 16, QChar('0')).toUpper()
                : QString("0x%1").arg(msg->canId, 3, 16, QChar('0')).toUpper();
            new QTreeWidgetItem(clNode,
                QStringList{QString::fromStdString(msg->name), hexId});
        }
        clNode->setExpanded(true);
    }
}

void LeftPanelWidget::populateSignalsTab()
{
    ui->sigTree->clear();

    for (const auto& msg : m_arxmlDb.messages) {
        if (msg.signalDefs.empty()) continue;
        auto* msgItem = new QTreeWidgetItem(ui->sigTree,
            QStringList{QString::fromStdString(msg.name)});
        for (const auto& sig : msg.signalDefs) {
            new QTreeWidgetItem(msgItem, QStringList{
                QString::fromStdString(sig.name),
                QString::number(sig.startBit),
                QString::number(sig.bitLength),
                sig.isBigEndian ? "Motorola" : "Intel"
            });
        }
    }
}

void LeftPanelWidget::populateEcusTab()
{
    ui->ecuList->clear();
    for (const auto& ecu : m_arxmlDb.ecus)
        ui->ecuList->addItem(QString::fromStdString(ecu.name));
}

void LeftPanelWidget::populateSomeIpTab()
{
    ui->serviceTree->clear();
    for (const auto& svc : m_arxmlDb.someipServices) {
        new QTreeWidgetItem(ui->serviceTree, QStringList{
            QString::fromStdString(svc.name),
            QString("0x%1").arg(svc.serviceId, 4, 16, QChar('0')).toUpper()
        });
    }
}
