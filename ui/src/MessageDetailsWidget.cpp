#include "MessageDetailsWidget.h"
#include "ui_MessageDetailsWidget.h"
#include <QHeaderView>
#include <QColor>

MessageDetailsWidget::MessageDetailsWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MessageDetailsWidget)
{
    ui->setupUi(this);

    ui->generalHeader->setObjectName("headerLabel");
    ui->hexHeader->setObjectName("headerLabel");
    ui->binHeader->setObjectName("headerLabel");
    ui->signalsHeader->setObjectName("headerLabel");

    ui->hexDataLabel->setObjectName("codeLabel");
    ui->binDataLabel->setObjectName("codeLabel");

    ui->generalProps->horizontalHeader()->setVisible(false);
    ui->generalProps->verticalHeader()->setVisible(false);
    ui->generalProps->setStyleSheet("background-color: transparent; border: none;");
    ui->generalProps->resizeColumnsToContents();
    ui->generalProps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    ui->signalsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    populateGeneralProps();
    populateSignalsTable();
}

MessageDetailsWidget::~MessageDetailsWidget()
{
    delete ui;
}

void MessageDetailsWidget::populateGeneralProps()
{
    struct Row { const char *key; const char *value; };
    static const Row rows[] = {
        {"Timestamp",    "00:02:19.340123"},
        {"Relative Time","00:02:19.340123"},
        {"Bus",          "CAN FD (500 kbps)"},
        {"ID",           "0x101"},
        {"Name",         "EngineData_1"},
        {"DLC",          "8"},
        {"Length",       "8 bytes"},
        {"Type",         "Data Frame"},
    };

    QTableWidget *t = ui->generalProps;
    t->setRowCount(8);
    for (int i = 0; i < 8; ++i) {
        auto *kItem = new QTableWidgetItem(rows[i].key);
        kItem->setForeground(QColor("#8b8b99"));

        auto *vItem = new QTableWidgetItem(rows[i].value);
        vItem->setForeground(QColor("#dcdcdc"));

        t->setItem(i, 0, kItem);
        t->setItem(i, 1, vItem);
    }
    t->resizeColumnsToContents();
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    ui->hexDataLabel->setText("3C 9A 10 27 00 64 FF 80");
    ui->binDataLabel->setText(
        "0011 1100  1001 1010  0001 0000\n"
        "0010 0111  0000 0000  0110 0100\n"
        "1111 1111  1000 0000");
}

void MessageDetailsWidget::populateSignalsTable()
{
    struct Row { const char *signal, *value, *unit, *startBit, *length; bool highlight; };
    static const Row rows[] = {
        {"EngineSpeed", "2450",  "rpm",        "0",  "16", true},
        {"EngineLoad",  "64.0",  "%",          "16", "8",  false},
        {"EngineTemp",  "92",    "°C",         "24", "8",  false},
        {"FuelRate",    "15.6",  "mg/stroke",  "32", "16", false},
    };

    QTableWidget *t = ui->signalsTable;
    t->setRowCount(4);
    for (int r = 0; r < 4; ++r) {
        const Row &row = rows[r];
        QStringList cols{row.signal, row.value, row.unit, row.startBit, row.length};
        for (int c = 0; c < cols.size(); ++c) {
            auto *item = new QTableWidgetItem(cols[c]);
            if (c > 0)
                item->setForeground(QColor("#dcdcdc"));
            if (c == 0 && row.highlight)
                item->setForeground(QColor("#3b82f6"));
            t->setItem(r, c, item);
        }
    }
}
