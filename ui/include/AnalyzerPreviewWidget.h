#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class AnalyzerPreviewWidget;
}
QT_END_NAMESPACE

class AnalyzerPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit AnalyzerPreviewWidget(QWidget* parent = nullptr);
    ~AnalyzerPreviewWidget() override;

private:
    Ui::AnalyzerPreviewWidget* ui;

    void populateDetections();
    void populateStatistics();
    void setupTimelineBar();
};
