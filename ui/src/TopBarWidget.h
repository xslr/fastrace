#pragma once
#include <QWidget>
#include "RecentFiles.h"

QT_BEGIN_NAMESPACE
namespace Ui { class TopBarWidget; }
QT_END_NAMESPACE

class TopBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit TopBarWidget(QWidget *parent = nullptr);
    ~TopBarWidget() override;

signals:
    void playToggled(bool playing);
    void speedChanged(const QString &speed);
    void traceFileChanged(const QString &path);

private slots:
    void onBtnOpenClicked();
    void onComboActivated(int index);

private:
    void populateRecentCombo();
    void openTrace(const QString& path);

    Ui::TopBarWidget *ui;
    fastrace::RecentFiles m_recentFiles;
};
