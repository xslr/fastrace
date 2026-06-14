#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <map>
#include <set>
#include <optional>
#include <memory>
#include "Analyzer.h"
#include "TraceMessage.h"

class MessageTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit MessageTableModel(QObject* parent = nullptr);

    void setAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    std::optional<fastrace::TraceMessage> getMessage(int row) const;

signals:
    void chunkDecodeRequested(size_t chunkIndex) const; // Needs to be const if emitted from data()

private slots:
    void onChunkDecoded(size_t chunkIndex, QVector<fastrace::TraceMessage> messages);

private:
    static constexpr size_t MAX_CACHED_CHUNKS = 3;

    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    
    // Mutable because they are modified in the const data() method when cache misses
    mutable std::map<size_t, std::vector<fastrace::TraceMessage>> m_cache;
    mutable std::set<size_t> m_pending;
};
