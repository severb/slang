#ifndef clox_val_h
#define clox_val_h

#include <assert.h>  // static_assert, assert
#include <stdbool.h> // bool
#include <stddef.h>  // max_align_t
#include <stdint.h>  // *int16_t, *int32_t, *int64_t, UINT64_C

// Forward declarations of some types defined elsewhere:
struct String;
struct Slice;
struct Table;
struct List;

// Val is a tagged union. It stores all value types exposed by the language and
// can be used with all collections. Because Vals are so versatile, they are
// often used internally by the compiler and VM for bookkeeping. All Vals,
// except doubles, have the following invariant: two Vals are equal if their
// bit strings are equal. The reverse in not necessarily true. For example, two
// String pointer values can point to different, but equal, strings.
typedef struct {
  union {
    double d;
    uint64_t u; // for bit fiddling
  };
} Val;

// Vals are 64 bits long and use a bit-packing technique called NaN tagging.
static_assert(sizeof(uint64_t) == sizeof(Val), "Val size mismatch");

static inline uint64_t val_u(Val v) { return v.u; }
static inline double val_d(Val v) { return v.d; }
static inline Val u_val(uint64_t u) { return (Val){.u = u}; }
static inline Val d_val(double d) { return (Val){.d = d}; }

// BYTES is a visual aid for defining 64 bit patterns.
#define BYTES(a, b, c, d, e, f, g, h) (UINT64_C(0x##a##b##c##d##e##f##g##h))

// The layout of doubles is: (s: sign bit, e: exponent, f: fraction)
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
//
// Double NaNs have all exponent bits set high -- the sign bit is ignored. In
// addition, there are two different types of NaNs: quiet (qNaN) and signaling
// (sNaN). The two are differentiated by the most significant bit of the
// fraction: qNaNs have the bit set high and sNaNs have it low. sNaNs must also
// have any other fraction bit set (usually the least significant) to
// distinguish them from infinity. Typically, these NaNs are encoded like so:
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
// 01111111|11111000|00000000|00000000|00000000|00000000|00000000|00000000
// 01111111|11110000|00000000|00000000|00000000|00000000|00000000|00000001
//
// This leaves enough space to use for other things, like storing values or
// 48-bit pointers. To do so, we use the following bit-pattern to identify
// non-double values:
// .1111111|1111.1..|........|........|........|........|........|........
//
// We use the top four most significant bits as type discriminants and store
// data in the remaining six bytes.

#define TAGGED_MASK BYTES(7f, f4, 00, 00, 00, 00, 00, 00)
static inline bool is_tagged(Val v) {
  return (val_u(v) & TAGGED_MASK) == TAGGED_MASK;
}
static inline bool is_double(Val v) { return !is_tagged(v); }

// We define two categories of values: pointers and data. Pointer values have
// the most significant discriminant bit (the sign bit) set, while data values
// have it unset.
#define SIGN_FLAG BYTES(80, 00, 00, 00, 00, 00, 00, 00)
static inline bool is_ptr(Val v) {
  return (val_u(v) & (TAGGED_MASK | SIGN_FLAG)) == TAGGED_MASK;
}
static inline bool is_data(Val v) {
  return (val_u(v) & (TAGGED_MASK | SIGN_FLAG)) == (TAGGED_MASK | SIGN_FLAG);
}

// Pointer values have additional tagging applied to their least significant
// bit. This is possible because pointers allocated with malloc are usually
// aligned; contrary to, for example, pointers to arbitrary chars in a string.
// We assume all pointers are at least two-byte aligned, which means we can use
// the least significant bit as a flag. If the flag is unset, the pointer value
// "owns" the data and is responsible for freeing it; otherwise, it's just a
// reference.
static_assert(sizeof(max_align_t) >= 2, "pointer alginment >= 2");
#define REF_FLAG BYTES(00, 00, 00, 00, 00, 00, 00, 01)
static inline bool is_own_ptr(Val v) {
  return (val_u(v) & (SIGN_FLAG | TAGGED_MASK | REF_FLAG)) == TAGGED_MASK;
}
static inline bool is_ref_ptr(Val v) {
  return (val_u(v) & (SIGN_FLAG | TAGGED_MASK | REF_FLAG)) ==
         (TAGGED_MASK | REF_FLAG);
}

// PTR_MASK isolates the pointer value and ignores the ownership flag.
#define PTR_MASK BYTES(00, 00, FF, FF, FF, FF, FF, FE)
static inline void *ptr(Val v) {
  return (void *)(uintptr_t)(val_u(v) & PTR_MASK);
}

// TYPE_MASK isolates the type discriminant from the stored value.
#define TYPE_MASK BYTES(ff, ff, 00, 00, 00, 00, 00, 00)

// String is the first pointer type. It points to a String and has the
// following layout:
// 01111111|11110100|........|........|........|........|........|.......o
#define STRING_PTR_TYPE BYTES(7f, f4, 00, 00, 00, 00, 00, 00)

#define IS_PTR_F(name, discriminant)                                           \
  static inline bool is_##name##_ptr(Val v) {                                  \
    return (val_u(v) & TYPE_MASK) == discriminant;                             \
  }

#define IS_PTR_OWN_F(name, discriminant)                                       \
  static inline bool is_##name##_own(Val v) {                                  \
    return (val_u(v) & (TYPE_MASK | REF_FLAG)) == discriminant;                \
  }

#define IS_PTR_REF_F(name, discriminant)                                       \
  static inline bool is_##name##_ref(Val v) {                                  \
    return (val_u(v) & (TYPE_MASK | REF_FLAG)) == (discriminant | REF_FLAG);   \
  }

#define PTR_F(name, type)                                                      \
  static inline type *name##_ptr(Val v) { return (type *)ptr(v); }

#define PTR_OWN_F(prefix, type, discriminant)                                  \
  static inline Val prefix##_own(type *t) {                                    \
    assert(!((intptr_t)t & (~PTR_MASK | REF_FLAG)));                           \
    return u_val((intptr_t)t | discriminant);                                  \
  }

#define PTR_REF_F(prefix, type, discriminant)                                  \
  static inline Val prefix##_ref(type *t) {                                    \
    assert(!((intptr_t)t & (~PTR_MASK | REF_FLAG)));                           \
    return u_val((intptr_t)t | discriminant | REF_FLAG);                       \
  }

IS_PTR_F(string, STRING_PTR_TYPE);                 // is_string_ptr
IS_PTR_OWN_F(string, STRING_PTR_TYPE);             // is_string_own
IS_PTR_REF_F(string, STRING_PTR_TYPE);             // is_string_ref
PTR_F(string, struct String);                      // string_ptr
PTR_OWN_F(string, struct String, STRING_PTR_TYPE); // string_own
PTR_REF_F(string, struct String, STRING_PTR_TYPE); // string_ref

// Next, we define the Table pointer which has the following layout:
// 01111111|11110101|........|........|........|........|........|.......o
#define TABLE_PTR_TYPE BYTES(7f, f5, 00, 00, 00, 00, 00, 00)
IS_PTR_F(table, TABLE_PTR_TYPE);                // is_table_ptr
IS_PTR_OWN_F(table, TABLE_PTR_TYPE);            // is_table_own
IS_PTR_REF_F(table, TABLE_PTR_TYPE);            // is_table_ref
PTR_F(table, struct Table);                     // table_ptr
PTR_OWN_F(table, struct Table, TABLE_PTR_TYPE); // table_own
PTR_REF_F(table, struct Table, TABLE_PTR_TYPE); // table_ref

// The List pointer has the following layout:
// 01111111|11110110|........|........|........|........|........|.......o
#define LIST_PTR_TYPE BYTES(7f, f6, 00, 00, 00, 00, 00, 00)
IS_PTR_F(list, LIST_PTR_TYPE);               // is_list_ptr
IS_PTR_OWN_F(list, LIST_PTR_TYPE);           // is_list_own
IS_PTR_REF_F(list, LIST_PTR_TYPE);           // is_list_ref
PTR_F(list, struct List);                    // list_ptr
PTR_OWN_F(list, struct List, LIST_PTR_TYPE); // list_own
PTR_REF_F(list, struct List, LIST_PTR_TYPE); // list_ref

// We also define a "big" 64-bit signed integer pointer (i.e., int64_t):
// 01111111|11110111|........|........|........|........|........|.......o
#define INT_PTR_TYPE BYTES(7f, f7, 00, 00, 00, 00, 00, 00)
IS_PTR_F(int, INT_PTR_TYPE);             // is_int_ptr
IS_PTR_OWN_F(int, INT_PTR_TYPE);         // is_int_own
IS_PTR_REF_F(int, INT_PTR_TYPE);         // is_int_ref
PTR_F(int, uint64_t);                    // int_ptr
PTR_OWN_F(int, uint64_t, LIST_PTR_TYPE); // int_own
PTR_REF_F(int, uint64_t, LIST_PTR_TYPE); // int_ref

// The next pointer value type represents an error and points to another Val
// which contains the error context (usually a String or Slice).
// NB: The ownership flag applies to the Val.
// 01111111|11111100|........|........|........|........|........|.......o
#define ERR_PTR_TYPE BYTES(7f, fc, 00, 00, 00, 00, 00, 00)
IS_PTR_F(err, ERR_PTR_TYPE);       // is_err_ptr
IS_PTR_OWN_F(err, ERR_PTR_TYPE);   // is_err_own
IS_PTR_REF_F(err, ERR_PTR_TYPE);   // is_err_ref
PTR_F(err, Val);                   // err_ptr
PTR_OWN_F(err, Val, ERR_PTR_TYPE); // err_own
PTR_REF_F(err, Val, ERR_PTR_TYPE); // err_ref

// Finally, we define a Slice pointer value type:
// 01111111|11111101|........|........|........|........|........|.......o
#define SLICE_PTR_TYPE BYTES(7f, fd, 00, 00, 00, 00, 00, 00)
IS_PTR_F(slice, SLICE_PTR_TYPE);                // is_silce_ptr
IS_PTR_OWN_F(slice, SLICE_PTR_TYPE);            // is_silce_own
IS_PTR_REF_F(slice, SLICE_PTR_TYPE);            // is_silce_ref
PTR_F(slice, struct Slice);                     // silce_ptr
PTR_OWN_F(slice, struct Slice, SLICE_PTR_TYPE); // silce_own
PTR_REF_F(slice, struct Slice, SLICE_PTR_TYPE); // silce_ref

// The remaining two pointer value types are reserved for later use:
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

#define IS_DATA_F(name, discriminant)                                          \
  static inline bool is_##name##_data(Val v) {                                 \
    return (val_u(v) & TYPE_MASK) == discriminant;                             \
  }
IS_DATA_F(pair, PAIR_DATA_TYPE); // is_pair_data

static inline uint16_t pair_ua(Val v) { return val_u(v) >> 32; }
static inline uint32_t pair_ub(Val v) { return val_u(v); }

static inline int16_t pair_a(Val v) {
  union {
    uint16_t as_uint16_t;
    int16_t as_int16_t;
  } x = {.as_uint16_t = pair_ua(v)};
  return x.as_int16_t;
}

static inline int32_t pair_b(Val v) {
  union {
    uint32_t as_uint32_t;
    int32_t as_int32_t;
  } x = {.as_int32_t = pair_ub(v)};
  return x.as_uint32_t;
}

// Next is the Symbol value which defines the following symbols: FALSE, TRUE,
// and NIL. These symbols and all others that fit in the least significant two
// bytes are reserved. The remaining 32-bit space is available for user-defined
// symbols and flags.
// 11111111|11110101|uuuuuuuu|uuuuuuuu|uuuuuuuu|uuuuuuuu|........|........
#define SYMB_DATA_TYPE BYTES(ff, f5, 00, 00, 00, 00, 00, 00)
IS_DATA_F(symb, SYMB_DATA_TYPE); // is_symb_data

static const Val VAL_FALSE = (Val){.u = SYMB_DATA_TYPE};
static const Val VAL_TRUE = (Val){.u = SYMB_DATA_TYPE + 1};
static const Val VAL_NIL = (Val){.u = SYMB_DATA_TYPE + 2};

static int is_false(Val v) { return val_u(v) == val_u(VAL_FALSE); }
static int is_true(Val v) { return val_u(v) == val_u(VAL_TRUE); }
static int is_nil(Val v) { return val_u(v) == val_u(VAL_NIL); }

static inline bool is_res_symbol(Val v) {
  return (val_u(v) >= SYMB_DATA_TYPE &&
          val_u(v) <= SYMB_DATA_TYPE + BYTES(00, 00, 00, 00, 00, 00, ff, ff));
}

static inline bool is_usr_symbol(Val v) {
  return (val_u(v) >= SYMB_DATA_TYPE + BYTES(00, 00, 00, 00, 00, 01, 00, 00) &&
          val_u(v) <= SYMB_DATA_TYPE + BYTES(00, 00, ff, ff, ff, ff, ff, ff));
}

static inline uint32_t usr_symb(Val v) { return val_u(v) >> 16; }

// There's also a 48-bit unsigned integer value:
// 11111111|11110110|........|........|........|........|........|........
#define UINT_DATA_TYPE BYTES(ff, f6, 00, 00, 00, 00, 00, 00)
IS_DATA_F(uint, UINT_DATA_TYPE); // is_uint_data

#define UINT_DATA_MAX ((2 << 48) - 1)
#define UINT_MASK BYTES(00, 00, ff, ff, ff, ff, ff, ff)
static inline uint64_t uint(Val v) { return val_u(v) & UINT_MASK; }

// The remaining five data value types are reserved for later use:
// 11111111|11110111|........|........|........|........|........|........
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........

#endif
