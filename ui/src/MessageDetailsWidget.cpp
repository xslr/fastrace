#include "MessageDetailsWidget.h"

#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "SignalDecoder.h"
#include "ui_MessageDetailsWidget.h"

MessageDetailsWidget::MessageDetailsWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MessageDetailsWidget)
{
    ui->setupUi(this);

    ui->generalHeader->setObjectName("headerLabel");
    ui->hexHeader->setObjectName("headerLabel");

    ui->generalProps->horizontalHeader()->setVisible(false);
    ui->generalProps->verticalHeader()->setVisible(false);
    ui->generalProps->setStyleSheet("background-color: transparent; border: none;");
    ui->generalProps->resizeColumnsToContents();
    ui->generalProps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // Set up signal decode tab
    auto* sigLayout = new QVBoxLayout(ui->tabSignalDecoding);
    sigLayout->setContentsMargins(8, 8, 8, 8);

    m_noSignalLabel = new QLabel("No signal definitions loaded", ui->tabSignalDecoding);
    m_noSignalLabel->setAlignment(Qt::AlignCenter);
    sigLayout->addWidget(m_noSignalLabel);

    m_signalTree = new QTreeWidget(ui->tabSignalDecoding);
    m_signalTree->setHeaderLabels({ "Signal", "Raw Value", "Bits" });
    m_signalTree->setRootIsDecorated(false);
    m_signalTree->setStyleSheet("background-color: transparent; border: none;");
    m_signalTree->setVisible(false);
    sigLayout->addWidget(m_signalTree);

    populateGeneralProps();
}

MessageDetailsWidget::~MessageDetailsWidget() { delete ui; }

void MessageDetailsWidget::populateGeneralProps()
{
    struct Row {
        const char* key;
        const char* value;
    };
    static const Row rows[] = {
        { "Timestamp", "00:02:19.340123" },
        { "Relative Time", "00:02:19.340123" },
        { "Bus", "CAN FD (500 kbps)" },
        { "ID", "0x101" },
        { "Name", "EngineData_1" },
        { "DLC", "8" },
        { "Length", "8 bytes" },
        { "Type", "Data Frame" },
    };

    QTableWidget* t = ui->generalProps;
    t->setRowCount(8);
    for (int i = 0; i < 8; ++i) {
        auto* kItem = new QTableWidgetItem(rows[i].key);
        kItem->setForeground(QColor("#8b8b99"));

        auto* vItem = new QTableWidgetItem(rows[i].value);
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

    m_lastMsg = msg;
    m_hasMsg = true;
    populateSignalTab();
}

void MessageDetailsWidget::attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer)
{
    m_analyzer = std::move(analyzer);
}

void MessageDetailsWidget::refreshSignalDecode()
{
    if (m_hasMsg && m_analyzer) {
        populateSignalTab();
    }
}

void MessageDetailsWidget::populateSignalTab()
{
    if (!m_analyzer || m_analyzer->arDatabase().empty()) {
        m_signalTree->setVisible(false);
        m_noSignalLabel->setVisible(true);
        return;
    }

    auto decoded = fastrace::decodeAllSignals(m_analyzer->arDatabase(), m_lastMsg);
    if (decoded.empty()) {
        m_signalTree->setVisible(false);
        m_noSignalLabel->setVisible(true);
        return;
    }

    m_signalTree->clear();
    for (const auto& ds : decoded) {
        auto* item = new QTreeWidgetItem(m_signalTree);
        item->setText(0, QString::fromStdString(ds.name));
        item->setText(1, QString("0x%1 (%2)").arg(ds.rawValue, 0, 16).arg(ds.rawValue));

        QString bitsStr;
        if (!ds.isBigEndian) {
            bitsStr = QString("[%1..%2]").arg(ds.startBit + ds.bitLength - 1).arg(ds.startBit);
        } else {
            bitsStr = QString("[%1..%2]").arg(ds.startBit).arg(ds.startBit - ds.bitLength + 1);
        }
        item->setText(2, bitsStr);
    }

    m_signalTree->resizeColumnToContents(0);
    m_signalTree->resizeColumnToContents(1);
    m_signalTree->setVisible(true);
    m_noSignalLabel->setVisible(false);
}
