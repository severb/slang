#ifndef slang_tag_h
#define slang_tag_h

#include <assert.h>  // static_assert, assert
#include <limits.h>  // INT_MAX
#include <stdbool.h> // bool
#include <stddef.h>  // max_align_t, size_t
#include <stdint.h>  // *int16_t, *int32_t, *int64_t, UINT64_C, uintptr_t

// A Tag is a discriminated union of all known Slang types.
//
// It uses NaN tagging to pack pointers and their type discriminant together.
// Tags can also embed a few small primitives, like floats, pairs, and symbols,
// to save dereferences.
//
// All types, except doubles (because of their NaNs), havethe following
// invariant: two values are equal if their tags are equal bitwise. The reverse
// in not necessarily true--eg., two pointers can reference equal, but
// distinct, values.
typedef struct {
  union {
    double d;
    uint64_t u; // for bit fiddling
  };
} Tag;

// Tags are 64 bit long
static_assert(sizeof(uint64_t) == sizeof(Tag), "Tag size is not 64");

// Forward declarations of tag pointer types.
struct String;
struct Table;
struct List;
struct Slice;

typedef struct String String;
typedef struct Table Table;
typedef struct List List;
typedef struct Slice Slice;

inline bool tag_biteq(Tag a, Tag b) { return a.u == b.u; }

// An aid to define 64 bit patterns:
#define BYTES(a, b, c, d, e, f, g, h) (UINT64_C(0x##a##b##c##d##e##f##g##h))

// NaN tagging primer
//
// Double layout (s: sign bit, e: exponent, f: fraction)
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
//
// Double NaNs have all exponent bits set high -- the sign bit is ignored. In
// addition, there are two different types of NaNs: quiet (qNaN) and signaling
// (sNaN). The two are differentiated by the most significant bit of the
// fraction: qNaNs have it set high and sNaNs have it low. sNaNs must also have
// any other fraction bit set (usually the least significant) to distinguish
// them from infinity.
//
// Typically, these NaNs are encoded like so:
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
// 01111111|11111000|00000000|00000000|00000000|00000000|00000000|00000000
// 01111111|11110000|00000000|00000000|00000000|00000000|00000000|00000001
//
// This leaves enough space to use for other things, like storing small values
// or 48-bit pointers. To do so, we use the following bit-pattern to tell tags
// from doubles:
// .1111111|1111.1..|........|........|........|........|........|........
//
// The top four most significant bits form the type discriminant and the
// remaining 6 bytes store pointers or other values that can fit.

#define TAGGED_MASK BYTES(7f, f4, 00, 00, 00, 00, 00, 00)

// We split the tags in two categories: pointers and data. Pointer values have
// the most significant discriminant bit (the sign bit) unset, while data
// values have it set.

#define SIGN_FLAG BYTES(80, 00, 00, 00, 00, 00, 00, 00)

inline bool tag_is_ptr(Tag t) {
  return (t.u & (TAGGED_MASK | SIGN_FLAG)) == TAGGED_MASK;
}
inline bool tag_is_data(Tag t) { return !tag_is_ptr(t); }

// Pointers have additional tagging applied to their least significant bit.
// This is possible because pointers allocated with malloc are usually aligned;
// contrary to, for example, pointers to arbitrary chars in a string. We assume
// all pointers are at least two-byte aligned, which means we can use the least
// significant bit as a flag. If the flag is unset, the pointer value "owns"
// the data and is responsible for freeing it; otherwise, it's just a
// reference.
static_assert(sizeof(max_align_t) >= 2, "pointer alginment >= 2");

inline bool tag_is_own(Tag t) {
  return (t.u & (SIGN_FLAG | TAGGED_MASK | 1)) == TAGGED_MASK;
}

inline bool tag_is_ref(Tag t) {
  return ((t.u & (SIGN_FLAG | TAGGED_MASK | 1)) == (TAGGED_MASK | 1));
}

inline Tag tag_to_ref(Tag t) {
  assert(tag_is_ptr(t));
  return (Tag){.u = (t.u | 1)};
}

// PTR_MASK isolates the pointer value, ignoring the ownership flag.
#define PTR_MASK BYTES(00, 00, FF, FF, FF, FF, FF, FE)

inline void *tag_to_ptr(Tag t) {
  assert(tag_is_ptr(t));
  return (void *)(uintptr_t)(t.u & PTR_MASK);
}

// DISCRIMINANT_MASK isolates the type discriminant from the stored value.
#define DISCRIMINANT_MASK BYTES(ff, ff, 00, 00, 00, 00, 00, 00)

#define STRING_DISCRIMINANT BYTES(7f, f4, 00, 00, 00, 00, 00, 00)
inline bool tag_is_string(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == STRING_DISCRIMINANT);
}
inline Tag string_to_tag(const String *s) {
  uintptr_t p = (uintptr_t)s;
  assert((p & DISCRIMINANT_MASK) == 0);
  return (Tag){.u = p | STRING_DISCRIMINANT};
}
inline String *tag_to_string(Tag t) {
  assert(tag_is_string(t));
  return (String *)tag_to_ptr(t);
}

#define TABLE_DISCRIMINANT BYTES(7f, f5, 00, 00, 00, 00, 00, 00)
inline bool tag_is_table(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == TABLE_DISCRIMINANT);
}
inline Tag table_to_tag(const Table *t) {
  uintptr_t p = (uintptr_t)t;
  assert((p & DISCRIMINANT_MASK) == 0);
  return (Tag){.u = p | TABLE_DISCRIMINANT};
}
inline Table *tag_to_table(Tag t) {
  assert(tag_is_table(t));
  return (Table *)tag_to_ptr(t);
}

#define LIST_DISCRIMINANT BYTES(7f, f6, 00, 00, 00, 00, 00, 00)
inline bool tag_is_list(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == LIST_DISCRIMINANT);
}
inline Tag list_to_tag(const List *l) {
  uintptr_t p = (uintptr_t)l;
  assert((p & DISCRIMINANT_MASK) == 0);
  return (Tag){.u = p | LIST_DISCRIMINANT};
}
inline List *tag_to_list(Tag t) {
  assert(tag_is_list(t));
  return (List *)tag_to_ptr(t);
}

#define I64_DISCRIMINANT BYTES(7f, f7, 00, 00, 00, 00, 00, 00)
inline bool tag_is_i64(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == I64_DISCRIMINANT);
}
inline Tag i64_to_tag(const int64_t *i) {
  uintptr_t p = (uintptr_t)i;
  assert((p & DISCRIMINANT_MASK) == 0);
  return (Tag){.u = p | I64_DISCRIMINANT};
}
inline int64_t *tag_to_i64(Tag t) {
  assert(tag_is_i64(t));
  return (int64_t *)tag_to_ptr(t);
}

#define ERROR_DISCRIMINANT BYTES(7f, fc, 00, 00, 00, 00, 00, 00)
inline bool tag_is_error(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == ERROR_DISCRIMINANT);
}
inline Tag error_to_tag(const Tag *t) {
  uintptr_t p = (uintptr_t)t;
  assert((p & DISCRIMINANT_MASK) == 0);
  return (Tag){.u = p | ERROR_DISCRIMINANT};
}
inline Tag *tag_to_error(Tag t) {
  assert(tag_is_error(t));
  return (Tag *)tag_to_ptr(t);
}

#define SLICE_DISCRIMINANT BYTES(7f, fd, 00, 00, 00, 00, 00, 00)
inline bool tag_is_slice(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == SLICE_DISCRIMINANT);
}
inline Tag slice_to_tag(const Slice *s) {
  uintptr_t p = (uintptr_t)s;
  assert((p & DISCRIMINANT_MASK) == 0);
  return (Tag){.u = p | SLICE_DISCRIMINANT};
}
inline Slice *tag_to_slice(Tag t) {
  assert(tag_is_slice(t));
  return (Slice *)tag_to_ptr(t);
}

// There are two pointer tags left:
// 01111111|11111110|........|........|........|........|........|.......o
// 01111111|11111111|........|........|........|........|........|.......o

// Other than the pointer tags, we define a few data tags which embed their
// values directly in the tag. These tags have the most significant
// discriminant bit (the sign bit) set to 1.
//
// Dobules are also considered data tags, but they don't match the TAGGED_MASK.

#define SAFE_CAST(FROM, TO, VAL)                                               \
  (union {                                                                     \
    FROM f;                                                                    \
    TO t;                                                                      \
  }){.f = VAL}                                                                 \
      .t

#define PAIR_DISCRIMINANT BYTES(ff, f4, 00, 00, 00, 00, 00, 00)
inline bool tag_is_pair(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == PAIR_DISCRIMINANT);
}
inline Tag upair_to_tag(uint16_t a, uint32_t b) {
  uint64_t aa = ((uint64_t)a) << 32;
  uint64_t bb = (uint64_t)b;
  return (Tag){.u = (PAIR_DISCRIMINANT | aa | bb)};
}
inline Tag pair_to_tag(int16_t a, int32_t b) {
  uint16_t aa = SAFE_CAST(int16_t, uint16_t, a);
  uint32_t bb = SAFE_CAST(int32_t, uint32_t, b);
  return upair_to_tag(aa, bb);
}
inline uint16_t tag_to_pair_ua(Tag t) {
  assert(tag_is_pair(t));
  return (t.u & BYTES(00, 00, ff, ff, 00, 00, 00, 00)) >> 32;
}
inline uint32_t tag_to_pair_ub(Tag t) {
  assert(tag_is_pair(t));
  return t.u & BYTES(00, 00, 00, 00, ff, ff, ff, ff);
}
inline int16_t tag_to_pair_a(Tag t) {
  uint16_t ua = tag_to_pair_ua(t);
  return SAFE_CAST(uint16_t, int16_t, ua);
}
inline int32_t tag_to_pair_b(Tag t) {
  uint32_t ub = tag_to_pair_ub(t);
  return SAFE_CAST(uint32_t, int32_t, ub);
}

#define SYMBOL_DISCRIMINANT BYTES(ff, f5, 00, 00, 00, 00, 00, 00)
inline bool tag_is_symbol(Tag t) {
  return ((t.u & DISCRIMINANT_MASK) == SYMBOL_DISCRIMINANT);
}

typedef enum { SYM_FALSE, SYM_TRUE, SYM_NIL, SYM_OK, SYM__COUNT } Symbol;

#define TAG_FALSE ((Tag){.u = (SYMBOL_DISCRIMINANT) | SYM_FALSE})
#define TAG_TRUE ((Tag){.u = (SYMBOL_DISCRIMINANT) | SYM_TRUE})
#define TAG_NIL ((Tag){.u = (SYMBOL_DISCRIMINANT) | SYM_NIL})
#define TAG_OK ((Tag){.u = (SYMBOL_DISCRIMINANT) | SYM_OK})

#define USER_SYMBOL(x) ((Tag){.u = (SYMBOL_DISCRIMINANT | ((x) + SYM__COUNT))})

inline Symbol tag_to_symbol(Tag t) {
  assert(tag_is_symbol(t));
  uint64_t symbol = (t.u & ~DISCRIMINANT_MASK);
  assert(symbol < INT_MAX);
  return symbol;
}

inline bool tag_is_double(Tag t) { return (t.u & TAGGED_MASK) != TAGGED_MASK; }
inline double tag_to_double(Tag t) { return t.d; }
inline Tag double_to_tag(double d) { return (Tag){.d = d}; }

// There are six data tags left:
// 11111111|11110110|........|........|........|........|........|........
// 11111111|11110111|........|........|........|........|........|........
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........

typedef enum {
  TYPE_STRING = 0,
  TYPE_TABLE,
  TYPE_LIST,
  TYPE_I64,
  TYPE_PAIR,
  TYPE_SYMBOL,
  TYPE_ERROR = 8,
  TYPE_SLICE,
  TYPE_DOUBLE,
} TagType;

// tag_type() returns the type discriminant
inline TagType tag_type(Tag t) {
  if ((t.u & TAGGED_MASK) != TAGGED_MASK) {
    return TYPE_DOUBLE;
  }
  // convert the type discriminant to a TagType
  uint64_t a = (t.u & BYTES(00, 0b, 00, 00, 00, 00, 00, 00)) >> 48;
  uint64_t b = (t.u & BYTES(80, 00, 00, 00, 00, 00, 00, 00)) >> 61;
  return (int)(a | b);
}

void tag_free(Tag);
void tag_print(Tag);
void tag_repr(Tag);
size_t tag_hash(Tag);
bool tag_is_true(Tag);
bool tag_eq(Tag, Tag);
Tag tag_add(Tag, Tag);
// Tag tag_cmp(Tag, Tag); // use symbols for lt, eq, gt--can also return err

#endif
