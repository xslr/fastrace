#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class LeftPanelWidget; }
QT_END_NAMESPACE

class LeftPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit LeftPanelWidget(QWidget *parent = nullptr);
    ~LeftPanelWidget() override;

private:
    Ui::LeftPanelWidget *ui;

    void populateTraceSummary();
    void populateBusTree();
};
