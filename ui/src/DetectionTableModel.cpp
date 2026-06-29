#include "DetectionTableModel.h"

DetectionTableModel::DetectionTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int DetectionTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_detections.size());
}

int DetectionTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 4; // Time, Detector, Severity, Message
}

QVariant DetectionTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_detections.size())) {
        return QVariant();
    }

    const auto& d = m_detections[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return QString::number(d.timestampUs / 1000000.0, 'f', 6);
        case 1:
            return QString::fromStdString(d.detectorName);
        case 2:
            switch (d.severity) {
            case Severity::Info:
                return "Info";
            case Severity::Warning:
                return "Warning";
            case Severity::Error:
                return "Error";
            }
            break;
        case 3:
            return QString::fromStdString(d.message);
        }
    }
    return QVariant();
}

QVariant DetectionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return "Time";
        case 1:
            return "Detector";
        case 2:
            return "Severity";
        case 3:
            return "Message";
        }
    }
    return QVariant();
}

void DetectionTableModel::setDetections(const std::vector<Detection>& detections)
{
    beginResetModel();
    m_detections = detections;
    endResetModel();
}

void DetectionTableModel::clear()
{
    beginResetModel();
    m_detections.clear();
    endResetModel();
}

const Detection& DetectionTableModel::detectionAt(int row) const { return m_detections.at(row); }
