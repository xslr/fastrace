#include "Detection.h"
#include <QWidget>
#include <vector>

class DetectionTableModel;
class DetectionFilterProxyModel;

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
    void setDetections(const std::vector<Detection>& detections);

signals:
    void detectionSelected(size_t messageIndex);

private:
    Ui::DetectionsWidget* ui;
    DetectionTableModel* m_model;
    DetectionFilterProxyModel* m_proxyModel;

    void populateStatistics();
    void setupTimelineBar();
};
