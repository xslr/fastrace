#include "TopBarWidget.h"
#include "ui_TopBarWidget.h"
#include <QFileDialog>
#include <QFileInfo>

TopBarWidget::TopBarWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TopBarWidget)
{
    ui->setupUi(this);
    setFixedHeight(54);

    ui->btnOpen->setObjectName("iconBtn");
    connect(ui->btnOpen, &QPushButton::clicked, this, &TopBarWidget::onBtnOpenClicked);
}

TopBarWidget::~TopBarWidget()
{
    delete ui;
}

void TopBarWidget::onBtnOpenClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Trace File"),
        QString(),
        tr("BLF Files (*.blf);;All Files (*)"));
    if (path.isEmpty())
        return;

    const QString name = QFileInfo(path).fileName();
    if (ui->traceCombo->findText(name) == -1)
        ui->traceCombo->addItem(name);
    ui->traceCombo->setCurrentText(name);

    emit traceFileChanged(path);
}
