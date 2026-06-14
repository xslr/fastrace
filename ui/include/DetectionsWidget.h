#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class DetectionsWidget;
}
QT_END_NAMESPACE

class DetectionsWidget : public QWidget {
    Q_OBJECT
public:
    explicit DetectionsWidget(QWidget* parent = nullptr);
    ~DetectionsWidget() override;

private:
    Ui::DetectionsWidget* ui;

    void populateDetections();
    void populateStatistics();
    void setupTimelineBar();
};
