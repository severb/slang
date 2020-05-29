// Slang is a tiny work-in-progress language implemented in C that only depends
// on libc. It has only a few statements and operators and lacks functions,
// structures and classes. It's comparable in speed with other (non-JIT
// compiled) dynamic languages like Python and features a one-pass compiler that
// outputs bytecode for a compact VM.

// since there are no functions yet, print is a statement!
print "Hello, World!";

// a list of globals defined and initialized with all built-in types:
var integer = 1_000,      // integer digits can be grouped with _
    string = "A string",  // no character escaping yet
    float = 13.370_001,
    nothing = nil,
    boolean = true,
    list = ["x", "b", "c"],                  // lists are cool
    dict = {"one": 1, "two": 2, "three": 3}; // dicts are even cooler

// uninitialized variables default to nil
var uninitialized_1, uninitialized_2;

// multiple assignment is ok
uninitialized_1 = uninitialized_2 = 100;

print list[0]; // zero-indexed lists
list[0] = "a"; // list indexes are bound-checked at runtime
list []= "d";  // x []= y is the list append binary operator
list[] = "e";  // x[] = y is equivalent but reads nicer
print list;

dict["four"] = 4;
print dict;

print 10 + 20;    // 64 bit signed integers, unboxed when below 49 bits
print 10 - 20;
print 8 / 2.5;    // automatic promotion to float
print 9 * 3.0;
print 10.5 % 1.7; // remainder also works on floats

print 1 + 2 * 3 - 2 / (2 - 1); // the usual (mathematical) operand precedence

print 1 * -2 * -3; // unary negation
print !10;         // unary not, the result is a boolean, !!x is basically bool(x)

// logical operators short-circuit or return the last value
print 0 or [] or {} or "" or nil or false or "everything is false";
print 10 and "test" and [1] and true and "everything is true";

print 10 == 10 and 20 < 30 and 100 >= 50; // the usual comparison operators

 // Blocks can be used to group multiple statements and can be nested. All
 // variables defined in a block are temporary, local to the block, and shadow
 // other existing variables.
{
    var var1 = 1, var2 = 2; // locals are faster than late binding globals
    {
        var var2;
        var2 = "two";
        print var2; // prints "two"
    }
    print var1, ", ", var2; // prints "1, 2"
} // var1 and var2 are out of scope after the block

var i = 0;
while (i < 10) // a while statement
    i = i + 1;

i = 0;
while (i < 10) { // a while statement followed by a block (like the one above)
    print i;
    i = i + 1;
}

if (i == 10) { // a conditional statement
    print "i is ten";
} else {
    print "i is no ten";
}
