import { useBackend } from './hooks/useBackend.js'
import FilePicker from './components/FilePicker.jsx'
import MainLayout from './components/MainLayout.jsx'

export default function App() {
  const { viewModel, connect, send } = useBackend()

  if (!viewModel) return <FilePicker onLoad={connect} />
  return <MainLayout viewModel={viewModel} send={send} />
}
