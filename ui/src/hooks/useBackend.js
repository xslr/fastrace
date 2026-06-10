import { useState, useCallback, useEffect, useRef } from 'react'
import { MOCK_SIGNALS, MOCK_ANALYZERS } from '../mock/data.js'

const EMPTY_WINDOW = { offset: 0, rows: [] }

export function useBackend() {
  const [viewModel, setViewModel] = useState(null)
  const evsRef = useRef(null)

  const connect = useCallback((_tracePath, _arxmlPath) => {
    // Seed the full viewModel shape immediately so MainLayout renders without throwing.
    // Signals and analyzers stay client-local for this scaffold; only packets come from the server.
    setViewModel({
      status: 'loading',
      tracePath: _tracePath || '(connecting…)',
      arxmlPath: _arxmlPath || '',
      signals: MOCK_SIGNALS,
      activeSignals: new Set([1, 2]),
      totalPackets: 0,
      packetWindow: EMPTY_WINDOW,
      analyzers: MOCK_ANALYZERS,
    })

    if (evsRef.current) evsRef.current.close()
    const evs = new EventSource('/api/events')
    evsRef.current = evs

    evs.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data)
        if (msg.type === 'status_update') {
          setViewModel(vm => ({
            ...vm,
            status: msg.status ?? vm.status,
            totalPackets: msg.totalPackets ?? vm.totalPackets,
            tracePath: msg.tracePath ?? vm.tracePath,
          }))
        }
      } catch (_) {}
    }

    evs.onerror = () => { /* EventSource auto-reconnects */ }
  }, [])

  useEffect(() => () => { evsRef.current?.close() }, [])

  const send = useCallback((msg) => {
    switch (msg.type) {
      case 'request_packets':
        fetch(`/api/packets?offset=${msg.offset}&count=${msg.count}`)
          .then(r => r.json())
          .then(rows => setViewModel(vm => ({
            ...vm,
            packetWindow: { offset: msg.offset, rows },
          })))
          .catch(() => {})
        break

      case 'run_analyzer':
        setViewModel(vm => ({
          ...vm,
          analyzers: vm.analyzers.map(a =>
            a.id === msg.id ? { ...a, running: true, output: null, errors: [] } : a
          ),
        }))
        setTimeout(() => {
          setViewModel(vm => ({
            ...vm,
            analyzers: vm.analyzers.map(a =>
              a.id === msg.id
                ? { ...a, running: false, output: { type: 'text', data: 'Analyzer backend not yet implemented.' } }
                : a
            ),
          }))
        }, 400)
        break

      case 'update_analyzer_code':
        setViewModel(vm => ({
          ...vm,
          analyzers: vm.analyzers.map(a => a.id === msg.id ? { ...a, code: msg.code } : a),
        }))
        break

      case 'add_analyzer':
        setViewModel(vm => ({
          ...vm,
          analyzers: [...vm.analyzers, {
            id: Date.now(),
            name: 'New Analyzer',
            language: 'python',
            code: 'def analyze(signals, packets):\n    return {"type": "text", "data": ""}\n',
            output: null,
            errors: [],
            running: false,
          }],
        }))
        break

      case 'toggle_signal':
        setViewModel(vm => {
          const next = new Set(vm.activeSignals)
          if (next.has(msg.signalId)) next.delete(msg.signalId)
          else next.add(msg.signalId)
          return { ...vm, activeSignals: next }
        })
        break
    }
  }, [])

  return { viewModel, connect, send }
}
