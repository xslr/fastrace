#include "MessageListWidget.h"
#include "ui_MessageListWidget.h"
#include <QApplication>
#include <QHeaderView>
#include <QTableWidgetItem>
#include "Analyzer.h"
#include "BlfTypes.h"

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
            t->setItem(r, c, item);
        }
    }
}

void MessageListWidget::loadFile(const QString& path)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    fastrace::Analyzer analyzer;
    analyzer.collectMessages = true;
    analyzer.processFile(path.toStdString());

    QApplication::restoreOverrideCursor();

    QTableWidget* t = ui->msgTable;
    t->setRowCount(0);
    t->setRowCount(static_cast<int>(analyzer.messages.size()));

    for (int r = 0; r < t->rowCount(); ++r) {
        const fastrace::TraceMessage& msg = analyzer.messages[r];

        // Time: HH:MM:SS.microseconds
        const uint64_t us   = msg.timestampUs;
        const int      h    = static_cast<int>(us / 3'600'000'000ull);
        const int      m    = static_cast<int>((us % 3'600'000'000ull) / 60'000'000ull);
        const int      s    = static_cast<int>((us % 60'000'000ull) / 1'000'000ull);
        const int      us6  = static_cast<int>(us % 1'000'000ull);
        const QString timeStr = QString("%1:%2:%3.%4")
            .arg(h,   2, 10, QChar('0'))
            .arg(m,   2, 10, QChar('0'))
            .arg(s,   2, 10, QChar('0'))
            .arg(us6, 6, 10, QChar('0'));

        // Bus type
        QString bus;
        switch (static_cast<BLFObjectType>(msg.objectType)) {
            case CAN_MESSAGE:
            case CAN_MESSAGE2:     bus = "CAN";      break;
            case CAN_FD_MESSAGE:
            case CAN_FD_MESSAGE_64: bus = "CAN FD"; break;
            case ETHERNET_FRAME:
            case ETHERNET_FRAME_EX: bus = "Ethernet"; break;
            default: bus = "Other"; break;
        }

        // ID / Src
        QString idStr;
        if (msg.objectType == ETHERNET_FRAME ||
            msg.objectType == ETHERNET_FRAME_EX) {
            idStr = QString("CH%1").arg(msg.channel);
        } else {
            idStr = "0x" + QString::number(msg.arbId, 16).toUpper();
            if (msg.extendedId) idStr += "x";
        }

        // Data hex
        QString dataHex;
        dataHex.reserve(msg.dataLen * 3);
        for (int i = 0; i < msg.dataLen; ++i) {
            if (i > 0) dataHex += ' ';
            dataHex += QString("%1").arg(msg.data[i], 2, 16, QChar('0')).toUpper();
        }

        const QString dlcStr = QString::number(msg.dlc);
        const QString lenStr = QString::number(msg.dataLen);
        const QString ecuStr = QString("CH%1").arg(msg.channel);

        const QStringList cols{timeStr, bus, idStr, QString(), dlcStr, dataHex, lenStr, ecuStr};
        for (int c = 0; c < cols.size(); ++c)
            t->setItem(r, c, new QTableWidgetItem(cols[c]));
    }

    ui->msgTable->resizeColumnsToContents();
    ui->msgTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
}
