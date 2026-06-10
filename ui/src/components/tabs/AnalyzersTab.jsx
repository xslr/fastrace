import AnalyzerCell from '../analyzer/AnalyzerCell.jsx'

export default function AnalyzersTab({ analyzers, onRun, onCodeChange, onAdd }) {
  return (
    <div style={{ height: '100%', overflowY: 'auto', padding: 8 }}>
      <div style={{ marginBottom: 8 }}>
        <button onClick={onAdd}>+ New Analyzer</button>
      </div>
      {analyzers.length === 0 && (
        <p style={{ color: '#808080', fontSize: 12 }}>No analyzers. Click "+ New Analyzer" to create one.</p>
      )}
      {analyzers.map(a => (
        <AnalyzerCell
          key={a.id}
          analyzer={a}
          onRun={onRun}
          onCodeChange={onCodeChange}
        />
      ))}
    </div>
  )
}
