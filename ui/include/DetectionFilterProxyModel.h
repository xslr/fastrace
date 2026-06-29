#pragma once
#include <QSortFilterProxyModel>
#include <QString>

class DetectionFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit DetectionFilterProxyModel(QObject* parent = nullptr);

    void setSeverityFilter(const QString& severity);
    void setDetectorFilter(const QString& detector);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
    QString m_severityFilter = "All";
    QString m_detectorFilter = "All";
};
