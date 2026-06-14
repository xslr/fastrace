#include "AnalyzerPreviewWidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

#include "ui_AnalyzerPreviewWidget.h"

AnalyzerPreviewWidget::AnalyzerPreviewWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::AnalyzerPreviewWidget)
{
    ui->setupUi(this);

    ui->statisticsHeader->setObjectName("headerLabel");
    ui->btnClear->setObjectName("iconBtn");
    ui->btnClear->setStyleSheet("border: 1px solid #3b3f55;");

    ui->detectionsHeader->setStyleSheet("font-weight: bold;");

    ui->detectionsTable->horizontalHeader()->setStretchLastSection(true);
    ui->detectionsTable->resizeColumnsToContents();

    ui->statisticsTable->setFixedHeight(65);
    ui->statisticsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    populateDetections();
    populateStatistics();
    setupTimelineBar();
}

AnalyzerPreviewWidget::~AnalyzerPreviewWidget() { delete ui; }

void AnalyzerPreviewWidget::populateDetections()
{
    struct Row {
        const char *time, *analyzer, *severity, *message;
    };
    static const Row rows[] = {
        { "00:02:15.153", "RPM_SPIKE", "High", "Engine speed changed too fast" },
        { "00:02:18.671", "RPM_SPIKE", "High", "Engine speed changed too fast" },
        { "00:02:20.310", "RPM_SPIKE", "High", "Engine speed changed too fast" },
        { "00:02:22.145", "RPM_SPIKE", "High", "Engine speed changed too fast" },
        { "00:02:23.902", "RPM_SPIKE", "High", "Engine speed changed too fast" },
    };

    QTableWidget* t = ui->detectionsTable;
    t->setRowCount(5);
    ui->detectionsHeader->setText("Detections (8)");

    for (int r = 0; r < 5; ++r) {
        const Row& row = rows[r];
        t->setItem(r, 0, new QTableWidgetItem(row.time));
        t->setItem(r, 1, new QTableWidgetItem(row.analyzer));

        auto* sevLabel = new QLabel(row.severity);
        sevLabel->setAlignment(Qt::AlignCenter);
        sevLabel->setStyleSheet("background-color: #451a1a; color: #f87171;"
                                " border: 1px solid #7f1d1d; border-radius: 4px;"
                                " padding: 2px 6px; font-size: 10px; font-weight: bold;");
        auto* sevCell = new QWidget;
        auto* sevLayout = new QHBoxLayout(sevCell);
        sevLayout->setContentsMargins(4, 2, 4, 2);
        sevLayout->addWidget(sevLabel);
        t->setCellWidget(r, 2, sevCell);

        t->setItem(r, 3, new QTableWidgetItem(row.message));
    }
    t->resizeColumnsToContents();
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
}

void AnalyzerPreviewWidget::populateStatistics()
{
    QTableWidget* t = ui->statisticsTable;
    t->setItem(0, 0, new QTableWidgetItem("8"));
    t->setItem(0, 1, new QTableWidgetItem("8"));
    t->setItem(0, 2, new QTableWidgetItem("0"));
    t->setItem(0, 3, new QTableWidgetItem("0"));
    t->setItem(0, 4, new QTableWidgetItem("00:02:15.153"));
    t->setItem(0, 5, new QTableWidgetItem("00:02:23.902"));
}

void AnalyzerPreviewWidget::setupTimelineBar()
{
    auto* bar = ui->timelineBarLabel;
    bar->setFixedHeight(20);
    bar->setStyleSheet("background-color: #111218; border: 1px solid #272a35; "
                       "border-radius: 4px;");

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* marker = new QLabel("|  |    |        | | |    | |");
    marker->setStyleSheet("color: #ef4444; font-weight: bold; letter-spacing: 5px;");
    marker->setAlignment(Qt::AlignCenter);
    layout->addWidget(marker);
}
