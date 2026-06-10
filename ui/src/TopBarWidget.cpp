#include "TopBarWidget.h"
#include "ui_TopBarWidget.h"

TopBarWidget::TopBarWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TopBarWidget)
{
    ui->setupUi(this);
    setFixedHeight(54);
    setStyleSheet("background-color: #111218; border-bottom: 1px solid #272a35;");

    ui->logoLabel->setObjectName("titleLabel");

    ui->windowSizeLabel->setStyleSheet(
        "background-color: #2a2d3d; padding: 2px 6px; border-radius: 4px; font-size: 11px;");

    for (auto *btn : {ui->btnFirst, ui->btnReverse, ui->btnPlay, ui->btnForward, ui->btnLast})
        btn->setObjectName("iconBtn");

    ui->btnLivePreview->setStyleSheet(
        "QPushButton { background-color: #2a2d3d; color: #86efac;"
        " border: 1px solid #22c55e; border-radius: 4px; padding: 4px 10px; }");

    ui->btnSave->setObjectName("primaryBtn");

    ui->traceCombo->addItem("drive_2024-05-17_10-38-21.asc");
    ui->speedCombo->addItem("1x");
    ui->speedCombo->addItem("2x");
    ui->speedCombo->addItem("0.5x");

    connect(ui->btnPlay, &QPushButton::clicked, this, [this]() {
        static bool playing = false;
        playing = !playing;
        ui->btnPlay->setText(playing ? "⏸" : "▶");
        emit playToggled(playing);
    });

    connect(ui->speedCombo, &QComboBox::currentTextChanged,
            this, &TopBarWidget::speedChanged);
}

TopBarWidget::~TopBarWidget()
{
    delete ui;
}
