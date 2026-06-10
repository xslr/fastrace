export default function TraceTab({ signals, activeSignals }) {
  const active = signals.filter(s => activeSignals.has(s.id))

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', padding: 8 }}>
      {active.length === 0 ? (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', flex: 1 }}>
          <div className="window" style={{ width: 360 }}>
            <div className="title-bar"><div className="title-bar-text">Trace View</div></div>
            <div className="window-body">
              <p style={{ color: '#808080', textAlign: 'center', margin: 0 }}>
                No signals selected.<br />Check signals in the panel on the left to view their trace.
              </p>
            </div>
          </div>
        </div>
      ) : (
        active.map(signal => (
          <fieldset key={signal.id} style={{ marginBottom: 8 }}>
            <legend>{signal.name} ({signal.unit})</legend>
            <div style={{
              height: 80,
              background: '#fff',
              border: '2px inset #fff',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: '#808080',
              fontSize: 11,
            }}>
              Waveform rendering — coming soon
            </div>
          </fieldset>
        ))
      )}
    </div>
  )
}
