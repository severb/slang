#ifndef clox_val_h
#define clox_val_h

#include <assert.h>  // static_assert, assert
#include <limits.h>  // INT_MAX
#include <stdbool.h> // bool
#include <stddef.h>  // max_align_t, size_t
#include <stdint.h>  // *int16_t, *int32_t, *int64_t, UINT64_C, uintptr_t

// Val is a tagged union of all known pointer and value types. Vals are
// polymorphic and can be used with all collections. Because Vals are
// so versatile, they are also often used internally by the compiler and VM for
// bookkeeping.
//
// The Val type defined in this file uses a few tricks, including bit-packing
// technique named NaN tagging, which keeps the data compact. The methods
// defined are meant to hide all that away, and should make it easy to replace
// the Val implementation with something easier to debug.
//
// The Vals, except doubles, have the following invariant: two Vals are equal if
// they are equal bitwise. The reverse in not necessarily true--eg., two String
// pointer values can point to different, but equal, strings.
// The doubles are special because a NaN is not equal to anything.
typedef struct {
  union {
    double d;
    uint64_t u; // for bit fiddling
  };
} Val;

inline double val_data2double(Val v) { return v.d; }
inline Val val_data4double(double d) { return (Val){.d = d}; }

// Forward declarations of pointer types that Val is polymorphic to.
struct String;
struct Table;
struct List;
struct Slice;

// Vals are 64 bits long and use a bit-packing technique called NaN tagging.
static_assert(sizeof(uint64_t) == sizeof(Val), "Val size mismatch");

inline bool val_biteq(Val a, Val b) { return a.u == b.u; }

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

// We split the values in two categories: pointers and data. Pointer values have
// the most significant discriminant bit (the sign bit) unset, while data values
// have it set.

#define SIGN_FLAG BYTES(80, 00, 00, 00, 00, 00, 00, 00)

inline bool val_is_ptr(Val v) {
  return (v.u & (TAGGED_MASK | SIGN_FLAG)) == TAGGED_MASK;
}
inline bool val_is_data(Val v) { return !val_is_ptr(v); }

// Pointer values have additional tagging applied to their least significant
// bit. This is possible because pointers allocated with malloc are usually
// aligned; contrary to, for example, pointers to arbitrary chars in a string.
// We assume all pointers are at least two-byte aligned, which means we can use
// the least significant bit as a flag. If the flag is unset, the pointer value
// "owns" the data and is responsible for freeing it; otherwise, it's just a
// reference.
static_assert(sizeof(max_align_t) >= 2, "pointer alginment >= 2");

#define REF_FLAG BYTES(00, 00, 00, 00, 00, 00, 00, 01)

inline bool val_is_ptr_own(Val v) {
  return (v.u & (SIGN_FLAG | TAGGED_MASK | REF_FLAG)) == TAGGED_MASK;
}
inline bool val_is_ptr_ref(Val v) {
  return ((v.u & (SIGN_FLAG | TAGGED_MASK | REF_FLAG)) ==
          (TAGGED_MASK | REF_FLAG));
}

inline Val val_ptr2ref(Val v) {
  assert(val_is_ptr(v));
  return (Val){.u = (v.u | REF_FLAG)};
}

// PTR_MASK isolates the pointer value, ignoring the ownership flag.
#define PTR_MASK BYTES(00, 00, FF, FF, FF, FF, FF, FE)

inline void *val_ptr2ptr(Val v) {
  assert(val_is_ptr(v));
  return (void *)(uintptr_t)(v.u & PTR_MASK);
}

// TYPE_MASK isolates the type discriminant from the stored value.
#define TYPE_MASK BYTES(ff, ff, 00, 00, 00, 00, 00, 00)

typedef enum {
  VAL_DOUBLE,
  VAL_STRING = 0x7ff4,
  VAL_TABLE,
  VAL_LIST,
  VAL_I64,
  VAL_ERR = 0x7ffc,
  VAL_SLICE,
  VAL_PAIR = 0xfff4,
  VAL_SYMBOL,
} ValType;

// val_type() returns the type discriminant of the Val.
inline ValType val_type(Val v) {
  return (v.u & TAGGED_MASK) == TAGGED_MASK
             ? (uint16_t)((v.u & TYPE_MASK) >> 48)
             : VAL_DOUBLE;
}

// The String pointer type points to a struct with a flexible characters array
// member and has the following bit pattern:
// 01111111|11110100|........|........|........|........|........|.......o
#define STRING_PTR_TYPE BYTES(7f, f4, 00, 00, 00, 00, 00, 00)

inline Val val_ptr4string(struct String *s) {
  return (Val){.u = ((uintptr_t)s | STRING_PTR_TYPE)};
}
inline struct String *val_ptr2string(Val v) {
  assert((v.u & TYPE_MASK) == STRING_PTR_TYPE);
  return (struct String *)val_ptr2ptr(v);
}

// The Table pointer type points to a hash-map of Vals to Vals.
// 01111111|11110101|........|........|........|........|........|.......o
#define TABLE_PTR_TYPE BYTES(7f, f5, 00, 00, 00, 00, 00, 00)

inline Val val_ptr4table(struct Table *t) {
  return (Val){.u = ((uintptr_t)t | TABLE_PTR_TYPE)};
}
inline struct Table *val_ptr2table(Val v) {
  assert((v.u & TYPE_MASK) == TABLE_PTR_TYPE);
  return (struct Table *)val_ptr2ptr(v);
}

// The List pointer type pots to a list of Vals.
// 01111111|11110110|........|........|........|........|........|.......o
#define LIST_PTR_TYPE BYTES(7f, f6, 00, 00, 00, 00, 00, 00)

inline Val val_ptr4list(struct List *l) {
  return (Val){.u = ((uintptr_t)l | LIST_PTR_TYPE)};
}
inline struct List *val_ptr2list(Val v) {
  assert((v.u & TYPE_MASK) == LIST_PTR_TYPE);
  return (struct List *)val_ptr2ptr(v);
}

// A "big" 64-bit signed integer pointer (i.e., int64_t):
// 01111111|11110111|........|........|........|........|........|.......o
#define INT_PTR_TYPE BYTES(7f, f7, 00, 00, 00, 00, 00, 00)

inline Val val_ptr4int64(int64_t *i) {
  return (Val){.u = ((uintptr_t)i | INT_PTR_TYPE)};
}
inline int64_t *val_ptr2int64(Val v) {
  assert((v.u & TYPE_MASK) == INT_PTR_TYPE);
  return (int64_t *)val_ptr2ptr(v);
}

// The error pointer type points to another Val which contains the error
// context (usually a String or Slice).
// 01111111|11111100|........|........|........|........|........|.......o
#define ERR_PTR_TYPE BYTES(7f, fc, 00, 00, 00, 00, 00, 00)

inline bool val_is_err(Val v) { return val_type(v) == ERR_PTR_TYPE; }

inline Val val_ptr4err(Val *v) {
  return (Val){.u = ((uintptr_t)v | ERR_PTR_TYPE)};
}

inline Val *val_ptr2err(Val v) {
  assert((v.u & TYPE_MASK) == ERR_PTR_TYPE);
  return (Val *)val_ptr2ptr(v);
}

// The Slice pointer value type is similar to the String pointer type, but it
// can point to strings located in an arbitrary location:
// 01111111|11111101|........|........|........|........|........|.......o
#define SLICE_PTR_TYPE BYTES(7f, fd, 00, 00, 00, 00, 00, 00)

inline Val val_ptr4slice(struct Slice *s) {
  return (Val){.u = ((uintptr_t)s | SLICE_PTR_TYPE)};
}
inline struct Slice *val_ptr2slice(Val v) {
  assert((v.u & TYPE_MASK) == SLICE_PTR_TYPE);
  return (struct Slice *)val_ptr2ptr(v);
}

// The remaining two pointer value types are reserved for later use:
// 01111111|11111110|........|........|........|........|........|.......o
// 01111111|11111111|........|........|........|........|........|.......o

// Other than the pointer value types, we define a few data value types. All
// data value types contain their values directly in the storage area and have
// the most significant discriminant bit (the sign bit) set to 1.
// A double is also a data value type, but it's special because it acts as the
// "hosting" type for all other value types through NaN tagging.

// The Pair value type stores a pair of two integers A and B. A's size is two
// bytes and B's size is four bytes. One can choose to interpret A and B as
// signed or unsigned.
// 11111111|11110100|aaaaaaaa|aaaaaaaa|bbbbbbbb|bbbbbbbb|bbbbbbbb|bbbbbbbb
#define PAIR_DATA_TYPE BYTES(ff, f4, 00, 00, 00, 00, 00, 00)

union i16 {
  uint16_t as_uint16_t;
  int16_t as_int16_t;
};
union i32 {
  uint32_t as_uint32_t;
  int32_t as_int32_t;
};

inline uint16_t val_data2pair_ua(Val v) {
  assert((v.u & TYPE_MASK) == PAIR_DATA_TYPE);
  return (uint16_t)(v.u >> 32);
}
inline uint32_t val_data2pair_ub(Val v) {
  assert((v.u & TYPE_MASK) == PAIR_DATA_TYPE);
  return (uint32_t)v.u;
}

inline int16_t val_data2pair_a(Val v) {
  return (union i16){.as_uint16_t = val_data2pair_ua(v)}.as_int16_t;
}
inline int32_t val_data2pair_b(Val v) {
  return (union i32){.as_uint32_t = val_data2pair_ub(v)}.as_int32_t;
}

inline Val val_data4upair(uint16_t a, uint32_t b) {
  uint64_t aa = ((uint64_t)a) << 32;
  uint64_t bb = (uint64_t)b;
  return (Val){.u = (PAIR_DATA_TYPE | aa | bb)};
}
inline Val val_data4pair(int16_t a, int32_t b) {
  uint16_t aa = (union i16){.as_int16_t = a}.as_uint16_t;
  uint32_t bb = (union i32){.as_int32_t = b}.as_uint32_t;
  return val_data4upair(aa, bb);
}

// The Symbol value type defines the following symbols:
// FALSE, TRUE, OK and NIL. These symbols and all others that fit
// in the least significant two bytes are reserved. The remaining
// values are available for user-defined symbols and flags.
// 11111111|11110101|........|........|........|........|........|........
#define SYMB_DATA_TYPE BYTES(ff, f5, 00, 00, 00, 00, 00, 00)

typedef enum { SYM_FALSE, SYM_TRUE, SYM_NIL, SYM_OK } Symbol;

inline Symbol val_data2symbol(Val v) {
  assert((v.u & TYPE_MASK) == (SYMB_DATA_TYPE));
  assert((v.u & ~TYPE_MASK) < (uint64_t)INT_MAX);
  return v.u & 0x7F;
}

#define VAL_FALSE ((Val){.u = (SYMB_DATA_TYPE) | SYM_FALSE})
#define VAL_TRUE ((Val){.u = (SYMB_DATA_TYPE) | SYM_TRUE})
#define VAL_NIL ((Val){.u = (SYMB_DATA_TYPE) | SYM_NIL})
#define VAL_OK ((Val){.u = (SYMB_DATA_TYPE) | SYM_OK})

#define USR_SYMBOL(x) ((Val){.u = ((SYMB_DATA_TYPE) | ((x) + SYM_OK + 1))})

// The remaining data value types are reserved for later use:
// 11111111|11110110|........|........|........|........|........|........
// 11111111|11110111|........|........|........|........|........|........
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........

void val_free(Val);
void val_print(Val);
// void val_print_repr(Val) <- this is different for strings!
size_t val_hash(Val);
bool val_is_true(Val);
bool val_eq(Val, Val);
// int val_cmp(Val, Val); // maybe return Val for errors?
Val val_add(Val, Val);
Val val_sub(Val, Val);

#endif
