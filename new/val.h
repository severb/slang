#ifndef clox_val_h
#define clox_val_h

#include <assert.h>  // static_assert
#include <stdbool.h> // bool
#include <stddef.h>  // max_align_t
#include <stdint.h>  // int8_t, uint16_t, uint32_t, uint64_t, UINT64_C

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

// val_u() safely converts a Val to an uint64_t for bit fiddling.
static inline uint64_t val_u(Val v) {
  union {
    Val val;
    uint64_t uint;
  } a = {.val = v};
  return a.uint;
}

// u_val() is the inverse of val_u().
static inline Val u_val(uint64_t v) {
  union {
    Val val;
    uint64_t uint;
  } a = {.uint = v};
  return a.val;
}

// The layout of doubles is: (s: sign bit, e: exponent, f: fraction)
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
//
// Quiet NaNs have all exponent bits and the first fraction bit set to 1 -- the
// sign bit is ignored. This leaves 52 free bits to work with:
// .1111111|11111...|........|........|........|........|........|........
#define DOUBLE_MASK BYTES(7f, f8, 00, 00, 00, 00, 00, 00)
#define IS_QUIET_NAN(v) ((val_u(v) & DOUBLE_MASK) == DOUBLE_MASK)
#define IS_DOUBLE(v) (!IS_QUIET_NAN(v))

// We stuff data in the least significant six bytes and use the other four bits
// as type discriminators (d). The six bytes are sufficient for storing
// pointers on most platforms.
// d1111111|11111ddd|........|........|........|........|........|........

// TYPE_MASK isolates the type discriminator from the storage area.
#define TYPE_MASK BYTES(ff, ff, 00, 00, 00, 00, 00, 00)

// We define two types of values: pointers and data. Pointers have the most
// significant discriminator bit (the sign bit) set to 0, while data values
// have it set to 1.
#define VAL_TYPE_MASK BYTES(ff, f8, 00, 00, 00, 00, 00, 00)
#define VAL_PTR BYTES(7f, f8, 00, 00, 00, 00, 00, 00)
#define VAL_DATA BYTES(ff, f8, 00, 00, 00, 00, 00, 00)
#define IS_PTR(v) ((val_u(v) & VAL_TYPE_MASK) == VAL_PTR)
#define IS_DATA(v) ((val_u(v) & VAL_TYPE_MASK) == VAL_DATA)

// Pointers have additional tagging applied to their least significant bit.
// This is possible because pointers allocated with malloc are usually aligned
// (contrary to pointers to arbitrary chars in a string). We assume all
// pointers are at least two-byte aligned, which gives us one bit to tag at the
// end of the pointer. We use this bit to flag if the pointer "owns" the data
// it points to and it's responsible for freeing it or if it's just a
// reference. Owned pointers have the flag bit set to 0, while references have
// it set to 1.
static_assert(sizeof(max_align_t) >= 2, "pointer alginment");
#define OWNERSHIP_MASK BYTES(00, 00, 00, 00, 00, 00, 00, 01)
#define IS_OWN_PTR(v) ((val_u(v) & (VAL_TYPE_MASK | OWNERSHIP_MASK)) == VAL_PTR)
#define IS_REF_PTR(v)                                                          \
  ((val_u(v) & (VAL_TYPE_MASK | OWNERSHIP_MASK)) == (VAL_PTR | OWNERSHIP_MASK))

// PTR_MASK isolates the pointer value and ignores the ownership flag.
#define PTR_MASK BYTES(00, 00, FF, FF, FF, FF, FF, FE)
#define PTR(v) ((void *)(val_u(v) & PTR_MASK))

// String is the first pointer type. It points to a String and has the
// following layout:
// 01111111|11111000|........|........|........|........|........|.......o
#define STR_PTR_TYPE BYTES(7f, f8, 00, 00, 00, 00, 00, 00)
#define IS_STR_PTR(v) ((val_u(v) & TYPE_MASK) == STR_PTR_TYPE)
#define IS_STR_OWN(v)                                                          \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) == STR_PTR_TYPE)
#define IS_STR_REF(v)                                                          \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) == (STR_PTR_TYPE | OWNERSHIP_MASK))
#define STR_PTR(v) ((String *)PTR(v))

// Next, we have the Table pointer which has the following layout:
// 01111111|11111001|........|........|........|........|........|.......o
#define TABLE_PTR_TYPE BYTES(7f, f9, 00, 00, 00, 00, 00, 00)
#define IS_TABLE_PTR(v) (val_u(v) & TYPE_MASK == TABLE_PTR_TYPE)
#define IS_TABLE_OWN(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) == TABLE_PTR_TYPE)
#define IS_TABLE_REF(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) ==                                \
   (TABLE_PTR_TYPE | OWNERSHIP_MASK))
#define TABLE_PTR(v) ((Table *)PTR(v))

// The Array pointer continues the pointer value series:
// 01111111|11111010|........|........|........|........|........|.......o
#define ARRAY_PTR_TYPE BYTES(7f, fa, 00, 00, 00, 00, 00, 00)
#define IS_ARRAY_PTR(v) (val_u(v) & TYPE_MASK == ARRAY_PTR_TYPE)
#define IS_ARRAY_OWN(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) == ARRAY_PTR_TYPE)
#define IS_ARRAY_REF(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) ==                                \
   (ARRAY_PTR_TYPE | OWNERSHIP_MASK))
#define ARRAY_PTR(v) ((Array *)PTR(v))

// Lastly, we have a "big" 64-bit signed integer pointer (i.e., int64_t):
// 01111111|11111011|........|........|........|........|........|.......o
#define INT_PTR_TYPE BYTES(7f, fb, 00, 00, 00, 00, 00, 00)
#define IS_INT_PTR(v) ((val_u(v) & TYPE_MASK) == INT_PTR_TYPE)
#define IS_INT_OWN(v)                                                          \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) == INT_PTR_TYPE)
#define IS_INT_REF(v)                                                          \
  ((val_u(v) & (TYPE_MASK | OWNERSHIP_MASK)) == (INT_PTR_TYPE | OWNERSHIP_MASK))
#define INT_PTR(v) ((int64_t *)PTR(v))

// The next two pointer value type are reserved for later use:
// 01111111|11111100|........|........|........|........|........|.......o
// 01111111|11111101|........|........|........|........|........|.......o
#define UNUSED_PTR_MASK BYTES(7f, fe, 00, 00, 00, 00, 00, 00)
#define RES1_PTR_TYPE BYTES(7f, fc, 00, 00, 00, 00, 00, 00)
#define RES2_PTR_TYPE BYTES(7f, fd, 00, 00, 00, 00, 00, 00)
#define IS_RES_PTR(v) ((val_u(v) & UNUSED_PTR_MASK) == RES1_PTR_TYPE)
#define IS_RES1_PTR(v) ((val_u(v) & TYPE_MASK) == RES1_PTR_TYPE)
#define IS_RES2_PTR(v) ((val_u(v) & TYPE_MASK) == RES2_PTR_TYPE)

// The last two pointer value types are unused and are available to the users:
// 01111111|11111110|........|........|........|........|........|.......o
// 01111111|11111111|........|........|........|........|........|.......o
#define USR1_PTR_TYPE BYTES(7f, fe, 00, 00, 00, 00, 00, 00)
#define USR2_PTR_TYPE BYTES(7f, ff, 00, 00, 00, 00, 00, 00)
#define IS_USR_PTR(v) ((val_u(v) & UNUSED_PTR_MASK) == USR1_PTR_TYPE)
#define IS_USR1_PTR(v) ((val_u(v) & TYPE_MASK) == USR1_PTR_TYPE)
#define IS_USR2_PTR(v) ((val_u(v) & TYPE_MASK) == USR2_PTR_TYPE)

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
#define STR5_DATA_TYPE BYTES(ff, f8, 00, 00, 00, 00, 00, 00)
#define STR5_SIZE_MASK BYTES(00, 00, ff, 00, 00, 00, 00, 00)
#define IS_STR5_DATA(v) ((val_u(v) & TYPE_MASK) == STR5_DATA_TYPE)
#define STR5_SIZE(v)                                                           \
  ((int8_t)((val_u(v) & STR5_SIZE_MASK) >> 40)) // safe because 0 <= size <= 5
#define STR5_CHARS(v) (((char *)&val_u(v)) + 3) // TODO: this doesn't work

// There's also a fixed six characters string data value type:
// 11111111|11111001|aaaaaaaa|bbbbbbbb|cccccccc|dddddddd|eeeeeeee|ffffffff
#define STR6_DATA_TYPE BYTES(ff, f9, 00, 00, 00, 00, 00, 00)
#define IS_STR6_DATA(v) ((val_u(v) & TYPE_MASK) == STR6_DATA_TYPE)
#define STR6_CHARS(v) (((char *)&val_u(v)) + 2) // TODO: this doesn't work

// The next data value type is the Pair. It stores a pair of two usigned
// integers a and b. a's size is two bytes and b's size is four bytes:
// 11111111|11111010|aaaaaaaa|aaaaaaaa|bbbbbbbb|bbbbbbbb|bbbbbbbb|bbbbbbbb
#define PAIR_DATA_TYPE BYTES(ff, fa, 00, 00, 00, 00, 00, 00)
#define PAIR_A_MASK BYTES(00, 00, ff, ff, 00, 00, 00, 00)
#define PAIR_B_MASK BYTES(00, 00, 00, 00, ff, ff, ff, ff)
#define IS_PAIR_DATA(v) ((val_u(v) & TYPE_MASK) == PAIR_DATA_TYPE)
#define PAIR_A(v) ((uint16_t)((val_u(v) & PAIR_A_MASK) >> 32))
#define PAIR_B(v) ((uint32_t)(val_u(v) & PAIR_B_MASK))

// Next is the Symbol value which defines the following symbols: FALSE, TRUE,
// NIL, and ERROR. These four symbols and all others that fit in the least two
// significant bytes are reserved. The remaining 32-bit space is available for
// user-defined symbols and flags.
// 11111111|11111011|uuuuuuuu|uuuuuuuu|uuuuuuuu|uuuuuuuu|........|........
#define SYMB_DATA_TYPE BYTES(ff, fb, 00, 00, 00, 00, 00, 00)
#define IS_SYMBOL_DATA(v) ((val_u(v) & TYPE_MASK) == SYMB_DATA_TYPE)
#define VAL_FALSE (SYMB_DATA_TYPE)
#define VAL_TRUE (SYMB_DATA_TYPE + 1)
#define VAL_NIL (SYMB_DATA_TYPE + 2)
#define VAL_ERROR (SYMB_DATA_TYPE + 3)
#define IS_FALSE(v) (val_u(v) == VAL_FALSE)
#define IS_TRUE(v) (val_u(v) == VAL_TRUE)
#define IS_NIL(v) (val_u(v) == VAL_NIL)
#define IS_ERROR(v) (val_u(v) == VAL_ERROR)

static inline bool is_res_symbol(Val v) {
  return (val_u(v) >= SYMB_DATA_TYPE &&
          val_u(v) <= SYMB_DATA_TYPE + BYTES(00, 00, 00, 00, 00, 00, ff, ff));
}

static inline bool is_usr_symbol(Val v) {
  return (val_u(v) >= SYMB_DATA_TYPE + BYTES(00, 00, 00, 00, 00, 01, 00, 00) &&
          val_u(v) <= SYMB_DATA_TYPE + BYTES(00, 00, ff, ff, ff, ff, ff, ff));
}

#define USR_SYMBOL_MASK BYTES(00, 00, ff, ff, ff, ff, 00, 00)
#define USR_SYMBOL(v) ((uint32_t)((val_u(v) & USR_SYMBOL_MASK) >> 16))

// The next two data value types are reserved for later use:
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
#define UNUSED_DATA_MASK BYTES(ff, fc, 00, 00, 00, 00, 00, 00)
#define RES1_DATA_TYPE BYTES(ff, fc, 00, 00, 00, 00, 00, 00)
#define RES2_DATA_TYPE BYTES(ff, fd, 00, 00, 00, 00, 00, 00)
#define IS_RES_DATA(v) ((val_u(v) & UNUSED_DATA_MASK) == RES1_DATA_TYPE)
#define IS_RES1_DATA(v) ((val_u(v) & TYPE_MASK) == RES1_DATA_TYPE)
#define IS_RES2_DATA(v) ((val_u(v) & TYPE_MASK) == RES2_DATA_TYPE)

// The last two data value types are unused and are available to the users:
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........
#define USR1_DATA_TYPE BYTES(ff, fe, 00, 00, 00, 00, 00, 00)
#define USR2_DATA_TYPE BYTES(ff, ff, 00, 00, 00, 00, 00, 00)
#define IS_USR_DATA(v) ((val_u(v) & UNUSED_DATA_MASK) == USR1_DATA_TYPE)
#define IS_USR1_DATA(v) ((val_u(v) & TYPE_MASK) == USR1_DATA_TYPE)
#define IS_USR2_DATA(v) ((val_u(v) & TYPE_MASK) == USR2_DATA_TYPE)

// ValType makes writing switch statements on a value's type less error prone
// and probably faster too (because of jump tables).
typedef enum {
  STR_PTR_T = (int)(STR_PTR_TYPE >> 48), // 0x7ff8
  TABLE_PTR_T,
  ARRAY_PTR_T,
  INT_PTR_T,
  RES1_PTR_T,
  RES2_PTR_T,
  USR1_PTR_T,
  USR2_PTR_T, // 0x7fff

  DOUBLE_DATA_T = (int)(STR5_DATA_TYPE >> 48) - 1, // 0xfff7
  STR5_DATA_T,                                     // 0xfff8
  STR6_DATA_T,
  PAIR_DATA_T,
  SYMB_DATA_T,
  RES1_DATA_T,
  RES2_DATA_T,
  USR1_DATA_T,
  USR2_DATA_T, // 0xffff

  // The enum order is correct even for 16-bit ints.
} ValType;

static inline ValType type(Val v) {
  uint64_t res = IS_DOUBLE(v) ? DOUBLE_DATA_T : (val_u(v) >> 48);
  return u_val(res);
}

#endif
