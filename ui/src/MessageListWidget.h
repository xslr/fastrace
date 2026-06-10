#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MessageListWidget; }
QT_END_NAMESPACE

class MessageListWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent = nullptr);
    ~MessageListWidget() override;

private:
    Ui::MessageListWidget *ui;

    void populateTable();
};
