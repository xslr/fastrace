#include "LeftPanelWidget.h"
#include "ui_LeftPanelWidget.h"
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QColor>

LeftPanelWidget::LeftPanelWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::LeftPanelWidget)
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

    populateTraceSummary();
    populateBusTree();
}

LeftPanelWidget::~LeftPanelWidget()
{
    delete ui;
}

void LeftPanelWidget::populateTraceSummary()
{
    struct Row { const char *key; const char *value; };
    static const Row rows[] = {
        {"Messages",  "12,358"},
        {"ECUs",      "18"},
        {"Start time","00:00:00.000"},
        {"Duration",  "00:15:23.920"},
        {"Bus",       "CAN FD 500 kbps, Ethernet"},
    };

    ui->traceSummary->setRowCount(5);
    for (int i = 0; i < 5; ++i) {
        auto *kItem = new QTableWidgetItem(rows[i].key);
        kItem->setForeground(QColor("#8b8b99"));

        auto *vItem = new QTableWidgetItem(rows[i].value);
        vItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        ui->traceSummary->setItem(i, 0, kItem);
        ui->traceSummary->setItem(i, 1, vItem);
    }
}

void LeftPanelWidget::populateBusTree()
{
    QTreeWidget *tree = ui->busTree;

    auto addChild = [&](QTreeWidgetItem *parent, const QString &id,
                        const QString &count, bool select = false) {
        auto *item = new QTreeWidgetItem(parent, QStringList{id, count});
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setForeground(1, QColor("#8b8b99"));
        if (select)
            tree->setCurrentItem(item);
    };

    auto *canNode = new QTreeWidgetItem(tree, QStringList{"CAN FD (500 kbps)"});
    canNode->setForeground(0, QColor("#dcdcdc"));
    addChild(canNode, "0x101",        "1,245", true);
    addChild(canNode, "0x102",        "1,245");
    addChild(canNode, "0x120",        "623");
    addChild(canNode, "0x1A0",        "512");
    addChild(canNode, "0x2BC",        "432");
    addChild(canNode, "0x3F1",        "256");
    addChild(canNode, "0x5AA",        "128");
    addChild(canNode, "0x7E0",        "64");
    addChild(canNode, "0x7E1",        "64");
    addChild(canNode, "...",          "");
    canNode->setExpanded(true);

    auto *ethNode = new QTreeWidgetItem(tree, QStringList{"Ethernet (100BASE-T1)"});
    addChild(ethNode, "192.168.1.10", "3,421");
    addChild(ethNode, "192.168.1.20", "2,112");
    addChild(ethNode, "192.168.1.30", "1,005");
    addChild(ethNode, "Multicast",    "320");
    ethNode->setExpanded(true);
}
