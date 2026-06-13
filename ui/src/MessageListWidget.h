#pragma once
#include <QWidget>
#include "TraceMessage.h"
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui { class MessageListWidget; }
QT_END_NAMESPACE

class MessageListWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent = nullptr);
    ~MessageListWidget() override;

public slots:
    /// Populate the table from a completed Analyzer::messages vector.
    /// Called on the UI thread after async loading finishes.
    void populateFrom(const std::vector<fastrace::TraceMessage>& messages);

    /// Clear the table and show an empty state.
    void clearTable();

private:
    Ui::MessageListWidget *ui;

    void populateTable();
};
