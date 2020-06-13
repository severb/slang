import * as monaco from "monaco-editor/esm/vs/editor/editor.api";

// Register slang
monaco.languages.register({ id: 'slang' });

monaco.languages.setLanguageConfiguration('slang', {
    brackets: [
        ['{', '}'],
        ['[', ']'],
        ['(', ')']
      ],
      autoClosingPairs: [
        { open: '[', close: ']' },
        { open: '{', close: '}' },
        { open: '(', close: ')' },
        { open: '"', close: '"' },
      ],
      surroundingPairs: [
        { open: '{', close: '}' },
        { open: '[', close: ']' },
        { open: '(', close: ')' },
        { open: '"', close: '"' },
      ],
    commens: {
          lineComment: "//",
      }
});

// Register a tokens provider for the language
monaco.languages.setMonarchTokensProvider('slang', {
    // Set defaultToken to invalid to see what you do not tokenize yet
    brackets: [
        ['{', '}', 'brackets.curly'],
        ['[', ']', 'brackets.square'],
        ['(', ')', 'brackets.parenthesis'],
    ],

    keywords: [
        'break', 'continue', 'else', 'for',
        'if', 'print', 'while',
    ],

    keyword_operators: [
        'and', 'or',
    ],

    keyword_constants: [
        'nil', 'true', 'false'
    ],

    operators: [
        '=', '>', '<', '!',
        '==', '<=', '>=', '!=',
        '+', '-', '*', '/', '%',
    ],

    symbols: /[=><!+\-*\/%]+/,

    digits: /\d+(_+\d+)*/,

    identifiers: /[a-zA-Z_]\w*/,

    tokenizer: {
        root: [
            { include: '@var_stmt' },
            { include: '@common' },
        ],

        var_stmt: [
            [/(var)\b/, 'keyword.$0', '@var_decl'],
        ],

        common: [
            // identifiers and keywords
            [/@identifiers/, {
                cases: {
                    '@keywords': { token: 'keyword.$0' },
                    '@keyword_operators': { token: 'operators.andor' },
                    '@keyword_constants': { token: 'constant' },
                    '@default': 'identifier'
                }
            }],

            // whitespace
            { include: '@whitespace' },

            [/\[\s*\]\s*\=/, 'operators'],

            [/(\{\s*\}|\[\s*\])/, 'constant'],

            { include: '@square_brackets' },
            { include: '@curly_brackets' },

            [/[()]/, '@brackets'],

            [/@symbols/, {
                cases: {
                    '@operators': 'operators',
                    '@default': ''
                }
            }],

            // numbers
            [/(@digits)\.(@digits)/, 'number.float'],
            [/(@digits)/, 'number'],

            // delimiter
            [/[;,:]/, 'delimiter'],

            // strings
            [/"[^"]*$/, 'string.invalid'],  // non-teminated string
            [/"/, 'string', '@string'],
        ],

        whitespace: [
            [/[ \t\r\n]+/, ''],
            [/\/\/.*$/, 'comment'],
        ],

        square_brackets: [
            [/\[/, '@brackets', '@in_square_brackets']
        ],

        in_square_brackets: [
            [/\]/, '@brackets', '@pop'],
            { include: '@common' },
        ],

        curly_brackets: [
            [/{/, '@brackets', '@in_curly_brackets']
        ],

        in_curly_brackets: [
            [/}/, '@brackets', '@pop'],
            { include: '@var_stmt' },
            { include: '@common' },
        ],

        string: [
            [/[^"]+/, 'string'],
            [/"/, 'string', '@pop']
        ],

        var_decl: [
            [/(?=;)/, '', '@pop'],
            [/@identifiers/, 'variable.name', '@var_init'],
            [/,/, 'delimiter'],
            { include: "@whitespace" }
        ],

        var_init: [
            [/(?=,|;)/, '', '@pop'],
            { include: '@common' },
        ],
    }
});
