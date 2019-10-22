#ifndef clox_val_h
#define clox_val_h

#include <stdio.h>

#include <assert.h>  // static_assert
#include <stdbool.h> // bool
#include <stddef.h>  // max_align_t
#include <stdint.h>  // *int16_t, *int32_t, *int64_t, UINT64_C

// Val is a polymorphic type. It stores all value types exposed by the language
// and can be used with all collections. Because Vals are so versatile, they
// are often used internally by the compiler and VM for bookkeeping.
// All Vals, except doubles, have the following invariant: two Vals are equal
// if their bitwise comparison is equal. The reverse in not necessarily true:
// two String pointer types can point to different strings which contain the
// same characters.
typedef struct {
  union {
    double d;
    uint64_t u;
  } as;
} Val;

// Vals are 64 bits long and use a bit-packing technique called NaN tagging.
static_assert(sizeof(uint64_t) == sizeof(double), "double size mismatch");

static inline uint64_t val_u(Val v) { return v.as.u; }
static inline Val u_val(uint64_t v) { return (Val){.as = {.u = v}}; }

// BYTES is a visual aid for defining 64 bit patterns.
#define BYTES(a, b, c, d, e, f, g, h) (UINT64_C(0x##a##b##c##d##e##f##g##h))

// The layout of doubles is: (s: sign bit, e: exponent, f: fraction)
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
//
// NaNs have all exponent bits set high -- the sign bit is ignored. In
// addition, there are two different types of NaNs: quiet (qNaN) and signaling
// (sNaN). The two are differentiated by the most significant bit of the
// fraction: qNaNs have the bit set high and sNaNs have it low. sNaNs must also
// have any other fraction bit set (usually the least significant) to
// distinguish them from infinity. Typically, these NaNs are encoded like so:
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
// 01111111|11111000|00000000|00000000|00000000|00000000|00000000|00000000
// 01111111|11110000|00000000|00000000|00000000|00000000|00000000|00000001
//
// This leaves us with enough space to use for other things, like storing
// values or 48-bit pointers. To do so, we use the following bit-pattern to
// identify non-double values:
// .1111111|1111.1..|........|........|........|........|........|........
//
// We use the top four bits as type discriminators for the data store in the
// remaining six bytes.

#define TAGGED_MASK BYTES(7f, f4, 00, 00, 00, 00, 00, 00)
#define IS_TAGGED(v) ((val_u(v) & TAGGED_MASK) == TAGGED_MASK)
#define IS_DOUBLE(v) (!IS_TAGGED(v))

// We define two categories of values: pointers and data. Pointers have the
// most significant discriminator bit (the sign bit) set, while data values
// have it unset.
#define SIGN_FLAG BYTES(80, 00, 00, 00, 00, 00, 00, 00)
#define IS_PTR(v) ((val_u(v) & (TAGGED_MASK | SIGN_FLAG)) == TAGGED_MASK)
#define IS_DATA(v)                                                             \
  ((val_u(v) && (TAGGED_MASK | SIGN_FLAG)) == (TAGGED_MASK | SIGN_FLAG))

// Pointers have additional tagging applied to their least significant bit.
// This is possible because pointers allocated with malloc are usually aligned
// (contrary to pointers to arbitrary chars in a string). We assume all
// pointers are at least two-byte aligned, which gives us one bit to tag at the
// end of the pointer. We use this bit to flag if the pointer "owns" the data
// it points to and it's responsible for freeing it or if it's just a
// reference. Owned pointers have the flag bit unset, while references have it
// set.
static_assert(sizeof(max_align_t) >= 2, "pointer alginment >= 2");
#define OWNER_FLAG BYTES(00, 00, 00, 00, 00, 00, 00, 01)
#define IS_OWN_PTR(v)                                                          \
  ((val_u(v) & (SIGN_FLAG | TAGGED_MASK | OWNER_FLAG)) == TAGGED_MASK)
#define IS_REF_PTR(v)                                                          \
  ((val_u(v) & (SIGN_FLAG | TAGGED_MASK | OWNER_FLAG)) ==                      \
   (TAGGED_MASK | OWNER_FLAG))

// PTR_MASK isolates the pointer value and ignores the ownership flag.
#define PTR_MASK BYTES(00, 00, FF, FF, FF, FF, FF, FE)
#define PTR(v) ((void *)(uintptr_t)(val_u(v) & PTR_MASK))

// TYPE_MASK isolates the type discriminator from the stored value.
#define TYPE_MASK BYTES(ff, ff, 00, 00, 00, 00, 00, 00)

// String is the first pointer type. It points to a String and has the
// following layout:
// 01111111|11110100|........|........|........|........|........|.......o
#define STR_PTR_TYPE BYTES(7f, f4, 00, 00, 00, 00, 00, 00)
#define IS_STR_PTR(v) ((val_u(v) & TYPE_MASK) == STR_PTR_TYPE)
#define IS_STR_OWN(v) ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == STR_PTR_TYPE)
#define IS_STR_REF(v)                                                          \
  ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == (STR_PTR_TYPE | OWNER_FLAG))
#define STR_PTR(v) ((String *)PTR(v))

// Next, we define the Table pointer which has the following layout:
// 01111111|11110101|........|........|........|........|........|.......o
#define TABLE_PTR_TYPE BYTES(7f, f5, 00, 00, 00, 00, 00, 00)
#define IS_TABLE_PTR(v) (val_u(v) & TYPE_MASK == TABLE_PTR_TYPE)
#define IS_TABLE_OWN(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == TABLE_PTR_TYPE)
#define IS_TABLE_REF(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == (TABLE_PTR_TYPE | OWNER_FLAG))
#define TABLE_PTR(v) ((Table *)PTR(v))

// The Array pointer is the next pointer value type:
// 01111111|11110110|........|........|........|........|........|.......o
#define ARRAY_PTR_TYPE BYTES(7f, f6, 00, 00, 00, 00, 00, 00)
#define IS_ARRAY_PTR(v) (val_u(v) & TYPE_MASK == ARRAY_PTR_TYPE)
#define IS_ARRAY_OWN(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == ARRAY_PTR_TYPE)
#define IS_ARRAY_REF(v)                                                        \
  ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == (ARRAY_PTR_TYPE | OWNER_FLAG))
#define ARRAY_PTR(v) ((Array *)PTR(v))

// Lastly, we have a "big" 64-bit signed integer pointer (i.e., int64_t):
// 01111111|11110111|........|........|........|........|........|.......o
#define INT_PTR_TYPE BYTES(7f, f7, 00, 00, 00, 00, 00, 00)
#define IS_INT_PTR(v) ((val_u(v) & TYPE_MASK) == INT_PTR_TYPE)
#define IS_INT_OWN(v) ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == INT_PTR_TYPE)
#define IS_INT_REF(v)                                                          \
  ((val_u(v) & (TYPE_MASK | OWNER_FLAG)) == (INT_PTR_TYPE | OWNER_FLAG))
#define INT_PTR(v) ((int64_t *)PTR(v))

// The remaining four pointer value type are reserved for later use:
// 01111111|11111100|........|........|........|........|........|.......o
// 01111111|11111101|........|........|........|........|........|.......o
// 01111111|11111110|........|........|........|........|........|.......o
// 01111111|11111111|........|........|........|........|........|.......o

// Other than the pointer value types, we define a few data value types. All
// data value types contain their values directly in the storage area and have
// the most significant discriminant bit (the sign bit) set to 1.
// A double is also a data value type, but it's special because it acts as the
// "hosting" type for all other value types through NaN tagging.

// The first data value type is the unsigned Pair. It stores a pair of two
// integers a and b. a's size is two bytes and b's size is four bytes:
// 11111111|11110100|aaaaaaaa|aaaaaaaa|bbbbbbbb|bbbbbbbb|bbbbbbbb|bbbbbbbb
#define PAIR_DATA_TYPE BYTES(ff, f4, 00, 00, 00, 00, 00, 00)
#define PAIR_A_MASK BYTES(00, 00, ff, ff, 00, 00, 00, 00)
#define PAIR_B_MASK BYTES(00, 00, 00, 00, ff, ff, ff, ff)
#define IS_PAIR_DATA(v) ((val_u(v) & TYPE_MASK) == PAIR_DATA_TYPE)
#define PAIR_UA(v) ((uint16_t)((val_u(v) & PAIR_A_MASK) >> 32))
#define PAIR_UB(v) ((uint32_t)(val_u(v) & PAIR_B_MASK))

int16_t u16_i(uint16_t v) {
  union {
    uint16_t as_uint16_t;
    int16_t as_int16_t;
  } x = {.as_uint16_t = v};
  return x.as_int16_t;
}

uint32_t u32_i(int32_t v) {
  union {
    uint32_t as_uint32_t;
    int32_t as_int32_t;
  } x = {.as_int32_t = v};
  return x.as_uint32_t;
}

#define PAIR_A(v) (u16_i(PAIR_UA(v)))
#define PAIR_B(v) (u32_i(PAIR_UB(v)))

// Next is the Symbol value which defines the following symbols: FALSE, TRUE,
// NIL, and ERROR. These four symbols and all others that fit in the least
// significant two bytes are reserved. The remaining 32-bit space is available
// for user-defined symbols and flags.
// 11111111|11110101|uuuuuuuu|uuuuuuuu|uuuuuuuu|uuuuuuuu|........|........
#define SYMB_DATA_TYPE BYTES(ff, f5, 00, 00, 00, 00, 00, 00)
#define IS_SYMBOL_DATA(v) ((val_u(v) & TYPE_MASK) == SYMB_DATA_TYPE)
#define VAL_FALSE (u_val(SYMB_DATA_TYPE))
#define VAL_TRUE (u_val(SYMB_DATA_TYPE + 1))
#define VAL_NIL (u_val(SYMB_DATA_TYPE + 2))
#define VAL_ERROR (u_val(SYMB_DATA_TYPE + 3))
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

// The remaining six data value types are reserved for later use:
// 11111111|11110110|........|........|........|........|........|........
// 11111111|11110111|........|........|........|........|........|........
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........

// ValType makes writing switch statements on a value's type less error prone:
typedef enum {
  STR_PTR_T = (int)(STR_PTR_TYPE >> 48), // 0x7ff4
  TABLE_PTR_T,
  ARRAY_PTR_T,
  INT_PTR_T,
  // NB: these are negative values for 16-bit ints, but the order is still OK.
  DOUBLE_DATA_T = (int)(PAIR_DATA_TYPE >> 48) - 1, // 0xfff4 - 1
  PAIR_DATA_T,
  SYMB_DATA_T,
} ValType;

static inline ValType type(Val v) {
  if (IS_DOUBLE(v))
    return DOUBLE_DATA_T;
  return (ValType)((val_u(v) & TYPE_MASK) >> 48);
}

#endif
