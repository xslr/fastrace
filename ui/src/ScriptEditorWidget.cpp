#include "ScriptEditorWidget.h"

#include "ui_ScriptEditorWidget.h"

ScriptEditorWidget::ScriptEditorWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ScriptEditorWidget)
{
    ui->setupUi(this);

    ui->scriptHeader->setObjectName("headerLabel");
    ui->btnRun->setObjectName("primaryBtn");

    ui->cursorPosLabel->setStyleSheet("color: #8b8b99;");
    ui->errorStatusLabel->setStyleSheet("color: #22c55e;");

    ui->scriptCombo->addItem("Engine RPM Spike Detector");

    loadSampleScript();
}

ScriptEditorWidget::~ScriptEditorWidget() { delete ui; }

void ScriptEditorWidget::loadSampleScript()
{
    const QString html
        = R"(<span style="color: #6a9955;">// Detect engine speed spikes</span><br>)"
          R"(<span style="color: #c586c0;">param</span> <span style="color: #9cdcfe;">max_rate</span> = <span style="color: #b5cea8;">3000</span>;  <span style="color: #6a9955;">// rpm per second</span><br>)"
          R"(<span style="color: #c586c0;">param</span> <span style="color: #9cdcfe;">min_rpm</span> = <span style="color: #b5cea8;">500</span>;<br>)"
          R"(<span style="color: #c586c0;">param</span> <span style="color: #9cdcfe;">window_ms</span> = <span style="color: #b5cea8;">1000</span>;<br><br>)"
          R"(<span style="color: #569cd6;">on_signal</span>(EngineSpeed) <span style="color: #569cd6;">as</span> s {<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #569cd6;">static</span> prev = s.value;<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #569cd6;">static</span> prev_ts = s.timestamp;<br><br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #569cd6;">let</span> dt = (s.timestamp - prev_ts) / <span style="color: #b5cea8;">1000.0</span>;<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #c586c0;">if</span> (dt &lt;= <span style="color: #b5cea8;">0</span>) <span style="color: #c586c0;">return</span>;<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #569cd6;">let</span> rate = (s.value - prev) / dt;<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #c586c0;">if</span> (s.value &gt; min_rpm &amp;&amp; abs(rate) &gt; max_rate) {<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<span style="color: #dcdcaa;">report</span>(<span style="color: #ce9178;">"RPM_SPIKE"</span>,<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;severity = <span style="color: #ce9178;">"high"</span>,<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;value = s.value,<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;rate = rate,<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;message = <span style="color: #ce9178;">"Engine speed changed too fast"</span>);<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;}<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;prev = s.value;<br>)"
          R"(&nbsp;&nbsp;&nbsp;&nbsp;prev_ts = s.timestamp;<br>)"
          R"(})";

    ui->scriptEditor->setHtml(html);
    ui->cursorPosLabel->setText("Ln 18, Col 47");
    ui->errorStatusLabel->setText("✓ No errors");
}
