#pragma once
#include "Detection.h"
#include <QAbstractTableModel>
#include <vector>

class DetectionTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit DetectionTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setDetections(const std::vector<Detection>& detections);
    const Detection& detectionAt(int row) const;
    void clear();

private:
    std::vector<Detection> m_detections;
};
