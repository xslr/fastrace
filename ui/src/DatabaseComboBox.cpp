#include "DatabaseComboBox.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionComboBox>

DatabaseComboBox::DatabaseComboBox(QWidget* parent)
    : QComboBox(parent)
{
}

void DatabaseComboBox::setActiveCount(int count)
{
    if (m_activeCount == count) {
        return;
    }
    m_activeCount = count;
    update(); // trigger repaint
}

void DatabaseComboBox::paintEvent(QPaintEvent* /*event*/)
{
    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    // Override the display text based on active count
    if (m_activeCount > 0) {
        opt.currentText
            = tr("%1 database%2").arg(m_activeCount).arg(m_activeCount == 1 ? QString() : QStringLiteral("s"));
    } else {
        opt.currentText = tr("Select databases (ARXML)");
    }

    // Clear the icon so we only show text
    opt.currentIcon = QIcon();

    QPainter painter(this);
    style()->drawComplexControl(QStyle::CC_ComboBox, &opt, &painter, this);
    style()->drawControl(QStyle::CE_ComboBoxLabel, &opt, &painter, this);
}
