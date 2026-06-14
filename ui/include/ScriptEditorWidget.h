#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class ScriptEditorWidget;
}
QT_END_NAMESPACE

class ScriptEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScriptEditorWidget(QWidget* parent = nullptr);
    ~ScriptEditorWidget() override;

private:
    Ui::ScriptEditorWidget* ui;

    void loadSampleScript();
};
