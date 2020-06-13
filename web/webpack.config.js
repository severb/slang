const path = require("path");
const MonacoWebpackPlugin = require("monaco-editor-webpack-plugin");
const HtmlWebpackPlugin = require('html-webpack-plugin');

module.exports = {
	mode: process.env.NODE_ENV,
	entry: "./index.js",
	output: {
		path: path.resolve(__dirname, "dist"),
	},
	module: {
		rules: [{
			test: /\.css$/,
			use: ["style-loader", "css-loader",],
		}, {
			test: /\.(ttf|woff2?|eot|svg)$/,
			use: ['file-loader']
		},
		{
			test: path.resolve(__dirname, "../wasm/bin/slang.wasm"),
			loader: 'file-loader',
			type: 'javascript/auto',
		}],
	},
	optimization: {
		minimize: true,

	},
	node: {
		fs: "empty"
	},
	devServer: {
		contentBase: './dist',
	},
	plugins: [
		new MonacoWebpackPlugin({
			languages: [],
			features: [
				'!accessibilityHelp',
				'bracketMatching',
				'caretOperations',
				'clipboard',
				'!codeAction',
				'!codelens',
				'!colorDetector',
				'comment',
				'contextmenu',
				'coreCommands',
				'cursorUndo',
				'dnd',
				'find',
				'!folding',
				'fontZoom',
				'!format',
				'!gotoError',
				'gotoLine',
				'gotoSymbol',
				'hover',
				'iPadShowKeyboard',
				'inPlaceReplace',
				'!inspectTokens',
				'linesOperations',
				'links',
				'multicursor',
				'!parameterHints',
				'quickCommand',
				'!quickOutline',
				'referenceSearch',
				'rename',
				'smartSelect',
				'!snippets',
				'suggest',
				'toggleHighContrast',
				'toggleTabFocusMode',
				'transpose',
				'wordHighlighter',
				'wordOperations',
				'wordPartOperations',
			]
		}),
		new HtmlWebpackPlugin({
			template: "html/index.html",
		})
	]
};
