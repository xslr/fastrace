#include "MainWindow.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QStatusBar>
#include <QHeaderView>
#include <QToolBar>
#include <QIcon>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AutoTrace Analyzer");
    resize(1400, 900);
    
    // Set a dark style sheet for the whole application
    setStyleSheet(R"(
        QMainWindow { background-color: #181921; color: #dcdcdc; }
        QWidget { background-color: #181921; color: #dcdcdc; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; font-size: 13px; }
        QSplitter::handle { background-color: #272a35; }
        QPushButton { background-color: #2a2d3d; border: 1px solid #3b3f55; border-radius: 4px; padding: 4px 10px; color: #ffffff; }
        QPushButton:hover { background-color: #3b3f55; }
        QPushButton#primaryBtn { background-color: #2563eb; color: white; border: none; }
        QPushButton#primaryBtn:hover { background-color: #1d4ed8; }
        QPushButton#iconBtn { background-color: transparent; border: none; padding: 4px; border-radius: 4px; }
        QPushButton#iconBtn:hover { background-color: #2a2d3d; }
        QComboBox { background-color: #1e1f2b; border: 1px solid #3b3f55; border-radius: 4px; padding: 2px 10px; color: #dcdcdc; }
        QComboBox::drop-down { border-left: 1px solid #3b3f55; width: 20px; }
        QTabWidget::pane { border: 1px solid #272a35; background-color: #181921; }
        QTabBar::tab { background-color: #181921; border-bottom: 2px solid transparent; padding: 8px 16px; color: #8b8b99; }
        QTabBar::tab:selected { border-bottom: 2px solid #3b82f6; color: #ffffff; }
        QTabBar::tab:hover { color: #ffffff; }
        QTableWidget { background-color: #1e1f2b; alternate-background-color: #181921; border: 1px solid #272a35; gridline-color: #272a35; selection-background-color: #2a2d3d; color: #dcdcdc; }
        QHeaderView::section { background-color: #1e1f2b; border: none; border-bottom: 1px solid #272a35; border-right: 1px solid #272a35; padding: 4px 8px; color: #8b8b99; font-weight: bold; font-size: 11px; }
        QTreeWidget { background-color: #181921; border: none; }
        QTreeWidget::item { padding: 4px; }
        QTreeWidget::item:selected { background-color: #2a2d3d; }
        QTextEdit { background-color: #111218; border: 1px solid #272a35; font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; line-height: 1.5; selection-background-color: #3b3f55; }
        QLabel { color: #dcdcdc; }
        QLabel#headerLabel { font-size: 11px; font-weight: bold; color: #8b8b99; text-transform: uppercase; padding-bottom: 4px; }
        QLabel#titleLabel { font-size: 16px; font-weight: bold; color: #ffffff; }
        QLabel#valueLabel { color: #ffffff; }
        QLabel#codeLabel { font-family: 'Consolas', 'Courier New', monospace; background-color: #111218; padding: 8px; border: 1px solid #272a35; border-radius: 4px; }
        QScrollBar:vertical { background: #181921; width: 12px; margin: 0px; }
        QScrollBar::handle:vertical { background: #3b3f55; min-height: 20px; border-radius: 6px; margin: 2px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar:horizontal { background: #181921; height: 12px; margin: 0px; }
        QScrollBar::handle:horizontal { background: #3b3f55; min-width: 20px; border-radius: 6px; margin: 2px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
    )");

    // 1. Top Toolbar (simulated with a widget since it has complex layout)
    QWidget *topBar = new QWidget;
    topBar->setFixedHeight(54);
    topBar->setStyleSheet("background-color: #111218; border-bottom: 1px solid #272a35;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 8, 16, 8);
    
    QLabel *logoLabel = new QLabel("<b><i>AA</i></b> AutoTrace Analyzer");
    logoLabel->setObjectName("titleLabel");
    
    QComboBox *traceCombo = new QComboBox;
    traceCombo->addItem("drive_2024-05-17_10-38-21.asc");
    traceCombo->setMinimumWidth(250);
    
    QLabel *timeLabel = new QLabel("00:02:14.350 \u2013 00:02:24.350");
    QLabel *windowSizeLabel = new QLabel("10 s");
    windowSizeLabel->setStyleSheet("background-color: #2a2d3d; padding: 2px 6px; border-radius: 4px; font-size: 11px;");
    
    QPushButton *btnPrev = new QPushButton("|<"); btnPrev->setObjectName("iconBtn");
    QPushButton *btnRev = new QPushButton("<<"); btnRev->setObjectName("iconBtn");
    QPushButton *btnPlay = new QPushButton("▶"); btnPlay->setObjectName("iconBtn");
    QPushButton *btnFwd = new QPushButton(">>"); btnFwd->setObjectName("iconBtn");
    QPushButton *btnNext = new QPushButton(">|"); btnNext->setObjectName("iconBtn");
    QComboBox *speedCombo = new QComboBox;
    speedCombo->addItem("1x");

    QPushButton *btnLive = new QPushButton("● Live Preview");
    btnLive->setStyleSheet("QPushButton { background-color: #2a2d3d; color: #86efac; border: 1px solid #22c55e; border-radius: 4px; padding: 4px 10px; }");
    QPushButton *btnSave = new QPushButton("Save Analyzer ▾");
    btnSave->setObjectName("primaryBtn");

    topLayout->addWidget(logoLabel);
    topLayout->addSpacing(30);
    topLayout->addWidget(traceCombo);
    topLayout->addStretch();
    topLayout->addWidget(timeLabel);
    topLayout->addWidget(windowSizeLabel);
    topLayout->addSpacing(20);
    topLayout->addWidget(btnPrev);
    topLayout->addWidget(btnRev);
    topLayout->addWidget(btnPlay);
    topLayout->addWidget(btnFwd);
    topLayout->addWidget(btnNext);
    topLayout->addWidget(speedCombo);
    topLayout->addStretch();
    topLayout->addWidget(btnLive);
    topLayout->addWidget(btnSave);

    setMenuWidget(topBar);

    // 2. Main Central Splitter
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);
    setCentralWidget(mainSplitter);

    // --- LEFT PANEL ---
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 16, 16, 16);
    leftLayout->setSpacing(12);
    
    QLabel *traceHeader = new QLabel("TRACE");
    traceHeader->setObjectName("headerLabel");
    
    QTableWidget *traceSummary = new QTableWidget(5, 2);
    traceSummary->horizontalHeader()->setVisible(false);
    traceSummary->verticalHeader()->setVisible(false);
    traceSummary->setShowGrid(false);
    traceSummary->setFocusPolicy(Qt::NoFocus);
    traceSummary->setStyleSheet("background-color: transparent; border: none;");
    traceSummary->setItem(0, 0, new QTableWidgetItem("Messages")); traceSummary->setItem(0, 1, new QTableWidgetItem("12,358"));
    traceSummary->setItem(1, 0, new QTableWidgetItem("ECUs")); traceSummary->setItem(1, 1, new QTableWidgetItem("18"));
    traceSummary->setItem(2, 0, new QTableWidgetItem("Start time")); traceSummary->setItem(2, 1, new QTableWidgetItem("00:00:00.000"));
    traceSummary->setItem(3, 0, new QTableWidgetItem("Duration")); traceSummary->setItem(3, 1, new QTableWidgetItem("00:15:23.920"));
    traceSummary->setItem(4, 0, new QTableWidgetItem("Bus")); traceSummary->setItem(4, 1, new QTableWidgetItem("CAN FD 500 kbps, Ethernet"));
    
    for (int i = 0; i < 5; ++i) {
        traceSummary->item(i, 0)->setForeground(QColor("#8b8b99"));
        traceSummary->item(i, 1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    
    traceSummary->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    traceSummary->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    traceSummary->setFixedHeight(130);

    QPushButton *btnOpenTrace = new QPushButton("📂 Open Trace");
    btnOpenTrace->setStyleSheet("padding: 6px;");

    QTabWidget *leftTabs = new QTabWidget;
    QWidget *tabSignals = new QWidget;
    QWidget *tabMessages = new QWidget;
    QWidget *tabECUs = new QWidget;
    
    leftTabs->addTab(tabSignals, "Signals");
    leftTabs->addTab(tabMessages, "Messages");
    leftTabs->addTab(tabECUs, "ECUs");
    leftTabs->setCurrentIndex(1); // Select Messages tab

    // Setup Messages tab inside left panel
    QVBoxLayout *msgTabLayout = new QVBoxLayout(tabMessages);
    msgTabLayout->setContentsMargins(0, 10, 0, 0);
    msgTabLayout->setSpacing(10);
    
    QPushButton *btnSearch = new QPushButton("🔍 Search messages...");
    btnSearch->setStyleSheet("text-align: left; padding: 6px 10px; color: #8b8b99; background-color: #111218; border: 1px solid #272a35;");
    
    QTreeWidget *busTree = new QTreeWidget;
    busTree->setHeaderHidden(true);
    busTree->setColumnCount(2);
    busTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    busTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    
    QTreeWidgetItem *canNode = new QTreeWidgetItem(busTree, QStringList() << "CAN FD (500 kbps)");
    canNode->setForeground(0, QColor("#dcdcdc"));
    
    auto addBusItem = [](QTreeWidgetItem* parent, const QString& id, const QString& count, bool selected = false) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parent, QStringList() << id << count);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setForeground(1, QColor("#8b8b99"));
        if (selected) {
            parent->treeWidget()->setCurrentItem(item);
        }
    };
    
    addBusItem(canNode, "0x101", "1,245", true);
    addBusItem(canNode, "0x102", "1,245");
    addBusItem(canNode, "0x120", "623");
    addBusItem(canNode, "0x1A0", "512");
    addBusItem(canNode, "0x2BC", "432");
    addBusItem(canNode, "0x3F1", "256");
    addBusItem(canNode, "0x5AA", "128");
    addBusItem(canNode, "0x7E0", "64");
    addBusItem(canNode, "0x7E1", "64");
    addBusItem(canNode, "...", "");
    canNode->setExpanded(true);
    
    QTreeWidgetItem *ethNode = new QTreeWidgetItem(busTree, QStringList() << "Ethernet (100BASE-T1)");
    addBusItem(ethNode, "192.168.1.10", "3,421");
    addBusItem(ethNode, "192.168.1.20", "2,112");
    addBusItem(ethNode, "192.168.1.30", "1,005");
    addBusItem(ethNode, "Multicast", "320");
    ethNode->setExpanded(true);
    
    msgTabLayout->addWidget(btnSearch);
    msgTabLayout->addWidget(busTree);

    leftLayout->addWidget(traceHeader);
    leftLayout->addWidget(traceSummary);
    leftLayout->addWidget(btnOpenTrace);
    leftLayout->addSpacing(16);
    leftLayout->addWidget(leftTabs);

    // --- MIDDLE PANEL ---
    QSplitter *middleSplitter = new QSplitter(Qt::Vertical);
    middleSplitter->setChildrenCollapsible(false);
    
    // Timeline area (simulated with a label for the graph)
    QWidget *timelineWidget = new QWidget;
    timelineWidget->setStyleSheet("background-color: #1e1f2b;");
    QVBoxLayout *timelineLayout = new QVBoxLayout(timelineWidget);
    timelineLayout->setContentsMargins(16, 16, 16, 16);
    
    QHBoxLayout *timelineTopLayout = new QHBoxLayout;
    QLabel *timelineHeader = new QLabel("Timeline");
    timelineHeader->setObjectName("headerLabel");
    timelineTopLayout->addWidget(timelineHeader);
    
    QPushButton *btnAnom = new QPushButton("⚠ Anomalies"); btnAnom->setObjectName("iconBtn"); btnAnom->setStyleSheet("color: #ef4444;");
    QPushButton *btnMsgFilter = new QPushButton("☑ Messages"); btnMsgFilter->setObjectName("iconBtn");
    QPushButton *btnBookmark = new QPushButton("🔖 Bookmarks"); btnBookmark->setObjectName("iconBtn");
    
    timelineTopLayout->addWidget(btnAnom);
    timelineTopLayout->addWidget(btnMsgFilter);
    timelineTopLayout->addWidget(btnBookmark);
    timelineTopLayout->addStretch();
    
    QLabel *timelineGraph = new QLabel;
    timelineGraph->setText("[ Timeline Graphs Component Placeholder ]\n\n- EngineSpeed (rpm)\n- CoolantTemp (°C)\n- ThrottlePos (%)\n- VehicleSpeed (km/h)\n- BatteryVoltage (V)");
    timelineGraph->setAlignment(Qt::AlignCenter);
    timelineGraph->setStyleSheet("background-color: #111218; border: 1px solid #272a35; border-radius: 4px; color: #8b8b99; font-style: italic;");
    
    timelineLayout->addLayout(timelineTopLayout);
    timelineLayout->addWidget(timelineGraph, 1);

    // Message List
    QWidget *msgListWidget = new QWidget;
    QVBoxLayout *msgListLayout = new QVBoxLayout(msgListWidget);
    msgListLayout->setContentsMargins(16, 16, 16, 16);
    msgListLayout->setSpacing(12);
    
    QHBoxLayout *msgListTop = new QHBoxLayout;
    QLabel *msgListHeader = new QLabel("Message List");
    msgListHeader->setObjectName("headerLabel");
    
    msgListTop->addWidget(msgListHeader);
    msgListTop->addSpacing(20);
    msgListTop->addWidget(new QLabel("View:")); 
    QComboBox *viewCombo = new QComboBox; viewCombo->addItem("All"); msgListTop->addWidget(viewCombo);
    msgListTop->addSpacing(10);
    msgListTop->addWidget(new QLabel("Bus:")); 
    QComboBox *busCombo = new QComboBox; busCombo->addItem("All"); msgListTop->addWidget(busCombo);
    msgListTop->addSpacing(10);
    msgListTop->addWidget(new QLabel("Display:")); 
    QComboBox *displayCombo = new QComboBox; displayCombo->addItem("Default"); msgListTop->addWidget(displayCombo);
    
    msgListTop->addStretch();
    QPushButton *btnCols = new QPushButton("☷ Columns");
    btnCols->setObjectName("iconBtn");
    msgListTop->addWidget(btnCols);
    
    QTableWidget *msgTable = new QTableWidget(6, 8);
    msgTable->setHorizontalHeaderLabels({"Time", "Bus", "ID / Src", "Name", "DLC", "Data", "Length", "ECU"});
    msgTable->horizontalHeader()->setStretchLastSection(true);
    msgTable->verticalHeader()->setVisible(false);
    msgTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    msgTable->setAlternatingRowColors(true);
    msgTable->setShowGrid(false);
    
    auto setMsgRow = [&](int row, const QStringList& data, bool highlight = false) {
        for (int i = 0; i < data.size() && i < 8; ++i) {
            QTableWidgetItem *item = new QTableWidgetItem(data[i]);
            if (i == 5) item->setFont(QFont("Consolas", 9)); // Data column monospace
            msgTable->setItem(row, i, item);
        }
        if (highlight) {
            for (int i = 0; i < 8; ++i) {
                msgTable->item(row, i)->setBackground(QColor("#2a2d3d"));
            }
        }
    };
    
    setMsgRow(0, {"00:02:19.340123", "CAN FD", "0x101", "EngineData_1", "8", "3C 9A 10 27 00 64 FF 80", "8", "ECM"}, true);
    setMsgRow(1, {"00:02:19.340256", "CAN FD", "0x102", "EngineData_2", "8", "01 0F 00 00 7B 3C 20 10", "8", "ECM"});
    setMsgRow(2, {"00:02:19.340410", "CAN FD", "0x120", "VehicleStatus", "8", "00 00 00 01 00 00 40 00", "8", "VCU"});
    setMsgRow(3, {"00:02:19.340512", "Ethernet", "192.168.1.10", "DiagResponse", "64", "02 10 41 00 BE 1F 90 23 ...", "64", "GATEWAY"});
    setMsgRow(4, {"00:02:19.340889", "CAN FD", "0x1A0", "BrakeStatus", "8", "00 00 00 00 00 00 00 00", "8", "ABS"});
    setMsgRow(5, {"00:02:19.341002", "Ethernet", "192.168.1.20", "CameraFrame", "512", "FF D8 FF E1 00 10 4A 46 ...", "512", "CAMERA"});
    
    msgTable->resizeColumnsToContents();
    msgTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch); // Data column expands
    
    msgListLayout->addLayout(msgListTop);
    msgListLayout->addWidget(msgTable);

    // Message Details
    QTabWidget *msgDetailsTabs = new QTabWidget;
    QWidget *tabGeneral = new QWidget;
    QHBoxLayout *genLayout = new QHBoxLayout(tabGeneral);
    genLayout->setContentsMargins(16, 16, 16, 16);
    genLayout->setSpacing(24);
    
    // 1st col: General props
    QWidget *col1 = new QWidget;
    QVBoxLayout *col1Layout = new QVBoxLayout(col1);
    col1Layout->setContentsMargins(0,0,0,0);
    QLabel *genHeader = new QLabel("General");
    genHeader->setObjectName("headerLabel");
    
    QTableWidget *genProps = new QTableWidget(8, 2);
    genProps->horizontalHeader()->setVisible(false);
    genProps->verticalHeader()->setVisible(false);
    genProps->setShowGrid(false);
    genProps->setFocusPolicy(Qt::NoFocus);
    genProps->setStyleSheet("background-color: transparent; border: none;");
    
    auto setProp = [&](int row, const QString& k, const QString& v) {
        QTableWidgetItem *ik = new QTableWidgetItem(k); ik->setForeground(QColor("#8b8b99"));
        QTableWidgetItem *iv = new QTableWidgetItem(v); iv->setForeground(QColor("#dcdcdc"));
        genProps->setItem(row, 0, ik);
        genProps->setItem(row, 1, iv);
    };
    
    setProp(0, "Timestamp", "00:02:19.340123");
    setProp(1, "Relative Time", "00:02:19.340123");
    setProp(2, "Bus", "CAN FD (500 kbps)");
    setProp(3, "ID", "0x101");
    setProp(4, "Name", "EngineData_1");
    setProp(5, "DLC", "8");
    setProp(6, "Length", "8 bytes");
    setProp(7, "Type", "Data Frame");
    
    genProps->resizeColumnsToContents();
    genProps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    
    col1Layout->addWidget(genHeader);
    col1Layout->addWidget(genProps);
    col1Layout->addStretch();

    // 2nd col: Data
    QWidget *col2 = new QWidget;
    QVBoxLayout *col2Layout = new QVBoxLayout(col2);
    col2Layout->setContentsMargins(0,0,0,0);
    
    QLabel *hexHeader = new QLabel("Data (Hex)"); hexHeader->setObjectName("headerLabel");
    QLabel *hexData = new QLabel("3C 9A 10 27 00 64 FF 80");
    hexData->setObjectName("codeLabel");
    
    QLabel *binHeader = new QLabel("Data (Binary)"); binHeader->setObjectName("headerLabel");
    QLabel *binData = new QLabel("0011 1100 1001 1010 0001\n0000 0010 0111 0000 0000\n0110 0100 1111 1111 1000");
    binData->setObjectName("codeLabel");
    
    col2Layout->addWidget(hexHeader);
    col2Layout->addWidget(hexData);
    col2Layout->addSpacing(16);
    col2Layout->addWidget(binHeader);
    col2Layout->addWidget(binData);
    col2Layout->addStretch();

    // 3rd col: Decoded Signals
    QWidget *col3 = new QWidget;
    QVBoxLayout *col3Layout = new QVBoxLayout(col3);
    col3Layout->setContentsMargins(0,0,0,0);
    
    QLabel *sigHeader = new QLabel("Decoded Signals"); sigHeader->setObjectName("headerLabel");
    QTableWidget *sigTable = new QTableWidget(4, 5);
    sigTable->setHorizontalHeaderLabels({"Signal", "Value", "Unit", "Start Bit", "Length"});
    sigTable->verticalHeader()->setVisible(false);
    sigTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    sigTable->setShowGrid(false);
    sigTable->setAlternatingRowColors(true);
    
    auto setSigRow = [&](int row, const QStringList& data, bool hl = false) {
        for (int i = 0; i < data.size() && i < 5; ++i) {
            QTableWidgetItem *item = new QTableWidgetItem(data[i]);
            if (i > 0) item->setForeground(QColor("#dcdcdc"));
            if (i == 0 && hl) item->setForeground(QColor("#3b82f6")); // Highlight EngineSpeed
            sigTable->setItem(row, i, item);
        }
    };
    
    setSigRow(0, {"EngineSpeed", "2450", "rpm", "0", "16"}, true);
    setSigRow(1, {"EngineLoad", "64.0", "%", "16", "8"});
    setSigRow(2, {"EngineTemp", "92", "°C", "24", "8"});
    setSigRow(3, {"FuelRate", "15.6", "mg/stroke", "32", "16"});
    
    sigTable->resizeColumnsToContents();
    sigTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    
    col3Layout->addWidget(sigHeader);
    col3Layout->addWidget(sigTable);

    genLayout->addWidget(col1, 1);
    genLayout->addWidget(col2, 1);
    genLayout->addWidget(col3, 2);

    msgDetailsTabs->addTab(tabGeneral, "Message Details");
    msgDetailsTabs->addTab(new QWidget, "CAN FD");
    msgDetailsTabs->addTab(new QWidget, "Signal Decoding");

    middleSplitter->addWidget(timelineWidget);
    middleSplitter->addWidget(msgListWidget);
    middleSplitter->addWidget(msgDetailsTabs);
    
    // Adjust middle splitter sizes (approximate based on mockup)
    middleSplitter->setSizes({400, 250, 250});

    // --- RIGHT PANEL ---
    QSplitter *rightSplitter = new QSplitter(Qt::Vertical);
    rightSplitter->setChildrenCollapsible(false);
    
    // Script Editor
    QWidget *scriptWidget = new QWidget;
    QVBoxLayout *scriptLayout = new QVBoxLayout(scriptWidget);
    scriptLayout->setContentsMargins(16, 16, 16, 16);
    scriptLayout->setSpacing(10);
    
    QHBoxLayout *scriptHeaderLayout = new QHBoxLayout;
    QLabel *scriptHeader = new QLabel("ANALYZER (SCRIPT)");
    scriptHeader->setObjectName("headerLabel");
    scriptHeaderLayout->addWidget(scriptHeader);
    
    QHBoxLayout *scriptComboLayout = new QHBoxLayout;
    QComboBox *scriptCombo = new QComboBox;
    scriptCombo->addItem("Engine RPM Spike Detector");
    scriptComboLayout->addWidget(scriptCombo, 1);
    QPushButton *btnAddScript = new QPushButton("+");
    btnAddScript->setFixedWidth(30);
    scriptComboLayout->addWidget(btnAddScript);
    
    QTextEdit *scriptEditor = new QTextEdit;
    QString code = R"(<span style="color: #6a9955;">// Detect engine speed spikes</span>
<span style="color: #c586c0;">param</span> <span style="color: #9cdcfe;">max_rate</span> = <span style="color: #b5cea8;">3000</span>;  <span style="color: #6a9955;">// rpm per second</span>
<span style="color: #c586c0;">param</span> <span style="color: #9cdcfe;">min_rpm</span> = <span style="color: #b5cea8;">500</span>;
<span style="color: #c586c0;">param</span> <span style="color: #9cdcfe;">window_ms</span> = <span style="color: #b5cea8;">1000</span>;

<span style="color: #569cd6;">on_signal</span>(EngineSpeed) <span style="color: #569cd6;">as</span> s {
    <span style="color: #569cd6;">static</span> prev = s.value;
    <span style="color: #569cd6;">static</span> prev_ts = s.timestamp;

    <span style="color: #569cd6;">let</span> dt = (s.timestamp - prev_ts) / <span style="color: #b5cea8;">1000.0</span>;
    <span style="color: #c586c0;">if</span> (dt <= <span style="color: #b5cea8;">0</span>) <span style="color: #c586c0;">return</span>;
    <span style="color: #569cd6;">let</span> rate = (s.value - prev) / dt;
    <span style="color: #c586c0;">if</span> (s.value > min_rpm && abs(rate) > max_rate) {
        <span style="color: #dcdcaa;">report</span>(<span style="color: #ce9178;">"RPM_SPIKE"</span>,
            severity = <span style="color: #ce9178;">"high"</span>,
            value = s.value,
            rate = rate,
            message = <span style="color: #ce9178;">"Engine speed changed too fast"</span>);
    }
    prev = s.value;
    prev_ts = s.timestamp;
}
)";
    // Use HTML to simulate syntax highlighting as seen in mockup
    scriptEditor->setHtml(code);
    scriptEditor->setLineWrapMode(QTextEdit::NoWrap);
    
    QHBoxLayout *scriptBottom = new QHBoxLayout;
    QLabel *lnColLabel = new QLabel("Ln 18, Col 47");
    lnColLabel->setStyleSheet("color: #8b8b99;");
    scriptBottom->addWidget(lnColLabel);
    scriptBottom->addStretch();
    QLabel *errLabel = new QLabel("✓ No errors");
    errLabel->setStyleSheet("color: #22c55e;");
    scriptBottom->addWidget(errLabel);
    scriptBottom->addStretch();
    QPushButton *btnRun = new QPushButton("▶ Run");
    btnRun->setObjectName("primaryBtn");
    scriptBottom->addWidget(btnRun);

    scriptLayout->addLayout(scriptHeaderLayout);
    scriptLayout->addLayout(scriptComboLayout);
    scriptLayout->addWidget(scriptEditor, 1);
    scriptLayout->addLayout(scriptBottom);

    // Preview area
    QTabWidget *previewTabs = new QTabWidget;
    QWidget *tabPreview = new QWidget;
    QVBoxLayout *prevLayout = new QVBoxLayout(tabPreview);
    prevLayout->setContentsMargins(16, 16, 16, 16);
    prevLayout->setSpacing(12);
    
    QHBoxLayout *prevTop = new QHBoxLayout;
    QLabel *detHeader = new QLabel("Detections (8)");
    detHeader->setStyleSheet("font-weight: bold;");
    prevTop->addWidget(detHeader);
    prevTop->addStretch();
    QPushButton *btnClear = new QPushButton("Clear");
    btnClear->setObjectName("iconBtn");
    btnClear->setStyleSheet("border: 1px solid #3b3f55;");
    prevTop->addWidget(btnClear);
    
    QTableWidget *detTable = new QTableWidget(5, 4);
    detTable->setHorizontalHeaderLabels({"Time", "Analyzer", "Severity", "Message"});
    detTable->horizontalHeader()->setStretchLastSection(true);
    detTable->verticalHeader()->setVisible(false);
    detTable->setShowGrid(false);
    detTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    auto setDetRow = [&](int row, const QString& time, const QString& type, const QString& sev, const QString& msg) {
        detTable->setItem(row, 0, new QTableWidgetItem(time));
        detTable->setItem(row, 1, new QTableWidgetItem(type));
        
        QLabel *sevLabel = new QLabel(sev);
        sevLabel->setAlignment(Qt::AlignCenter);
        sevLabel->setStyleSheet("background-color: #451a1a; color: #f87171; border: 1px solid #7f1d1d; border-radius: 4px; padding: 2px 6px; font-size: 10px; font-weight: bold;");
        QWidget *sevWidget = new QWidget;
        QHBoxLayout *sevLayout = new QHBoxLayout(sevWidget);
        sevLayout->setContentsMargins(4,2,4,2);
        sevLayout->addWidget(sevLabel);
        detTable->setCellWidget(row, 2, sevWidget);
        
        detTable->setItem(row, 3, new QTableWidgetItem(msg));
    };
    
    setDetRow(0, "00:02:15.153", "RPM_SPIKE", "High", "Engine speed changed too fast");
    setDetRow(1, "00:02:18.671", "RPM_SPIKE", "High", "Engine speed changed too fast");
    setDetRow(2, "00:02:20.310", "RPM_SPIKE", "High", "Engine speed changed too fast");
    setDetRow(3, "00:02:22.145", "RPM_SPIKE", "High", "Engine speed changed too fast");
    setDetRow(4, "00:02:23.902", "RPM_SPIKE", "High", "Engine speed changed too fast");
    
    detTable->resizeColumnsToContents();
    detTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    
    QLabel *statHeader = new QLabel("Statistics");
    statHeader->setObjectName("headerLabel");
    
    QTableWidget *statTable = new QTableWidget(1, 6);
    statTable->setHorizontalHeaderLabels({"Total Detections", "High", "Medium", "Low", "First", "Last"});
    statTable->setItem(0, 0, new QTableWidgetItem("8"));
    statTable->setItem(0, 1, new QTableWidgetItem("8"));
    statTable->setItem(0, 2, new QTableWidgetItem("0"));
    statTable->setItem(0, 3, new QTableWidgetItem("0"));
    statTable->setItem(0, 4, new QTableWidgetItem("00:02:15.153"));
    statTable->setItem(0, 5, new QTableWidgetItem("00:02:23.902"));
    statTable->verticalHeader()->setVisible(false);
    statTable->setShowGrid(false);
    statTable->setFixedHeight(65);
    
    // Add timeline bar under stats as seen in mockup
    QLabel *timelineBar = new QLabel;
    timelineBar->setStyleSheet("background-color: #111218; border: 1px solid #272a35; border-radius: 4px;");
    timelineBar->setFixedHeight(20);
    // Add some pseudo markers
    QHBoxLayout *barLayout = new QHBoxLayout(timelineBar);
    barLayout->setContentsMargins(0,0,0,0);
    QLabel *marker = new QLabel("|  |    |        | | |    | |");
    marker->setStyleSheet("color: #ef4444; font-weight: bold; letter-spacing: 5px;");
    marker->setAlignment(Qt::AlignCenter);
    barLayout->addWidget(marker);

    prevLayout->addLayout(prevTop);
    prevLayout->addWidget(detTable, 1);
    prevLayout->addSpacing(10);
    prevLayout->addWidget(statHeader);
    prevLayout->addWidget(statTable);
    prevLayout->addWidget(timelineBar);

    previewTabs->addTab(tabPreview, "Preview");
    previewTabs->addTab(new QWidget, "Analyzer Output");

    rightSplitter->addWidget(scriptWidget);
    rightSplitter->addWidget(previewTabs);
    rightSplitter->setSizes({500, 400});

    // Add panels to main splitter
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(middleSplitter);
    mainSplitter->addWidget(rightSplitter);
    
    // Adjust main splitter sizes
    mainSplitter->setSizes({250, 750, 400});

    // 3. Status Bar
    QStatusBar *statusBar = new QStatusBar;
    statusBar->setStyleSheet("background-color: #181921; border-top: 1px solid #272a35; color: #8b8b99; padding: 4px;");
    
    QLabel *statusLeft = new QLabel("🟢 Trace Loaded     12,358 messages     18 ECUs     No Filters");
    QLabel *statusRight = new QLabel("Window: 10 s     Cursor: 00:02:19.350");
    
    statusBar->addWidget(statusLeft);
    statusBar->addPermanentWidget(statusRight);
    
    setStatusBar(statusBar);
}
