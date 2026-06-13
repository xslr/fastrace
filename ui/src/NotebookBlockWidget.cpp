#include "NotebookBlockWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QApplication>

NotebookBlockWidget::NotebookBlockWidget(const QString &title,
                                         QWidget *contentWidget,
                                         QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_content(contentWidget)
{
    // Outer layout: header + content, no margins so the border looks clean
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(0, 0, 0, 8);
    m_outerLayout->setSpacing(0);

    buildHeader();

    // Content widget goes directly below the header
    m_content->setParent(this);
    m_outerLayout->addWidget(m_content);
}

void NotebookBlockWidget::buildHeader()
{
    m_headerFrame = new QFrame(this);
    m_headerFrame->setObjectName("blockHeader");
    m_headerFrame->setFixedHeight(34);
    m_headerFrame->setCursor(Qt::SizeAllCursor);

    auto *headerLayout = new QHBoxLayout(m_headerFrame);
    headerLayout->setContentsMargins(8, 0, 8, 0);
    headerLayout->setSpacing(4);

    // Drag handle
    m_handleLabel = new QLabel("⠿", m_headerFrame);
    m_handleLabel->setObjectName("dragHandle");
    m_handleLabel->setToolTip(tr("Drag to reorder"));
    m_handleLabel->setCursor(Qt::SizeAllCursor);
    headerLayout->addWidget(m_handleLabel);

    // Left colour accent bar
    auto *accent = new QFrame(m_headerFrame);
    accent->setObjectName("blockAccent");
    accent->setFixedWidth(3);
    accent->setFixedHeight(18);
    headerLayout->addWidget(accent);

    // Title
    auto *titleLabel = new QLabel(m_title, m_headerFrame);
    titleLabel->setObjectName("blockTitle");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    // Run button
    m_runBtn = new QPushButton("▶ Run", m_headerFrame);
    m_runBtn->setObjectName("blockRunBtn");
    m_runBtn->setFixedHeight(24);
    headerLayout->addWidget(m_runBtn);

    // Collapse button
    m_collapseBtn = new QPushButton("▼", m_headerFrame);
    m_collapseBtn->setObjectName("blockIconBtn");
    m_collapseBtn->setFixedSize(24, 24);
    m_collapseBtn->setToolTip(tr("Collapse / expand"));
    connect(m_collapseBtn, &QPushButton::clicked, this, &NotebookBlockWidget::toggleCollapse);
    headerLayout->addWidget(m_collapseBtn);

    // Delete button
    m_deleteBtn = new QPushButton("✕", m_headerFrame);
    m_deleteBtn->setObjectName("blockIconBtn");
    m_deleteBtn->setFixedSize(24, 24);
    m_deleteBtn->setToolTip(tr("Remove block"));
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(this);
    });
    headerLayout->addWidget(m_deleteBtn);

    m_outerLayout->addWidget(m_headerFrame);
}

void NotebookBlockWidget::toggleCollapse()
{
    m_collapsed = !m_collapsed;
    m_content->setVisible(!m_collapsed);
    m_collapseBtn->setText(m_collapsed ? "▶" : "▼");
}

// ── Drag-and-drop wiring ──────────────────────────────────────────────────────

void NotebookBlockWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStart = event->pos();
        m_dragging  = false;
    }
    QWidget::mousePressEvent(event);
}

void NotebookBlockWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (!m_dragging &&
        (event->pos() - m_dragStart).manhattanLength()
            >= QApplication::startDragDistance()) {
        m_dragging = true;
        emit dragStarted(this);
    }
    QWidget::mouseMoveEvent(event);
}
