#pragma once
#include <QWidget>

#include "RecentFiles.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class TopBarWidget;
}
QT_END_NAMESPACE

class TopBarWidget : public QWidget {
    Q_OBJECT
public:
    enum class ViewMode { Overview, Notebook };

    explicit TopBarWidget(QWidget* parent = nullptr);
    ~TopBarWidget() override;

    ViewMode currentMode() const { return m_mode; }

signals:
    void playToggled(bool playing);
    void speedChanged(const QString& speed);
    void traceFileChanged(const QString& path);
    /** Emitted whenever the user clicks Overview or Notebook. */
    void modeChanged(TopBarWidget::ViewMode mode);
    void databaseSelectionChanged(const QStringList& paths);

private slots:
    void onComboActivated(int index);
    void onDbComboActivated(int index);
    void onOverviewClicked();
    void onNotebookClicked();

private:
    void populateTraceCombo();
    void populateDbCombo();
    void openTrace(const QString& path);
    void emitDbSelectionChanged();
    void updateDbComboDisplay(); ///< syncs DatabaseComboBox active-count display
    void updateModeButtons();

    Ui::TopBarWidget* ui;
    fastrace::RecentFiles m_recentFiles;
    fastrace::RecentFiles m_recentDbs;
    QSet<QString> m_activeDbPaths;
    ViewMode m_mode = ViewMode::Overview;
};
