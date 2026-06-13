#pragma once
#include <QWidget>
#include <QString>

class QVBoxLayout;
class QPushButton;
class QLabel;
class QFrame;

/**
 * NotebookBlockWidget wraps any content widget with a header bar that provides:
 *  - A drag handle (≡) for drag-and-drop reordering
 *  - A title label
 *  - A "Run" button
 *  - A collapse/expand toggle (▼/▶)
 *  - A close/delete button (✕)
 *
 * The parent NotebookView is responsible for the actual drag-and-drop
 * reordering logic; this widget just emits the relevant signals.
 */
class NotebookBlockWidget : public QWidget {
    Q_OBJECT
public:
    explicit NotebookBlockWidget(const QString &title,
                                 QWidget *contentWidget,
                                 QWidget *parent = nullptr);

    QWidget *contentWidget() const { return m_content; }
    QString  title()         const { return m_title; }

signals:
    /** Emitted when the user clicks the ✕ button. */
    void deleteRequested(NotebookBlockWidget *self);

    /** Emitted when the user starts dragging the handle. */
    void dragStarted(NotebookBlockWidget *self);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event)  override;

private slots:
    void toggleCollapse();

private:
    void buildHeader();

    QString      m_title;
    QWidget     *m_content      = nullptr;
    QFrame      *m_headerFrame  = nullptr;
    QPushButton *m_collapseBtn  = nullptr;
    QPushButton *m_deleteBtn    = nullptr;
    QPushButton *m_runBtn       = nullptr;
    QLabel      *m_handleLabel  = nullptr;
    QVBoxLayout *m_outerLayout  = nullptr;

    bool    m_collapsed   = false;
    QPoint  m_dragStart;
    bool    m_dragging    = false;
};
