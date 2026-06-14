#pragma once
#include <QWidget>

#include "TraceMessage.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MessageDetailsWidget;
}
QT_END_NAMESPACE

class MessageDetailsWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageDetailsWidget(QWidget* parent = nullptr);
    ~MessageDetailsWidget() override;

    void updateFromMessage(const fastrace::TraceMessage& msg);

private:
    Ui::MessageDetailsWidget* ui;

    void populateGeneralProps();
};
