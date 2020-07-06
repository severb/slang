// .▄▄ · ▄▄▌   ▄▄▄·  ▐ ▄  ▄▄ • 
// ▐█ ▀. ██•  ▐█ ▀█ •█▌▐█▐█ ▀ ▪
// ▄▀▀▀█▄██▪  ▄█▀▀█ ▐█▐▐▌▄█ ▀█▄
// ▐█▄▪▐█▐█▌▐▌▐█ ▪▐▌██▐█▌▐█▄▪▐█
//  ▀▀▀▀ .▀▀▀  ▀  ▀ ▀▀ █▪·▀▀▀▀ 

// Slang is a tiny pet language implemented in C with no dependencies other than
// libc. This online version is compiled to WASM with Emscripten.

// Right now, Slang has just few statements and operators, and lacks closures,
// structures, and classes. Speed-wise it's comparable to other (non-JIT
// compiled) dynamic languages like Python and features a one-pass compiler that
// generates bytecode for a compact VM.

fun fibonacci(n) { // functions are first class, can be passed around like values
    // variables are declared with 'var' and optionally initialized (default to nil)
    var a = 1, b = 1, result = [];

    for(; n > 0; n -= 1) { // C-like for loops; can have empty clauses
        result[] = a;      // there's a list append binary operator
        var tmp = b;
        b += a;            // shortcut assignment operators
        a = tmp;
    }

    return result;
}

// the for loop can introduce local variables in the block
for (var f_10 = fibonacci(10), i = 0; i < 10; i += 1) {
    print f_10[i]; // lists are zero-indexed and bound-checked at runtime
} // f_10 and i are unavailable here

// Slang comes with the usual built-in types:
var integer = 1_000,      // integer digits can be grouped with _
    string = "A string",  // strings lack character escaping for now
    float = 13.370_001,
    nothing = nil,
    boolean = true,
    list = ["x", "b", "c"],
    dict = {"one": 1, "two": 2, "three": 3};

dict["four"] = 4; // dict item assignment, no deletion operation yet

// math operators
10 + 20; // 64 bit signed integers, unboxed when below 49 bits
10 - 20;
8 / 2.5; // automatic promotion to float
9 * 3.0;
10 % 7;

1 * -2 * -3; // unary negation
!10;         // unary not, the result is a boolean, !!x is a boolean conversion

// logical operators short-circuit or return the last value
0 or [] or {} or "" or nil or false or "everything is false";
10 and "test" and [1] and true and "everything is true";

10 == 10 and 20 < 30 and 100 >= 50; // the usual comparison operators

var i = 0;
while (i < 100) {     // a while statement followed by a block
    if (i % 2 == 0) { // a conditional statement with an else clause
        i = i + 1;
    } else {
        i = i * 2;
    }
}

for(var j = 0; ;j += 1) { // for with an empty middle clause
    if (i < 10) continue;   // continue to the next iteration
    break;                  // break the loop
}