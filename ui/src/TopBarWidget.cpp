#include "TopBarWidget.h"
#include "ui_TopBarWidget.h"

TopBarWidget::TopBarWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TopBarWidget)
{
    ui->setupUi(this);
    setFixedHeight(54);

    ui->btnOpen->setObjectName("iconBtn");
}

TopBarWidget::~TopBarWidget()
{
    delete ui;
}
