#pragma once
#include <QComboBox>

/**
 * DatabaseComboBox is a QComboBox subclass that overrides the collapsed
 * display text to show a summary of how many signal databases are currently
 * active (checked), rather than the standard "current item" text.
 *
 * Display rules:
 *   - 0 active  →  shows placeholder text "Select databases (ARXML)"
 *   - N active  →  shows "N databases"  (e.g. "2 databases")
 *
 * The active count is set externally via setActiveCount().  The widget does
 * not inspect the model itself — TopBarWidget remains the single source of
 * truth for which items are checked.
 */
class DatabaseComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit DatabaseComboBox(QWidget* parent = nullptr);

    /** Update the collapsed display; triggers a repaint. */
    void setActiveCount(int count);

    int activeCount() const { return m_activeCount; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_activeCount = 0;
};
