// .▄▄ · ▄▄▌   ▄▄▄·  ▐ ▄  ▄▄ • 
// ▐█ ▀. ██•  ▐█ ▀█ •█▌▐█▐█ ▀ ▪
// ▄▀▀▀█▄██▪  ▄█▀▀█ ▐█▐▐▌▄█ ▀█▄
// ▐█▄▪▐█▐█▌▐▌▐█ ▪▐▌██▐█▌▐█▄▪▐█
//  ▀▀▀▀ .▀▀▀  ▀  ▀ ▀▀ █▪·▀▀▀▀ 

// Slang is a tiny pet language implemented in C with no dependencies other than
// libc. This online version is compiled to WASM with Emscripten.

// Right now, Slang has just few statements and operators, and lacks functions,
// structures, and classes. Speed-wise it's comparable to other (non-JIT
// compiled) dynamic languages like Python and features a one-pass compiler that
// generates bytecode for a compact VM.

// variables are declared with 'var' and optionally initialized (default to nil)
var a = 1, b = 1;

while (a < 1_000) { // print the fibonacci sequence up to 1000
    print a;        // since there are no functions yet, print is a statement
    b = a + b;
    a = b - a;
}

// Slang comes with the usual built-in types:
var integer = 1_000,      // integer digits can be grouped with _
    string = "A string",  // strings lack character escaping for now
    float = 13.370_001,
    nothing = nil,
    boolean = true,
    list = ["x", "b", "c"],
    dict = {"one": 1, "two": 2, "three": 3};

list[0] = "a"; // lists are zero-indexed and bound-checked at runtime
list []= "d";  // x []= y is the list append binary operator
list[] = "e";  // x[] = y is equivalent but reads nicer

dict["four"] = 4; // dict item assignment, no deletion yet

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

// Blocks can be used to group multiple statements and can be nested.
// The variables defined in a block are temporary, local to the block, and
// shadow other variables.
{
    var var1 = 1, var2 = 2; // locals are faster than late binding globals
    {
        var var2; // shadows var2 = 2
        var2 = "two";
    }
} // var1 and var2 are out of scope after the block

var i = 0;
while (i < 100) {     // a while statement followed by a block
    if (i % 2 == 0) { // a conditional statement with an else clause
        i = i + 1;
    } else {
        i = i * 2;
    }
}

var sum = 0;
for (var j = 0; j < 10; j = j + 1) { // like in C, for can define local variables
    sum = sum + j;
} // j is out of scope after the block ends

for(var j = 0; ;j = j + 1) { // for loops can have empty clauses
    if (i < 10) continue;   // continue to the next iteration
    break;                  // break the loop
}