import { useEffect, useRef } from 'react'
import { EditorView, basicSetup } from 'codemirror'
import { python } from '@codemirror/lang-python'
import { javascript } from '@codemirror/lang-javascript'

const langExt = {
  python:     () => [python()],
  javascript: () => [javascript()],
  lua:        () => [],
}

export default function AnalyzerEditor({ code, language, onChange }) {
  const containerRef = useRef(null)
  const viewRef = useRef(null)
  const onChangeRef = useRef(onChange)
  useEffect(() => { onChangeRef.current = onChange }, [onChange])

  useEffect(() => {
    const initialCode = viewRef.current?.state.doc.toString() ?? code
    viewRef.current?.destroy()

    viewRef.current = new EditorView({
      doc: initialCode,
      extensions: [
        basicSetup,
        ...(langExt[language]?.() ?? []),
        EditorView.updateListener.of(u => {
          if (u.docChanged) onChangeRef.current?.(u.state.doc.toString())
        }),
        EditorView.theme({ '&': { fontSize: '12px' } }),
      ],
      parent: containerRef.current,
    })

    return () => { viewRef.current?.destroy(); viewRef.current = null }
  }, [language]) // eslint-disable-line react-hooks/exhaustive-deps

  return <div ref={containerRef} style={{ border: '2px inset #fff' }} />
}
