#ifndef clox_val_h
#define clox_val_h

#include <assert.h>  // static_assert
#include <stdbool.h> // bool
#include <stdint.h>  // uint8_t, uint16_t, uint32_t, uint64_t, UINT64_C

// Val is a polymorphic type. It stores all value types exposed by the language
// and can be used with all collections. Because Vals are so versatile, they
// are often used internally by the compiler and VM for bookkeeping.
// All Vals, except doubles, have the following invariant: two Vals are equal
// if their bitwise comparison is equal. The reverse in not necessarily true:
// two String pointer types can point to different strings which contain the
// same characters.
typedef double Val;

// BYTES is a visual aid for defining 64 bit patterns.
#define BYTES(a, b, c, d, e, f, g, h) (UINT64_C(0x##a##b##c##d##e##f##g##h))

// All values are 64 bits long and use NaN tagging.
static_assert(sizeof(uint64_t) == sizeof(double), "double size mismatch");

// The layout of doubles is: (s: sign bit, e: exponent, f: fraction)
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
//
// Quiet NaNs have all exponent bits and the first fraction bit set to 1 -- the
// sign bit is ignored. This leaves 52 free bits to work with:
// .1111111|11111...|........|........|........|........|........|........
static uint64_t const DOUBLE_MASK = BYTES(7f, f8, 00, 00, 00, 00, 00, 00);
#define IS_QUIET_NAN(v) ((v)&DOUBLE_MASK == DOUBLE_MASK)
#define IS_DOUBLE(v) (!IS_QUIET_NAN((v)))

// We stuff data in the least significant six bytes and use the other four bits
// as type discriminators (d). The six bytes are sufficient for storing
// pointers on most platforms.
// d1111111|11111ddd|........|........|........|........|........|........

// TYPE_MASK isolates the type discriminator from the storage area.
static uint64_t const TYPE_MASK = BYTES(ff, ff, 00, 00, 00, 00, 00, 00);

// We define two types of values: pointers and data. Pointers have the most
// significant discriminator bit (the sign bit) set to 0, while data values
// have it set to 1.
static uint64_t const VAL_TYPE_MASK = BYTES(ff, f8, 00, 00, 00, 00, 00, 00);
static uint64_t const VAL_PTR = BYTES(7f, f8, 00, 00, 00, 00, 00, 00);
static uint64_t const VAL_DATA = BYTES(ff, f8, 00, 00, 00, 00, 00, 00);
#define IS_PTR(v) ((v)&VAL_TYPE_MASK == VAL_PTR)
#define IS_DATA(v) ((v)&VAL_TYPE_MASK == VAL_DATA)

// Pointers have additional tagging applied to their least significant bit.
// This is possible because pointers are usually aligned -- unless they point
// to one-byte values. We assume all pointers to be at least two-byte aligned
// which gives us one bit to tag at the end of the pointer. We use this bit to
// flag if the pointer "owns" the data it points to and it's responsible for
// freeing it or if it's just a reference. Owned pointers have the flag bit set
// to 0, while references have it set to 1.
static uint64_t const OWNERSHIP_MASK = BYTES(00, 00, 00, 00, 00, 00, 00, 01);
#define IS_OWN_PTR(v) ((v) & (VAL_TYPE_MASK | OWNERSHIP_MASK) == VAL_PTR)
#define IS_REF_PTR(v)                                                          \
  ((v) & (VAL_TYPE_MASK | OWNERSHIP_MASK) == (VAL_PTR | OWNERSHIP_MASK))

// PTR_MASK isolates the pointer value and ignores the ownership flag.
static uint64_t const PTR_MASK = BYTES(00, 00, FF, FF, FF, FF, FF, FE);
#define PTR(v) ((void *)((v)&PTR_MASK))

// String is the first pointer type. It points to a String and has the
// following layout:
// 01111111|11111000|........|........|........|........|........|.......o
static uint64_t const STR_PTR_TYPE = BYTES(7f, f8, 00, 00, 00, 00, 00, 00);
#define IS_STR_PTR(v) ((v)&TYPE_MASK == STR_PTR_TYPE)
#define IS_STR_OWN(v) ((v) & (TYPE_MASK | OWNERSHIP_MASK) == STR_PTR_TYPE)
#define IS_STR_REF(v)                                                          \
  ((v) & (TYPE_MASK | OWNERSHIP_MASK) == (STR_PTR_TYPE | OWNERSHIP_MASK))
#define STR_PTR(v) ((String *)PTR(v))

// Next, we have the Table pointer which has the following layout:
// 01111111|11111001|........|........|........|........|........|.......o
static uint64_t const TABLE_PTR_TYPE = BYTES(7f, f9, 00, 00, 00, 00, 00, 00);
#define IS_TABLE_PTR(v) ((v)&TYPE_MASK == TABLE_PTR_TYPE)
#define IS_TABLE_OWN(v) ((v) & (TYPE_MASK | OWNERSHIP_MASK) == TABLE_PTR_TYPE)
#define IS_TABLE_REF(v)                                                        \
  ((v) & (TYPE_MASK | OWNERSHIP_MASK) == (TABLE_PTR_TYPE | OWNERSHIP_MASK))
#define TABLE_PTR(v) ((Table *)PTR(v))

// The Array pointer continues the pointer value series:
// 01111111|11111010|........|........|........|........|........|.......o
static uint64_t const ARRAY_PTR_TYPE = BYTES(7f, fa, 00, 00, 00, 00, 00, 00);
#define IS_ARRAY_PTR(v) ((v)&TYPE_MASK == ARRAY_PTR_TYPE)
#define IS_ARRAY_OWN(v) ((v) & (TYPE_MASK | OWNERSHIP_MASK) == ARRAY_PTR_TYPE)
#define IS_ARRAY_REF(v)                                                        \
  ((v) & (TYPE_MASK | OWNERSHIP_MASK) == (ARRAY_PTR_TYPE | OWNERSHIP_MASK))
#define ARRAY_PTR(v) ((Array *)PTR(v))

// Lastly, we have a "big" 64-bit integer pointer (i.e., uint64_t):
// 01111111|11111011|........|........|........|........|........|.......o
static uint64_t const BIGINT_PTR_TYPE = BYTES(7f, fb, 00, 00, 00, 00, 00, 00);
#define IS_BIGINT_PTR(v) ((v)&TYPE_MASK == BIGINT_PTR_TYPE)
#define IS_BIGINT_OWN(v) ((v) & (TYPE_MASK | OWNERSHIP_MASK) == BIGINT_PTR_TYPE)
#define IS_BIGINT_REF(v)                                                       \
  ((v) & (TYPE_MASK | OWNERSHIP_MASK) == (BIGINT_PTR_TYPE | OWNERSHIP_MASK))
#define BIGINT_PTR(v) ((uint64_t *)PTR(v))

// The next two pointer value type are reserved for later use:
// 01111111|11111100|........|........|........|........|........|.......o
// 01111111|11111101|........|........|........|........|........|.......o
static uint64_t const UNUSED_PTR_MASK = BYTES(7f, fe, 00, 00, 00, 00, 00, 00);
static uint64_t const RES1_PTR_TYPE = BYTES(7f, fc, 00, 00, 00, 00, 00, 00);
static uint64_t const RES2_PTR_TYPE = BYTES(7f, fd, 00, 00, 00, 00, 00, 00);
#define IS_RES_PTR(v) ((v)&UNUSED_PTR_MASK == RES1_PTR_TYPE)
#define IS_RES1_PTR(v) ((v)&TYPE_MASK == RES1_PTR_TYPE)
#define IS_RES2_PTR(v) ((v)&TYPE_MASK == RES2_PTR_TYPE)

// The last two pointer value types are unused and are available to the users:
// 01111111|11111110|........|........|........|........|........|.......o
// 01111111|11111111|........|........|........|........|........|.......o
static uint64_t const USR1_PTR_TYPE = BYTES(7f, fe, 00, 00, 00, 00, 00, 00);
static uint64_t const USR2_PTR_TYPE = BYTES(7f, ff, 00, 00, 00, 00, 00, 00);
#define IS_USR_PTR(v) ((v)&UNUSED_PTR_MASK == USR1_PTR_TYPE)
#define IS_USR1_PTR(v) ((v)&TYPE_MASK == USR1_PTR_TYPE)
#define IS_USR2_PTR(v) ((v)&TYPE_MASK == USR2_PTR_TYPE)

// Other than the pointer value types, we define a few data value types. All
// data value types contain their values directly in the storage area and have
// the most significant discriminant bit (the sign bit) set to 1.
// A double is also a data value type, but it's special because it acts as the
// "hosting" type for all other value types through NaN tagging.

// The first data value type is used to store a short, variable length string
// of maximum five characters. Its size is stored in the most significant byte
// and the string is stored in the remaining five bytes like shown here:
// 11111111|11111000|ssssssss|aaaaaaaa|bbbbbbbb|cccccccc|dddddddd|eeeeeeee
static_assert(sizeof(uint8_t) == sizeof(char), "char size mismatch");
static uint64_t const STR5_TYPE = BYTES(ff, f8, 00, 00, 00, 00, 00, 00);
static uint64_t const STR5_SIZE_MASK = BYTES(00, 00, ff, 00, 00, 00, 00, 00);
#define IS_STR5(v) ((v)&TYPE_MASK == STR5_TYPE)
#define STR5_SIZE(v) ((uint8_t)((v)&STR5_SIZE_MASK >> 40))
#define STR5_CHARS(v) (((char *)&(v)) + 3)

// There's also a fixed six characters string data value type:
// 11111111|11111001|aaaaaaaa|bbbbbbbb|cccccccc|dddddddd|eeeeeeee|ffffffff
static uint64_t const STR6_TYPE = BYTES(ff, f9, 00, 00, 00, 00, 00, 00);
#define IS_STR6(v) ((v)&TYPE_MASK == STR6_TYPE)
#define STR6_CHARS(v) ((char *)&(v) + 2)

// The next data value type is the Pair. It stores a pair of two integers a and
// b. a's size is two bytes and b's size is four bytes. The Pair's layout is:
// 11111111|11111010|aaaaaaaa|aaaaaaaa|bbbbbbbb|bbbbbbbb|bbbbbbbb|bbbbbbbb
static uint64_t const PAIR_TYPE = BYTES(ff, f8, 00, 00, 00, 00, 00, 00);
static uint64_t const PAIR_A_MASK = BYTES(00, 00, ff, ff, 00, 00, 00, 00);
static uint64_t const PAIR_B_MASK = BYTES(00, 00, 00, 00, ff, ff, ff, ff);
#define IS_PAIR(v) ((v)&TYPE_MASK == PAIR_TYPE)
#define PAIR_A(v) ((uint16_t)((v)&PAIR_A_MASK >> 32))
#define PAIR_B(v) ((uint32_t)((v)&PAIR_B_MASK))

// Next is the Symbol value which defines the following symbols: FALSE, TRUE,
// NIL, and ERROR. These four symbols and all others that fit in the least two
// significant bytes are reserved. The remaining 32-bit space is available for
// user-defined symbols and flags.
// 11111111|11111011|uuuuuuuu|uuuuuuuu|uuuuuuuu|uuuuuuuu|........|........
static uint64_t const SYMBOL_TYPE = BYTES(ff, f9, 00, 00, 00, 00, 00, 00);
#define IS_SYMBOL(v) ((v)&TYPE_MASK == SYMBOL_TYPE)
static uint64_t const VAL_FALSE = BYTES(ff, f9, 00, 00, 00, 00, 00, 00);
static uint64_t const VAL_TRUE = BYTES(ff, f9, 00, 00, 00, 00, 00, 01);
static uint64_t const VAL_NIL = BYTES(ff, f9, 00, 00, 00, 00, 00, 02);
static uint64_t const VAL_ERROR = BYTES(ff, f9, 00, 00, 00, 00, 00, 03);
#define IS_FALSE(v) ((uint64_t)(v) == VAL_FALSE)
#define IS_TRUE(v) ((uint64_t)(v) == VAL_TRUE)
#define IS_NIL(v) ((uint64_t)(v) == VAL_NIL)
#define IS_ERROR(v) ((uint64_t)(v) == VAL_ERROR)

static uint64_t const USR_SYMBOL_MASK = BYTES(00, 00, ff, ff, ff, ff, 00, 00);
#define USR_SYMBOL(v) ((uint32_t)((v)&USR_SYMBOL_MASK >> 16))

// The next two data value types are reserved for later use:
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
static uint64_t const UNUSED_DATA_MASK = BYTES(ff, fe, 00, 00, 00, 00, 00, 00);
static uint64_t const RES1_DATA_TYPE = BYTES(ff, fc, 00, 00, 00, 00, 00, 00);
static uint64_t const RES2_DATA_TYPE = BYTES(ff, fd, 00, 00, 00, 00, 00, 00);
#define IS_RES_DATA(v) ((v)&UNUSED_DATA_MASK == RES1_DATA_TYPE)
#define IS_RES1_DATA(v) ((v)&TYPE_MASK == RES1_DATA_TYPE)
#define IS_RES2_DATA(v) ((v)&TYPE_MASK == RES2_DATA_TYPE)

// The last two data value types are unused and are available to the users:
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........
static uint64_t const USR1_DATA_TYPE = BYTES(ff, fe, 00, 00, 00, 00, 00, 00);
static uint64_t const USR2_DATA_TYPE = BYTES(ff, ff, 00, 00, 00, 00, 00, 00);
#define IS_USR_DATA(v) ((v)&UNUSED_DATA_MASK == USR1_DATA_TYPE)
#define IS_USR1_DATA(v) ((v)&TYPE_MASK == USR1_DATA_TYPE)
#define IS_USR2_DATA(v) ((v)&TYPE_MASK == USR2_DATA_TYPE)

#undef BYTES

#endif
