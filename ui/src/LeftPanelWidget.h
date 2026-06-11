#pragma once
#include <QWidget>
#include "ArxmlTypes.h"
#include "SignalDatabases.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LeftPanelWidget; }
QT_END_NAMESPACE

class LeftPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit LeftPanelWidget(QWidget *parent = nullptr);
    ~LeftPanelWidget() override;

private slots:
    void onBtnAddDbClicked();
    void onBtnRemoveDbClicked();

private:
    void loadDatabases();
    void populateDatabasesList();
    void populateTraceSummary();
    void populateMessagesTab();
    void populateSignalsTab();
    void populateEcusTab();
    void populateSomeIpTab();

    Ui::LeftPanelWidget        *ui;
    fastrace::SignalDatabases   m_signalDbs;
    fastrace::ArDatabase        m_arxmlDb;
};
