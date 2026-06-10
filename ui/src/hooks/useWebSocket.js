import { useState, useCallback } from 'react'
import { MOCK_SIGNALS, MOCK_ANALYZERS, generatePacketWindow, TOTAL_PACKET_COUNT } from '../mock/data.js'

export function useWebSocket() {
  const [viewModel, setViewModel] = useState(null)

  const connect = useCallback((tracePath, arxmlPath) => {
    setViewModel({
      status: 'loading',
      tracePath,
      arxmlPath,
      signals: MOCK_SIGNALS,
      activeSignals: new Set([1, 2]),
      totalPackets: TOTAL_PACKET_COUNT,
      packetWindow: { offset: 0, rows: generatePacketWindow(0, 300) },
      analyzers: MOCK_ANALYZERS,
    })
    setTimeout(() => setViewModel(vm => ({ ...vm, status: 'ready' })), 1200)
  }, [])

  const send = useCallback((msg) => {
    switch (msg.type) {
      case 'request_packets':
        setTimeout(() => {
          setViewModel(vm => ({
            ...vm,
            packetWindow: { offset: msg.offset, rows: generatePacketWindow(msg.offset, msg.count) },
          }))
        }, 10)
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
                ? { ...a, running: false, output: { type: 'text', data: 'Analysis complete. Found 3 anomalies in the selected window.' } }
                : a
            ),
          }))
        }, 600 + Math.random() * 400)
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
