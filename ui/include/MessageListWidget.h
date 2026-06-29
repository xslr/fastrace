#pragma once
#include <QWidget>
#include <memory>
#include <vector>

#include "Analyzer.h"
#include "TraceMessage.h"

class MessageTableModel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MessageListWidget;
}
QT_END_NAMESPACE

class MessageListWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget* parent = nullptr);
    ~MessageListWidget() override;

public slots:
    /// Clear the table and show an empty state.
    void clearTable();

    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

signals:
    void messageSelected(const fastrace::TraceMessage& msg);

private slots:
    void onSelectionChanged();

private:
    Ui::MessageListWidget* ui;
    MessageTableModel* m_model;
};
