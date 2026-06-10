export default function SignalPanel({ signals, activeSignals, onToggle }) {
  const groups = [...new Set(signals.map(s => s.group))]

  return (
    <fieldset style={{ height: '100%', display: 'flex', flexDirection: 'column', margin: 0, overflow: 'hidden' }}>
      <legend>Signals</legend>
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {groups.map(group => (
          <div key={group} style={{ marginBottom: 8 }}>
            <div style={{ fontSize: 10, color: '#808080', fontWeight: 'bold', marginBottom: 2, textTransform: 'uppercase' }}>
              {group}
            </div>
            {signals.filter(s => s.group === group).map(signal => (
              <label key={signal.id} style={{
                display: 'flex',
                alignItems: 'center',
                gap: 4,
                fontSize: 12,
                marginBottom: 3,
                cursor: 'pointer',
              }}>
                <input
                  type="checkbox"
                  checked={activeSignals.has(signal.id)}
                  onChange={() => onToggle(signal.id)}
                />
                <span style={{ flex: 1 }}>{signal.name}</span>
                <span style={{ color: '#808080', fontSize: 10, fontFamily: 'monospace' }}>
                  {signal.value} {signal.unit}
                </span>
              </label>
            ))}
          </div>
        ))}
      </div>
      <button style={{ marginTop: 4, width: '100%' }}>Add Signal…</button>
    </fieldset>
  )
}
