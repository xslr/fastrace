#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MessageDetailsWidget; }
QT_END_NAMESPACE

class MessageDetailsWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageDetailsWidget(QWidget *parent = nullptr);
    ~MessageDetailsWidget() override;

private:
    Ui::MessageDetailsWidget *ui;

    void populateGeneralProps();
    void populateSignalsTable();
};
