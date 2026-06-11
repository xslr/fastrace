#include "MessageListWidget.h"
#include "ui_MessageListWidget.h"
#include <QHeaderView>
#include <QFont>

MessageListWidget::MessageListWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MessageListWidget)
{
    ui->setupUi(this);

    ui->btnColumns->setObjectName("iconBtn");

    ui->cmbChannel->addItem("All");
    ui->cmbView->addItem("Default");

    ui->msgTable->horizontalHeader()->setStretchLastSection(true);
    ui->msgTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    populateTable();
    ui->msgTable->resizeColumnsToContents();
    ui->msgTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
}

MessageListWidget::~MessageListWidget()
{
    delete ui;
}

void MessageListWidget::populateTable()
{
    struct Row {
        const char *time, *bus, *id, *name, *dlc, *data, *len, *ecu;
        bool selected;
    };
    static const Row rows[] = {
        {"00:02:19.340123","CAN FD",  "0x101",        "EngineData_1", "8",  "3C 9A 10 27 00 64 FF 80",     "8",   "ECM",     true},
        {"00:02:19.340256","CAN FD",  "0x102",        "EngineData_2", "8",  "01 0F 00 00 7B 3C 20 10",     "8",   "ECM",     false},
        {"00:02:19.340410","CAN FD",  "0x120",        "VehicleStatus","8",  "00 00 00 01 00 00 40 00",     "8",   "VCU",     false},
        {"00:02:19.340512","Ethernet","192.168.1.10",  "DiagResponse", "64", "02 10 41 00 BE 1F 90 23 ...","64",  "GATEWAY", false},
        {"00:02:19.340889","CAN FD",  "0x1A0",        "BrakeStatus",  "8",  "00 00 00 00 00 00 00 00",     "8",   "ABS",     false},
        {"00:02:19.341002","Ethernet","192.168.1.20",  "CameraFrame",  "512","FF D8 FF E1 00 10 4A 46 ...","512", "CAMERA",  false},
    };

    QTableWidget *t = ui->msgTable;
    t->setRowCount(static_cast<int>(sizeof(rows) / sizeof(rows[0])));

    QFont monoFont("Consolas", 9);


    for (int r = 0; r < t->rowCount(); ++r) {
        const Row &row = rows[r];
        QStringList cols{row.time, row.bus, row.id, row.name, row.dlc, row.data, row.len, row.ecu};
        for (int c = 0; c < cols.size(); ++c) {
            auto *item = new QTableWidgetItem(cols[c]);
            // if (c == 5)
            //     item->setFont(monoFont);
            // if (row.selected)
            //     item->setBackground(QColor("#2a2d3d"));
            t->setItem(r, c, item);
        }
    }
}
