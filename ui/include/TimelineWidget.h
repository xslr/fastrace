#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class TimelineWidget;
}
QT_END_NAMESPACE

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget() override;

private:
    Ui::TimelineWidget* ui;
};
