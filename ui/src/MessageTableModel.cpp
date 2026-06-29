#include "MessageTableModel.h"

#include <QColor>
#include <QFont>
#include <QtConcurrent>

#include "BlfTypes.h"

MessageTableModel::MessageTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    // Connect the async chunk decoder request to a concurrent worker
    connect(
        this, &MessageTableModel::chunkDecodeRequested, this,
        [this](size_t chunkIndex) {
            if (!m_analyzer) {
                return;
            }

            // Check if it was already resolved by another request
            if (m_cache.find(chunkIndex) != m_cache.end()) {
                return;
            }

            // Run decoding in a background thread
            auto analyzer = m_analyzer;
            (void)QtConcurrent::run([this, analyzer, chunkIndex]() {
                auto msgs = analyzer->decodeChunk(chunkIndex);

                // Convert to QVector to safely pass across thread boundaries via
                // signals
                QVector<fastrace::TraceMessage> qmsgs(msgs.begin(), msgs.end());

                // Invoke slot on the main GUI thread
                QMetaObject::invokeMethod(this, "onChunkDecoded", Qt::QueuedConnection, Q_ARG(size_t, chunkIndex),
                    Q_ARG(QVector<fastrace::TraceMessage>, qmsgs));
            });
        },
        Qt::QueuedConnection);
}

void MessageTableModel::setAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer)
{
    beginResetModel();
    m_analyzer = analyzer;
    m_cache.clear();
    m_pending.clear();
    endResetModel();
}

void MessageTableModel::clear()
{
    beginResetModel();
    m_analyzer.reset();
    m_cache.clear();
    m_pending.clear();
    endResetModel();
}

int MessageTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_analyzer) {
        return 0;
    }
    return static_cast<int>(m_analyzer->totalMessages());
}

int MessageTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 8; // Time, Bus, ID, Name, DLC, Data, Len, ECU
}

QVariant MessageTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        static const QStringList headers = { "Time", "Bus", "ID", "Name", "DLC", "Data", "Len", "ECU" };
        if (section >= 0 && section < headers.size()) {
            return headers[section];
        }
    } else {
        // Row numbers
        return QString::number(section + 1);
    }
    return QVariant();
}

QVariant MessageTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !m_analyzer) {
        return QVariant();
    }

    if (role == Qt::FontRole && index.column() == 5) {
        return QFont("Consolas", 9);
    }

    if (role != Qt::DisplayRole && role != Qt::BackgroundRole && role != Qt::ForegroundRole) {
        return QVariant();
    }

    size_t row = static_cast<size_t>(index.row());
    size_t chunkIndex = row / fastrace::Analyzer::CHUNK_SIZE;
    size_t inChunkIdx = row % fastrace::Analyzer::CHUNK_SIZE;

    auto it = m_cache.find(chunkIndex);
    if (it == m_cache.end()) {
        if (m_pending.find(chunkIndex) == m_pending.end()) {
            m_pending.insert(chunkIndex);
            emit chunkDecodeRequested(chunkIndex);
        }

        if (role == Qt::DisplayRole && index.column() == 5) {
            return "Load...";
        }
        return QVariant();
    }

    const auto& chunkMsgs = it->second;
    if (inChunkIdx >= chunkMsgs.size()) {
        return QVariant();
    }

    const auto& msg = chunkMsgs[inChunkIdx];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: {
            const uint64_t us = msg.timestampUs;
            const int h = static_cast<int>(us / 3'600'000'000ull);
            const int m = static_cast<int>((us % 3'600'000'000ull) / 60'000'000ull);
            const int s = static_cast<int>((us % 60'000'000ull) / 1'000'000ull);
            const int us6 = static_cast<int>(us % 1'000'000ull);
            return QString("%1:%2:%3.%4")
                .arg(h, 2, 10, QChar('0'))
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0'))
                .arg(us6, 6, 10, QChar('0'));
        }
        case 1: {
            switch (static_cast<BLFObjectType>(msg.objectType)) {
            case CAN_MESSAGE:
            case CAN_MESSAGE2:
                return "CAN";
            case CAN_FD_MESSAGE:
            case CAN_FD_MESSAGE_64:
                return "CAN FD";
            case ETHERNET_FRAME_EX:
            case ETHERNET_FRAME_FORWARDED:
                return "Ethernet";
            default:
                return "Other";
            }
        }
        case 2: {
            if (msg.objectType == ETHERNET_FRAME_EX || msg.objectType == ETHERNET_FRAME_FORWARDED) {
                return QString("CH%1").arg(msg.channel);
            } else if (msg.objectType == CAN_MESSAGE || msg.objectType == CAN_MESSAGE2
                || msg.objectType == CAN_FD_MESSAGE || msg.objectType == CAN_FD_MESSAGE_64) {
                QString idStr = "0x" + QString::number(msg.arbId, 16).toUpper();
                if (msg.extendedId) {
                    idStr += "x";
                }
                return idStr;
            } else {
                return QString();
            }
        }
        case 3: {
            // For non-CAN/ETH objects, we can show the object type name here.
            std::string_view name = fastrace::objectTypeName(msg.objectType);
            if (!name.empty()) {
                return QString::fromUtf8(name.data(), name.size());
            }
            return QString("Type %1").arg(msg.objectType);
        }
        case 4:
            return QString::number(msg.dlc);
        case 5: {
            QString dataHex;
            dataHex.reserve(msg.dataLen * 3);
            for (int i = 0; i < msg.dataLen; ++i) {
                if (i > 0) {
                    dataHex += ' ';
                }
                dataHex += QString("%1").arg(msg.data[i], 2, 16, QChar('0')).toUpper();
            }
            return dataHex;
        }
        case 6:
            return QString::number(msg.dataLen);
        case 7:
            return QString("CH%1").arg(msg.channel); // ECU
        }
    }

    return QVariant();
}

void MessageTableModel::onChunkDecoded(size_t chunkIndex, QVector<fastrace::TraceMessage> messages)
{
    m_pending.erase(chunkIndex);

    // Manage cache size
    if (m_cache.size() >= MAX_CACHED_CHUNKS) {
        // simple eviction: remove an entry that is furthest from chunkIndex
        auto furthestIt = m_cache.begin();
        size_t maxDist = 0;
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            size_t dist = (it->first > chunkIndex) ? (it->first - chunkIndex) : (chunkIndex - it->first);
            if (dist > maxDist) {
                maxDist = dist;
                furthestIt = it;
            }
        }
        if (furthestIt != m_cache.end()) {
            m_cache.erase(furthestIt);
        }
    }

    m_cache[chunkIndex] = std::vector<fastrace::TraceMessage>(messages.begin(), messages.end());

    // Emit dataChanged for the affected rows
    int startRow = static_cast<int>(chunkIndex * fastrace::Analyzer::CHUNK_SIZE);
    int endRow = startRow + static_cast<int>(messages.size()) - 1;

    emit dataChanged(index(startRow, 0), index(endRow, columnCount() - 1));
}

std::optional<fastrace::TraceMessage> MessageTableModel::getMessage(int row) const
{
    if (row < 0 || !m_analyzer) {
        return std::nullopt;
    }

    size_t urow = static_cast<size_t>(row);
    size_t chunkIndex = urow / fastrace::Analyzer::CHUNK_SIZE;
    size_t inChunkIdx = urow % fastrace::Analyzer::CHUNK_SIZE;

    auto it = m_cache.find(chunkIndex);
    if (it == m_cache.end()) {
        // If not cached, we could synchronously decode it, or request async and
        // return nullopt. Since selection usually happens after rendering, it
        // should be cached. If not, we'll request decode and it will arrive later,
        // but returning nullopt for now.
        if (m_pending.find(chunkIndex) == m_pending.end()) {
            m_pending.insert(chunkIndex);
            emit chunkDecodeRequested(chunkIndex);
        }
        return std::nullopt;
    }

    const auto& chunkMsgs = it->second;
    if (inChunkIdx >= chunkMsgs.size()) {
        return std::nullopt;
    }

    return chunkMsgs[inChunkIdx];
}
