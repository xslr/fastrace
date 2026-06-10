#include "TimelineWidget.h"
#include "ui_TimelineWidget.h"

TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TimelineWidget)
{
    ui->setupUi(this);
    setStyleSheet("background-color: #1e1f2b;");

    ui->timelineHeader->setObjectName("headerLabel");

    for (auto *btn : {ui->btnAnomalies, ui->btnMessages, ui->btnBookmarks})
        btn->setObjectName("iconBtn");

    ui->btnAnomalies->setStyleSheet("QPushButton { color: #ef4444; background: transparent; border: none; padding: 4px; border-radius: 4px; }"
                                    "QPushButton:hover { background-color: #2a2d3d; }");

    ui->graphPlaceholder->setStyleSheet(
        "background-color: #111218; border: 1px solid #272a35;"
        " border-radius: 4px; color: #8b8b99; font-style: italic;");
}

TimelineWidget::~TimelineWidget()
{
    delete ui;
}
