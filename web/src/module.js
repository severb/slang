import { editor } from "./editor";
import wasmModule from '../../wasm/bin/slang.wasm'

const stdoutpre = document.getElementById('stdoutpre');

var initialized = false;

export var Module = {
    preRun: [],
    arguments: [],
    postRun: [],
    locateFile(path) {
        if (path.endsWith('.wasm')) {
            return wasmModule;
        }
        return path;
    },
    onRuntimeInitialized: function () {
        stdoutpre.innerHTML = '';
        initialized = true;
        const ccall = this.ccall;
        document.getElementById('run').addEventListener('click', function () {
            stdoutpre.innerHTML = '';
            ccall('run', 'number', ['string'], [editor.getValue()]);
            return false;
        });
        document.getElementById('bytecode').addEventListener('click', function () {
            stdoutpre.innerHTML = '';
            ccall('disassamble', 'void', ['string'], [editor.getValue()]);
            return false;
        })
    },
    print: function (text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join('\n');
        console.log(text);
        if (initialized) {
            stdoutpre.innerHTML += text + '\n';
        }
    },
    printErr: function (text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join('\n');
        console.error(text);
        if (initialized) {
            stdoutpre.innerHTML += 'error: ' + text + '\n';
        }
    },
};