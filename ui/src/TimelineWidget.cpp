#include "TimelineWidget.h"

#include "ui_TimelineWidget.h"

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::TimelineWidget)
{
    ui->setupUi(this);

    ui->timelineHeader->setObjectName("headerLabel");

    for (auto* btn : { ui->btnAnomalies, ui->btnBookmarks }) {
        btn->setObjectName("iconBtn");
    }
}

TimelineWidget::~TimelineWidget() { delete ui; }
