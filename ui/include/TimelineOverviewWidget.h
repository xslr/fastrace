#pragma once

#include <QFutureWatcher>
#include <QTimer>
#include <QWidget>
#include <array>
#include <memory>
#include <vector>

namespace fastrace {
class Analyzer;
}

class QCheckBox;
class QHBoxLayout;

class TimelineOverviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineOverviewWidget(QWidget* parent = nullptr);
    ~TimelineOverviewWidget() override = default;

    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

    void setVisibleWindow(uint64_t startUs, uint64_t endUs);

    void activate();
    void deactivate();

signals:
    void navigateRequested(uint64_t timestampUs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private slots:
    void onLaneToggled();
    void onHistogramFinished();

private:
    void restartHistogramJob();

    std::shared_ptr<fastrace::Analyzer> m_analyzer;

    QCheckBox* m_chkCan = nullptr;
    QCheckBox* m_chkEthernet = nullptr;

    // UI state
    uint64_t m_visibleStartUs = 0;
    uint64_t m_visibleEndUs = 0;

    // Async jobs
    QFutureWatcher<void> m_histogramWatcher;
    QTimer m_repaintTimer;
    QTimer m_debounceTimer;
};
