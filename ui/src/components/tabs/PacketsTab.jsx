import { useRef, useEffect, useCallback } from 'react'
import { useVirtualizer } from '@tanstack/react-virtual'

const COLS = [
  { key: 'timestamp', label: 'Timestamp (s)', width: 115 },
  { key: 'type',      label: 'Type',          width: 44  },
  { key: 'channel',   label: 'Ch',            width: 32  },
  { key: 'id',        label: 'ID',            width: 68  },
  { key: 'dlc',       label: 'DLC',           width: 34  },
  { key: 'data',      label: 'Data',          width: 999 },
]

const ROW_H = 20
const FETCH_AHEAD = 80
const WINDOW_SIZE = 400

function fmtCell(packet, key) {
  if (!packet) return '…'
  if (key === 'id') return '0x' + packet.id.toString(16).toUpperCase().padStart(3, '0')
  return String(packet[key])
}

export default function PacketsTab({ totalPackets, packetWindow, onRequestWindow }) {
  const parentRef = useRef(null)
  const onRequestRef = useRef(onRequestWindow)
  useEffect(() => { onRequestRef.current = onRequestWindow }, [onRequestWindow])

  const { offset, rows } = packetWindow

  const virtualizer = useVirtualizer({
    count: totalPackets,
    getScrollElement: () => parentRef.current,
    estimateSize: () => ROW_H,
    overscan: 30,
  })

  const items = virtualizer.getVirtualItems()
  const firstIdx = items[0]?.index ?? 0
  const lastIdx  = items[items.length - 1]?.index ?? 0

  useEffect(() => {
    if (items.length === 0) return
    const windowEnd = offset + rows.length
    const needLeft  = firstIdx < offset + FETCH_AHEAD && offset > 0
    const needRight = lastIdx  > windowEnd - FETCH_AHEAD
    if (needLeft || needRight) {
      const newOffset = Math.max(0, firstIdx - FETCH_AHEAD)
      onRequestRef.current(newOffset, WINDOW_SIZE)
    }
  }, [firstIdx, lastIdx]) // eslint-disable-line react-hooks/exhaustive-deps

  const rowBg = useCallback((idx, type) => {
    if (type === 'ETH') return idx % 2 === 0 ? '#e8eeff' : '#dde8ff'
    return idx % 2 === 0 ? '#ffffff' : '#f5f5f5'
  }, [])

  if (totalPackets === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: '#808080', fontSize: 11 }}>
        No trace loaded
      </div>
    )
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', minHeight: 0 }}>
      {/* Fixed header */}
      <div style={{ display: 'flex', background: '#c0c0c0', borderBottom: '2px solid #808080', flexShrink: 0 }}>
        {COLS.map(col => (
          <div key={col.key} style={{
            width: col.key === 'data' ? undefined : col.width,
            flex: col.key === 'data' ? 1 : undefined,
            padding: '2px 4px',
            fontWeight: 'bold',
            fontSize: 11,
            borderRight: '1px solid #808080',
            whiteSpace: 'nowrap',
            overflow: 'hidden',
          }}>
            {col.label}
          </div>
        ))}
      </div>

      {/* Virtualized rows — minHeight:0 lets flex shrink this below content height so the
          scroll container stays bounded and TanStack Virtual sees the correct viewport height */}
      <div ref={parentRef} style={{ flex: 1, overflow: 'auto', minHeight: 0 }}>
        <div style={{ height: virtualizer.getTotalSize(), position: 'relative' }}>
          {items.map(vRow => {
            const packet = rows[vRow.index - offset]
            return (
              <div key={vRow.key} style={{
                position: 'absolute',
                top: vRow.start,
                left: 0,
                right: 0,
                height: ROW_H,
                display: 'flex',
                alignItems: 'center',
                background: rowBg(vRow.index, packet?.type),
                borderBottom: '1px solid #e8e8e8',
              }}>
                {COLS.map(col => (
                  <div key={col.key} style={{
                    width: col.key === 'data' ? undefined : col.width,
                    flex: col.key === 'data' ? 1 : undefined,
                    padding: '0 4px',
                    fontSize: 11,
                    fontFamily: 'monospace',
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    borderRight: '1px solid #e0e0e0',
                    color: !packet ? '#bbb'
                      : col.key === 'type' && packet.type === 'ETH' ? '#000080'
                      : '#000',
                  }}>
                    {fmtCell(packet, col.key)}
                  </div>
                ))}
              </div>
            )
          })}
        </div>
      </div>

      <div style={{ padding: '2px 6px', fontSize: 10, borderTop: '1px solid #808080', background: '#c0c0c0', flexShrink: 0 }}>
        {totalPackets.toLocaleString()} packets — showing rows {offset.toLocaleString()}–{(offset + rows.length).toLocaleString()}
      </div>
    </div>
  )
}
