#include "DetectionFilterProxyModel.h"
#include "DetectionTableModel.h"

DetectionFilterProxyModel::DetectionFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void DetectionFilterProxyModel::setSeverityFilter(const QString& severity)
{
    m_severityFilter = severity;
    invalidateFilter();
}

void DetectionFilterProxyModel::setDetectorFilter(const QString& detector)
{
    m_detectorFilter = detector;
    invalidateFilter();
}

bool DetectionFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if (!sourceModel()) {
        return false;
    }

    QModelIndex detectorIdx = sourceModel()->index(source_row, 1, source_parent);
    QModelIndex severityIdx = sourceModel()->index(source_row, 2, source_parent);

    QString detector = sourceModel()->data(detectorIdx).toString();
    QString severity = sourceModel()->data(severityIdx).toString();

    bool severityMatch = (m_severityFilter == "All" || severity == m_severityFilter);
    bool detectorMatch = (m_detectorFilter == "All" || detector == m_detectorFilter);

    return severityMatch && detectorMatch;
}
