import { useState } from 'react'
import SignalPanel from './SignalPanel.jsx'
import PacketsTab from './tabs/PacketsTab.jsx'
import AnalyzersTab from './tabs/AnalyzersTab.jsx'
import TraceTab from './tabs/TraceTab.jsx'

const TABS = [
  { id: 'packets',   label: 'Packets'   },
  { id: 'analyzers', label: 'Analyzers' },
  { id: 'trace',     label: 'Trace'     },
]

export default function MainLayout({ viewModel, send }) {
  const [activeTab, setActiveTab] = useState('packets')
  const { status, tracePath, signals, activeSignals, totalPackets, packetWindow, analyzers } = viewModel
  const fileName = tracePath.split(/[/\\]/).pop()

  return (
    <div className="window" style={{ width: '100vw', height: '100vh', display: 'flex', flexDirection: 'column' }}>
      <div className="title-bar">
        <div className="title-bar-text">Fastrace — {fileName}</div>
        <div className="title-bar-controls">
          <button aria-label="Minimize" />
          <button aria-label="Maximize" />
          <button aria-label="Close" />
        </div>
      </div>

      {/* Menu bar */}
      <div style={{ display: 'flex', background: '#c0c0c0', padding: '1px 2px', borderBottom: '1px solid #808080', gap: 1, flexShrink: 0 }}>
        {['File', 'View', 'Analyzers', 'Help'].map(label => (
          <button key={label} style={{ padding: '1px 8px', fontSize: 12 }}>{label}</button>
        ))}
      </div>

      <div className="window-body" style={{ flex: 1, overflow: 'hidden', padding: 4, display: 'flex', flexDirection: 'column', gap: 4 }}>
        {status === 'loading' && (
          <div style={{ padding: '2px 4px', fontSize: 11, background: '#ffff80', border: '1px solid #c0c000' }}>
            Loading trace file…
          </div>
        )}

        <div style={{ display: 'flex', flex: 1, overflow: 'hidden', gap: 4 }}>
          {/* Signal sidebar */}
          <div style={{ width: 220, flexShrink: 0, overflow: 'hidden' }}>
            <SignalPanel
              signals={signals}
              activeSignals={activeSignals}
              onToggle={id => send({ type: 'toggle_signal', signalId: id })}
            />
          </div>

          {/* Tab area */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            <div role="tablist" style={{ display: 'flex', gap: 2, flexShrink: 0 }}>
              {TABS.map(tab => (
                <button
                  key={tab.id}
                  role="tab"
                  aria-selected={activeTab === tab.id}
                  onClick={() => setActiveTab(tab.id)}
                  style={{ padding: '2px 14px', fontSize: 12 }}
                >
                  {tab.label}
                </button>
              ))}
            </div>

            <div className="window" style={{ flex: 1, overflow: 'hidden', marginTop: 2, display: 'flex', flexDirection: 'column' }}>
              <div className="window-body" style={{ flex: 1, padding: 0, overflow: 'hidden', minHeight: 0 }}>
                {activeTab === 'packets' && (
                  <PacketsTab
                    totalPackets={totalPackets}
                    packetWindow={packetWindow}
                    onRequestWindow={(offset, count) => send({ type: 'request_packets', offset, count })}
                  />
                )}
                {activeTab === 'analyzers' && (
                  <AnalyzersTab
                    analyzers={analyzers}
                    onRun={id => send({ type: 'run_analyzer', id })}
                    onCodeChange={(id, code) => send({ type: 'update_analyzer_code', id, code })}
                    onAdd={() => send({ type: 'add_analyzer' })}
                  />
                )}
                {activeTab === 'trace' && (
                  <TraceTab signals={signals} activeSignals={activeSignals} />
                )}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Status bar */}
      <div className="status-bar">
        <p className="status-bar-field" style={{ flex: 1 }}>
          {status === 'loading' ? 'Loading…' : `Ready — ${fileName}`}
        </p>
        <p className="status-bar-field">{totalPackets.toLocaleString()} packets</p>
        <p className="status-bar-field">{signals.length} signals</p>
      </div>
    </div>
  )
}
