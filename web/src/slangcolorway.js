import * as monaco from "monaco-editor/esm/vs/editor/editor.api";

monaco.editor.defineTheme('slangcolorway', {
    base: 'vs-dark',
    inherit: true,
    rules: [
        { background: '#1A1A23' },
        { token: 'keyword', fontStyle: 'bold', foreground: '#D6DBD1' },
        { token: 'constant', fontStyle: 'italic', foreground: '#79A9D1' },
        { token: 'number', foreground: '#79A9D1' },
        { token: 'variable.name', foreground: '#23CE6B' },
        { token: 'string', foreground: '#79A9D1' },
        { token: 'comment', fontStyle: 'italic', foreground: '#66666E' },
        { token: 'delimiter', foreground: '#45455E' },
        { token: 'operators', foreground: '#9999A1' },
        { token: 'operators.andor', fontStyle: 'bold' },
        { token: 'brackets', foreground: '#66666E' },
    ],
    colors: {
        'editor.background': '#1A1A23',
        'editor.foreground': '#EFF1ED',
        'editorCursor.foreground': '#EFF1ED',
        'editor.lineHighlightBorder': '#FFFFFF00',
        'editor.lineHighlightBackground': '#FFFFFF0C',
    },
});