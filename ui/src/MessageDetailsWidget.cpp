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

    ui->generalProps->horizontalHeader()->setVisible(false);
    ui->generalProps->verticalHeader()->setVisible(false);
    ui->generalProps->setStyleSheet("background-color: transparent; border: none;");
    ui->generalProps->resizeColumnsToContents();
    ui->generalProps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    populateGeneralProps();
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
}

void MessageDetailsWidget::updateFromMessage(const fastrace::TraceMessage& msg)
{
    QString hexStr;
    for (int i = 0; i < msg.dataLen; ++i) {
        if (i > 0) {
            if (i % 8 == 0) {
                hexStr += "\n";
            } else {
                hexStr += " ";
            }
        }
        hexStr += QString("%1").arg(msg.data[i], 2, 16, QChar('0')).toUpper();
    }
    ui->hexDataEdit->setPlainText(hexStr);
}
