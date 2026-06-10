import { useState } from 'react'
import AnalyzerEditor from './AnalyzerEditor.jsx'

const LANG_LABEL = { python: 'PY', javascript: 'JS', lua: 'LUA' }
const LANG_COLOR = { python: '#3572A5', javascript: '#c8a400', lua: '#000080' }

export default function AnalyzerCell({ analyzer, onRun, onCodeChange }) {
  const [showCode, setShowCode] = useState(true)

  return (
    <fieldset style={{ marginBottom: 8 }}>
      <legend style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
        <span style={{
          background: LANG_COLOR[analyzer.language],
          color: '#fff',
          padding: '0 5px',
          fontSize: 10,
          fontFamily: 'monospace',
          fontWeight: 'bold',
        }}>
          {LANG_LABEL[analyzer.language]}
        </span>
        {analyzer.name}
      </legend>

      <div style={{ display: 'flex', gap: 4, marginBottom: 6 }}>
        <button onClick={() => onRun(analyzer.id)} disabled={analyzer.running}>
          {analyzer.running ? 'Running…' : '▶ Run'}
        </button>
        <button onClick={() => setShowCode(s => !s)}>
          {showCode ? 'Hide Code' : 'Show Code'}
        </button>
      </div>

      {showCode && (
        <AnalyzerEditor
          code={analyzer.code}
          language={analyzer.language}
          onChange={code => onCodeChange(analyzer.id, code)}
        />
      )}

      {analyzer.errors.length > 0 && (
        <pre style={{
          margin: '4px 0 0',
          padding: 4,
          background: '#ff0000',
          color: '#fff',
          fontSize: 11,
          fontFamily: 'monospace',
        }}>
          {analyzer.errors.join('\n')}
        </pre>
      )}

      {analyzer.output && (
        <div style={{ marginTop: 6 }}>
          <div style={{ fontSize: 10, color: '#808080', marginBottom: 2 }}>Output</div>
          <pre style={{
            margin: 0,
            padding: 6,
            background: '#fff',
            border: '2px inset #fff',
            fontSize: 11,
            fontFamily: 'monospace',
            whiteSpace: 'pre-wrap',
          }}>
            {analyzer.output.data ?? JSON.stringify(analyzer.output, null, 2)}
          </pre>
        </div>
      )}
    </fieldset>
  )
}
