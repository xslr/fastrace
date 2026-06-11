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

public slots:
    void loadFile(const QString& path);

private:
    Ui::MessageListWidget *ui;

    void populateTable();
};
