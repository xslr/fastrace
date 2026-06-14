#pragma once
#include <QWidget>
#include "TraceMessage.h"
#include <vector>
#include <memory>
#include "Analyzer.h"

class MessageTableModel;

QT_BEGIN_NAMESPACE
namespace Ui { class MessageListWidget; }
QT_END_NAMESPACE

class MessageListWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent = nullptr);
    ~MessageListWidget() override;

public slots:
    /// DEPRECATED: Use attachAnalyzer() and lazy loading instead.
    /// Populate the table from a completed Analyzer::messages vector.
    /// Called on the UI thread after async loading finishes.
    void populateFrom(const std::vector<fastrace::TraceMessage>& messages);

    /// Clear the table and show an empty state.
    /// Clear the table and show an empty state.
    void clearTable();

    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);
private:
    Ui::MessageListWidget *ui;
    MessageTableModel *m_model;
};
