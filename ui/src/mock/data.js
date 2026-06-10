export const MOCK_SIGNALS = [
  { id: 1, name: 'EngineRPM',     unit: 'rpm',  group: 'Powertrain',  value: 2450  },
  { id: 2, name: 'VehicleSpeed',  unit: 'km/h', group: 'Dynamics',    value: 87    },
  { id: 3, name: 'ThrottlePos',   unit: '%',    group: 'Powertrain',  value: 34.5  },
  { id: 4, name: 'BrakePressure', unit: 'bar',  group: 'Brakes',      value: 0     },
  { id: 5, name: 'BatteryVolt',   unit: 'V',    group: 'Electrical',  value: 12.6  },
  { id: 6, name: 'CoolantTemp',   unit: '°C',   group: 'Powertrain',  value: 92    },
  { id: 7, name: 'SteeringAngle', unit: 'deg',  group: 'Dynamics',    value: -12.3 },
]

export const MOCK_ANALYZERS = [
  {
    id: 1,
    name: 'RPM Spike Detector',
    language: 'python',
    code: `def analyze(signals, packets):
    threshold = 5000
    spikes = [
        p for p in packets
        if p.get('signal') == 'EngineRPM' and p.get('value', 0) > threshold
    ]
    return {'type': 'text', 'data': f'{len(spikes)} spikes detected above {threshold} rpm'}
`,
    output: null,
    errors: [],
    running: false,
  },
  {
    id: 2,
    name: 'CRC Validator',
    language: 'javascript',
    code: `function analyze(signals, packets) {
  const invalid = packets.filter(p => p.data.endsWith('ff'));
  return { type: 'text', data: \`\${invalid.length} frames with suspect trailing byte\` };
}
`,
    output: null,
    errors: [],
    running: false,
  },
]

const CHANNELS = [1, 2, 3, 4]
const IDS = [0x100, 0x200, 0x300, 0x400, 0x500, 0x6B0, 0x7DF]

export function generatePacketWindow(offset, count) {
  const packets = []
  for (let i = 0; i < count; i++) {
    const idx = offset + i
    const dlc = (idx % 7) + 1
    const data = Array.from({ length: dlc }, (_, j) =>
      ((idx * 13 + j * 7) & 0xff).toString(16).padStart(2, '0')
    ).join(' ')
    packets.push({
      index: idx,
      timestamp: ((idx * 1127 + 500000) / 1e6).toFixed(6),
      type: idx % 8 === 0 ? 'ETH' : 'CAN',
      channel: CHANNELS[idx % CHANNELS.length],
      id: IDS[idx % IDS.length],
      dlc,
      data,
    })
  }
  return packets
}

export const TOTAL_PACKET_COUNT = 1_000_000
