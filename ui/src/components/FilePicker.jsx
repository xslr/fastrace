import { useState } from 'react'

export default function FilePicker({ onLoad }) {
  const [tracePath, setTracePath] = useState('C:\\traces\\capture.blf')
  const [arxmlPath, setArxmlPath] = useState('C:\\defs\\signals.arxml')

  return (
    <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '100vh', background: '#008080' }}>
      <div className="window" style={{ width: 500 }}>
        <div className="title-bar">
          <div className="title-bar-text">Fastrace — Open Trace</div>
          <div className="title-bar-controls">
            <button aria-label="Close" />
          </div>
        </div>
        <div className="window-body">
          <p style={{ marginTop: 0 }}>Select a trace file and signal definition to begin analysis.</p>
          <fieldset style={{ marginBottom: 8 }}>
            <legend>Trace File (.blf)</legend>
            <div style={{ display: 'flex', gap: 4 }}>
              <input
                type="text"
                value={tracePath}
                onChange={e => setTracePath(e.target.value)}
                style={{ flex: 1 }}
              />
              <button>Browse...</button>
            </div>
          </fieldset>
          <fieldset style={{ marginBottom: 12 }}>
            <legend>Signal Definition (.arxml)</legend>
            <div style={{ display: 'flex', gap: 4 }}>
              <input
                type="text"
                value={arxmlPath}
                onChange={e => setArxmlPath(e.target.value)}
                style={{ flex: 1 }}
              />
              <button>Browse...</button>
            </div>
          </fieldset>
          <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 4 }}>
            <button onClick={() => onLoad(tracePath, arxmlPath)} disabled={!tracePath || !arxmlPath}>
              Load
            </button>
            <button>Cancel</button>
          </div>
        </div>
      </div>
    </div>
  )
}
