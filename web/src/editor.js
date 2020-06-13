import "./slangcolorway";
import "./slang";

import * as monaco from "monaco-editor/esm/vs/editor/editor.api";
export const editor = monaco.editor.create(
	document.getElementById("editor"), {
	value: 'print "Hello, World!";',
	language: "slang",
	scrollBeyondLastLine: false,
	lineNumbers: "off",
	tabSize: 4,
	lineHeight: 18,
	fontSize: 14,
	fontLigatures: true,
	fontFamily: 'Fira Code',
	theme: 'slangcolorway'
});

import intro from "raw-loader!../../intro.sl";
document.getElementById("intro").addEventListener("click", function () {
	editor.setValue(intro);
})