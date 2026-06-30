#include "DetectionsWidget.h"
#include "DetectionFilterProxyModel.h"
#include "DetectionTableModel.h"
#include <set>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

#include "ui_DetectionsWidget.h"

DetectionsWidget::DetectionsWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DetectionsWidget)
{
    ui->setupUi(this);

    ui->statisticsHeader->setObjectName("headerLabel");
    ui->btnClear->setObjectName("iconBtn");
    ui->btnClear->setStyleSheet("border: 1px solid #3b3f55;");

    ui->detectionsHeader->setStyleSheet("font-weight: bold;");

    m_model = new DetectionTableModel(this);
    m_proxyModel = new DetectionFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    ui->detectionsTable->setModel(m_proxyModel);
    ui->detectionsTable->horizontalHeader()->setStretchLastSection(true);

    ui->statisticsTable->setFixedHeight(65);
    ui->statisticsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui->btnClear, &QPushButton::clicked, this, [this]() { setDetections({}); });

    connect(ui->cmbSeverity, &QComboBox::currentTextChanged, m_proxyModel,
        [this](const QString& text) { m_proxyModel->setSeverityFilter(text == "All Severities" ? "All" : text); });

    connect(ui->cmbDetector, &QComboBox::currentTextChanged, m_proxyModel,
        [this](const QString& text) { m_proxyModel->setDetectorFilter(text == "All Detectors" ? "All" : text); });

    connect(ui->detectionsTable->selectionModel(), &QItemSelectionModel::currentChanged, this,
        [this](const QModelIndex& current) {
            if (current.isValid()) {
                QModelIndex sourceIdx = m_proxyModel->mapToSource(current);
                emit detectionSelected(m_model->detectionAt(sourceIdx.row()).messageIndex);
            }
        });

    populateStatistics();
    setupTimelineBar();
}

DetectionsWidget::~DetectionsWidget() { delete ui; }

void DetectionsWidget::setDetections(const std::vector<Detection>& detections)
{
    m_model->setDetections(detections);
    ui->detectionsHeader->setText(QString("Detections (%1)").arg(detections.size()));
    ui->detectionsTable->resizeColumnsToContents();

    int total = detections.size();

    // Populate detector filter
    QString currentDetector = ui->cmbDetector->currentText();
    ui->cmbDetector->blockSignals(true);
    ui->cmbDetector->clear();
    ui->cmbDetector->addItem("All Detectors");
    std::set<std::string> detectors;
    int high = 0, med = 0, low = 0;
    for (const auto& d : detections) {
        if (d.severity == Severity::Error) {
            high++;
        } else if (d.severity == Severity::Warning) {
            med++;
        } else {
            low++;
        }
        detectors.insert(d.detectorName);
    }
    for (const auto& det : detectors) {
        ui->cmbDetector->addItem(QString::fromStdString(det));
    }
    int idx = ui->cmbDetector->findText(currentDetector);
    if (idx >= 0) {
        ui->cmbDetector->setCurrentIndex(idx);
    }
    ui->cmbDetector->blockSignals(false);

    ui->cmbSeverity->setEnabled(total > 0);
    ui->cmbDetector->setEnabled(total > 0);

    ui->statisticsTable->setItem(0, 0, new QTableWidgetItem(QString::number(total)));
    ui->statisticsTable->setItem(0, 1, new QTableWidgetItem(QString::number(high)));
    ui->statisticsTable->setItem(0, 2, new QTableWidgetItem(QString::number(med)));
    ui->statisticsTable->setItem(0, 3, new QTableWidgetItem(QString::number(low)));

    if (total > 0) {
        ui->statisticsTable->setItem(
            0, 4, new QTableWidgetItem(QString::number(detections.front().timestampUs / 1000000.0, 'f', 6)));
        ui->statisticsTable->setItem(
            0, 5, new QTableWidgetItem(QString::number(detections.back().timestampUs / 1000000.0, 'f', 6)));
    } else {
        ui->statisticsTable->setItem(0, 4, new QTableWidgetItem("-"));
        ui->statisticsTable->setItem(0, 5, new QTableWidgetItem("-"));
    }
}

void DetectionsWidget::populateStatistics()
{
    QTableWidget* t = ui->statisticsTable;
    t->setItem(0, 0, new QTableWidgetItem("8"));
    t->setItem(0, 1, new QTableWidgetItem("4"));
    t->setItem(0, 2, new QTableWidgetItem("3"));
    t->setItem(0, 3, new QTableWidgetItem("1"));
    t->setItem(0, 4, new QTableWidgetItem("15.153"));
    t->setItem(0, 5, new QTableWidgetItem("22.341"));
}

void DetectionsWidget::setupTimelineBar()
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
