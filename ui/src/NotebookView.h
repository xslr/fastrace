#pragma once
#include <QWidget>
#include <QList>

class QScrollArea;
class QVBoxLayout;
class NotebookBlockWidget;
class TimelineWidget;
class MessageListWidget;
class ScriptEditorWidget;
class QPushButton;

/**
 * NotebookView presents blocks arranged vertically in a scrollable canvas.
 *
 * It holds:
 *   - Signal View  (shared TimelineWidget, re-parented here when this view is active)
 *   - Message List (its own instance so filters are independent of the Overview)
 *   - Script Editor + Analyzer Output stub
 *
 * The toolbar at the top of the canvas lets the user add more blocks.
 * Each block wraps its content in a NotebookBlockWidget that supports
 * collapsing and deletion.  Drag-and-drop reordering moves blocks within
 * the canvas VBoxLayout.
 */
class NotebookView : public QWidget {
    Q_OBJECT
public:
    /**
     * @param sharedTimeline  The single TimelineWidget instance shared across views.
     *                        NotebookView will re-parent it when activated.
     * @param scriptEditor    ScriptEditorWidget owned exclusively by the Notebook.
     * @param parent          Parent widget.
     */
    explicit NotebookView(TimelineWidget   *sharedTimeline,
                          ScriptEditorWidget *scriptEditor,
                          QWidget          *parent = nullptr);

    /**
     * Call this when the Notebook view becomes active so that the shared
     * TimelineWidget is re-parented into this layout.
     */
    void activate();

    /**
     * Call this before switching away so the shared TimelineWidget can be
     * removed without being destroyed.
     */
    void deactivate();

    /** Expose the notebook's own MessageListWidget for signal connections. */
    MessageListWidget *messageList() const { return m_messageList; }

private slots:
    void onBlockDeleteRequested(NotebookBlockWidget *block);
    void onBlockDragStarted(NotebookBlockWidget *block);
    void addSignalViewBlock();
    void addMessageListBlock();
    void addScriptBlock();
    void addMarkdownBlock();

private:
    NotebookBlockWidget *wrapInBlock(const QString &title, QWidget *content);
    void                 appendBlock(NotebookBlockWidget *block);

    // ── Shared / owned widgets ────────────────────────────────────────────────
    TimelineWidget     *m_timeline      = nullptr;  ///< shared, re-parented
    MessageListWidget  *m_messageList   = nullptr;  ///< owned by this view
    ScriptEditorWidget *m_scriptEditor  = nullptr;  ///< owned by this view

    // ── Canvas ────────────────────────────────────────────────────────────────
    QScrollArea  *m_scrollArea   = nullptr;
    QWidget      *m_canvas       = nullptr;
    QVBoxLayout  *m_canvasLayout = nullptr;

    // ── Toolbar ───────────────────────────────────────────────────────────────
    QPushButton  *m_addSignalViewBtn = nullptr;
    QPushButton  *m_addMsgListBtn    = nullptr;
    QPushButton  *m_addScriptBtn     = nullptr;
    QPushButton  *m_addMarkdownBtn   = nullptr;
    QPushButton  *m_runAllBtn        = nullptr;

    // ── Block tracking ────────────────────────────────────────────────────────
    QList<NotebookBlockWidget *> m_blocks;

    // ── Drag state ────────────────────────────────────────────────────────────
    NotebookBlockWidget *m_dragBlock = nullptr;
};
