#pragma once
#include "Analyzer.h"
#include "TraceMessage.h"
#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MessageDetailsWidget;
}
QT_END_NAMESPACE

class QTreeWidget;
class QLabel;
class QVBoxLayout;

class MessageDetailsWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageDetailsWidget(QWidget* parent = nullptr);
    ~MessageDetailsWidget() override;

    void updateFromMessage(const fastrace::TraceMessage& msg);
    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);
    void refreshSignalDecode();

private:
    Ui::MessageDetailsWidget* ui;
    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    fastrace::TraceMessage m_lastMsg;
    bool m_hasMsg = false;
    QTreeWidget* m_signalTree = nullptr;
    QLabel* m_noSignalLabel = nullptr;

    void populateGeneralProps();
    void populateSignalTab();
};
