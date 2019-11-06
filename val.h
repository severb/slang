#ifndef clox_val_h
#define clox_val_h

#include <assert.h>  // static_assert, assert
#include <stdbool.h> // bool
#include <stddef.h>  // max_align_t
#include <stdint.h>  // *int16_t, *int32_t, *int64_t, UINT64_C

// Val is a tagged union. It stores all value types exposed by the language and
// can be used with all collections. Because Vals are so versatile, they are
// often used internally by the compiler and VM for bookkeeping. All Vals,
// except doubles, have the following invariant: two Vals are equal if their
// bit strings are equal. The reverse in not necessarily true. For example, two
// String pointer values can point to different, but equal, strings.
typedef struct Val {
  union {
    double d;
    uint64_t u; // for bit fiddling
  };
} Val;

// Forward declarations of some types defined elsewhere:
struct String;
struct Slice;
struct Table;
struct List_val;

// Vals are 64 bits long and use a bit-packing technique called NaN tagging.
static_assert(sizeof(uint64_t) == sizeof(Val), "Val size mismatch");

#define val_u_(x) ((x).u)
static inline uint64_t val_u(Val v) { return val_u_(v); }

#define val_d_(x) ((x).d)
static inline double val_d(Val v) { return val_d_(v); }

#define u_val_(x) ((Val){.u = (x)})
static inline Val u_val(uint64_t u) { return u_val_(u); }

#define d_val_(x) ((Val){.d = (x)})
static inline Val d_val(double d) { return d_val_(d); }

#define val_biteq_(a, b) (val_u_(a) == val_u_(b))
static inline bool val_biteq(Val a, Val b) { return val_biteq_(a, b); }

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

#define is_tagged_(v) ((val_u_(v) & TAGGED_MASK) == TAGGED_MASK)
static inline bool is_tagged(Val v) { return is_tagged_(v); }

#define is_double_(v) (!is_tagged_(v))
static inline bool is_double(Val v) { return is_double_(v); }

// We define two categories of values: pointers and data. Pointer values have
// the most significant discriminant bit (the sign bit) set, while data values
// have it unset.

#define SIGN_FLAG BYTES(80, 00, 00, 00, 00, 00, 00, 00)

#define is_ptr_(v) ((val_u_(v) & (TAGGED_MASK | SIGN_FLAG)) == TAGGED_MASK)
static inline bool is_ptr(Val v) { return is_ptr_(v); }

#define is_data_(v)                                                            \
  ((val_u_(v) & (TAGGED_MASK | SIGN_FLAG)) == (TAGGED_MASK | SIGN_FLAG))
static inline bool is_data(Val v) { return is_data_(v); }

// Pointer values have additional tagging applied to their least significant
// bit. This is possible because pointers allocated with malloc are usually
// aligned; contrary to, for example, pointers to arbitrary chars in a string.
// We assume all pointers are at least two-byte aligned, which means we can use
// the least significant bit as a flag. If the flag is unset, the pointer value
// "owns" the data and is responsible for freeing it; otherwise, it's just a
// reference.
static_assert(sizeof(max_align_t) >= 2, "pointer alginment >= 2");

#define REF_FLAG BYTES(00, 00, 00, 00, 00, 00, 00, 01)

#define is_ptr_own_(v)                                                         \
  ((val_u_(v) & (SIGN_FLAG | TAGGED_MASK | REF_FLAG)) == TAGGED_MASK)
static inline bool is_ptr_own(Val v) { return is_ptr_own_(v); }

#define is_ptr_ref_(v)                                                         \
  ((val_u_(v) & (SIGN_FLAG | TAGGED_MASK | REF_FLAG)) ==                       \
   (TAGGED_MASK | REF_FLAG))
static inline bool is_ptr_ref(Val v) { return is_ptr_ref_(v); }

#define ref_(v) (u_val_(val_u_(v) & REF_FLAG))
static inline Val ref(Val v) { return is_ptr_own(v) ? ref_(v) : v; }

// PTR_MASK isolates the pointer value and ignores the ownership flag.
#define PTR_MASK BYTES(00, 00, FF, FF, FF, FF, FF, FE)

#define ptr_(v) ((void *)(uintptr_t)(val_u_(v) & PTR_MASK))
static inline void *ptr(Val v) { return ptr_(v); }

// TYPE_MASK isolates the type discriminant from the stored value.
#define TYPE_MASK BYTES(ff, ff, 00, 00, 00, 00, 00, 00)

#define same_type_(a, b) ((val_u_(a) & TYPE_MASK) == (val_u_(b) && TYPE_MASK))
static inline bool same_type(Val a, Val b) { return same_type_(a, b); }

// String is the first pointer type. It points to a String and has the
// following layout:
// 01111111|11110100|........|........|........|........|........|.......o
#define STRING_PTR_TYPE BYTES(7f, f4, 00, 00, 00, 00, 00, 00)

#define is_type_(discriminant, v) ((val_u_(v) & TYPE_MASK) == (discriminant))

#define IS_PTR_F(name, discriminant)                                           \
  static inline bool is_##name##_ptr(Val v) {                                  \
    return is_type_((discriminant), v);                                        \
  }

#define is_ptr_x_own_(discriminant, v)                                         \
  ((val_u_(v) & (TYPE_MASK | REF_FLAG)) == (discriminant))

#define IS_PTR_OWN_F(name, discriminant)                                       \
  static inline bool is_##name##_own(Val v) {                                  \
    return is_ptr_x_own_((discriminant), v);                                   \
  }

#define is_ptr_x_ref_(discriminant, v)                                         \
  ((val_u_(v) & (TYPE_MASK | REF_FLAG)) == ((discriminant) | REF_FLAG))

#define IS_PTR_REF_F(name, discriminant)                                       \
  static inline bool is_##name##_ref(Val v) {                                  \
    return is_ptr_x_ref_((discriminant), v);                                   \
  }

#define PTR_F(name, type)                                                      \
  static inline type *name##_ptr(Val v) { return (type *)ptr(v); }

#define ptr_own_(discriminant, ptr) (u_val_((uintptr_t)ptr | (discriminant)))

#define PTR_OWN_F(prefix, type, discriminant)                                  \
  static inline Val prefix##_own(type *t) {                                    \
    assert(((uintptr_t)t & (TYPE_MASK | REF_FLAG)) == 0);                      \
    return ptr_own_((discriminant), t);                                        \
  }

#define ptr_ref_(discriminant, ptr)                                            \
  (u_val_((uintptr_t)ptr | (discriminant) | REF_FLAG))

#define PTR_REF_F(prefix, type, discriminant)                                  \
  static inline Val prefix##_ref(type *t) {                                    \
    assert(((uintptr_t)t & (TYPE_MASK | REF_FLAG)) == 0);                      \
    return ptr_ref_((discriminant), t);                                        \
  }

IS_PTR_F(string, STRING_PTR_TYPE)                 // is_string_ptr
IS_PTR_OWN_F(string, STRING_PTR_TYPE)             // is_string_own
IS_PTR_REF_F(string, STRING_PTR_TYPE)             // is_string_ref
PTR_F(string, struct String)                      // string_ptr
PTR_OWN_F(string, struct String, STRING_PTR_TYPE) // string_own
PTR_REF_F(string, struct String, STRING_PTR_TYPE) // string_ref

// Next, we define the Table pointer which has the following layout:
// 01111111|11110101|........|........|........|........|........|.......o
#define TABLE_PTR_TYPE BYTES(7f, f5, 00, 00, 00, 00, 00, 00)
IS_PTR_F(table, TABLE_PTR_TYPE)                // is_table_ptr
IS_PTR_OWN_F(table, TABLE_PTR_TYPE)            // is_table_own
IS_PTR_REF_F(table, TABLE_PTR_TYPE)            // is_table_ref
PTR_F(table, struct Table)                     // table_ptr
PTR_OWN_F(table, struct Table, TABLE_PTR_TYPE) // table_own
PTR_REF_F(table, struct Table, TABLE_PTR_TYPE) // table_ref

// The List pointer has the following layout:
// 01111111|11110110|........|........|........|........|........|.......o
#define LIST_PTR_TYPE BYTES(7f, f6, 00, 00, 00, 00, 00, 00)
IS_PTR_F(list, LIST_PTR_TYPE)               // is_list_ptr
IS_PTR_OWN_F(list, LIST_PTR_TYPE)           // is_list_own
IS_PTR_REF_F(list, LIST_PTR_TYPE)           // is_list_ref
PTR_F(list, struct List_val)                    // list_ptr
PTR_OWN_F(list, struct List_val, LIST_PTR_TYPE) // list_own
PTR_REF_F(list, struct List_val, LIST_PTR_TYPE) // list_ref

// We also define a "big" 64-bit signed integer pointer (i.e., int64_t):
// 01111111|11110111|........|........|........|........|........|.......o
#define INT_PTR_TYPE BYTES(7f, f7, 00, 00, 00, 00, 00, 00)
IS_PTR_F(int, INT_PTR_TYPE)           // is_int_ptr
IS_PTR_OWN_F(int, INT_PTR_TYPE)       // is_int_own
IS_PTR_REF_F(int, INT_PTR_TYPE)       // is_int_ref
PTR_F(int, int64_t)                   // int_ptr
PTR_OWN_F(int, int64_t, INT_PTR_TYPE) // int_own
PTR_REF_F(int, int64_t, INT_PTR_TYPE) // int_ref

// The next pointer value type represents an error and points to another Val
// which contains the error context (usually a String or Slice).
// NB: The ownership flag applies to the Val.
// 01111111|11111100|........|........|........|........|........|.......o
#define ERR_PTR_TYPE BYTES(7f, fc, 00, 00, 00, 00, 00, 00)
IS_PTR_F(err, ERR_PTR_TYPE)       // is_err_ptr
IS_PTR_OWN_F(err, ERR_PTR_TYPE)   // is_err_own
IS_PTR_REF_F(err, ERR_PTR_TYPE)   // is_err_ref
PTR_F(err, Val)                   // err_ptr
PTR_OWN_F(err, Val, ERR_PTR_TYPE) // err_own
PTR_REF_F(err, Val, ERR_PTR_TYPE) // err_ref

// Finally, we define a Slice pointer value type:
// 01111111|11111101|........|........|........|........|........|.......o
#define SLICE_PTR_TYPE BYTES(7f, fd, 00, 00, 00, 00, 00, 00)
IS_PTR_F(slice, SLICE_PTR_TYPE)                // is_silce_ptr
IS_PTR_OWN_F(slice, SLICE_PTR_TYPE)            // is_silce_own
IS_PTR_REF_F(slice, SLICE_PTR_TYPE)            // is_silce_ref
PTR_F(slice, struct Slice)                     // silce_ptr
PTR_OWN_F(slice, struct Slice, SLICE_PTR_TYPE) // silce_own
PTR_REF_F(slice, struct Slice, SLICE_PTR_TYPE) // silce_ref

// The remaining two pointer value types are reserved for later use:
// 01111111|11111110|........|........|........|........|........|.......o
// 01111111|11111111|........|........|........|........|........|.......o

// Other than the pointer value types, we define a few data value types. All
// data value types contain their values directly in the storage area and have
// the most significant discriminant bit (the sign bit) set to 1.
// A double is also a data value type, but it's special because it acts as the
// "hosting" type for all other value types through NaN tagging.

// The first data value type is the Pair. It stores a pair of two integers a
// and b. a's size is two bytes and b's size is four bytes:
// 11111111|11110100|aaaaaaaa|aaaaaaaa|bbbbbbbb|bbbbbbbb|bbbbbbbb|bbbbbbbb
#define PAIR_DATA_TYPE BYTES(ff, f4, 00, 00, 00, 00, 00, 00)

#define IS_DATA_F(name, discriminant)                                          \
  static inline bool is_##name##_data(Val v) {                                 \
    return is_type_((discriminant), v);                                        \
  }
IS_DATA_F(pair, PAIR_DATA_TYPE) // is_pair_data

#define pair_ua_(v) ((uint16_t)(val_u_(v) >> 32))
static inline uint16_t pair_ua(Val v) { return pair_ua_(v); }

#define pair_ub_(v) ((uint32_t)(val_u_(v)))
static inline uint32_t pair_ub(Val v) { return pair_ub_(v); }

union i16 {
  uint16_t as_uint16_t;
  int16_t as_int16_t;
};
union i32 {
  uint32_t as_uint32_t;
  int32_t as_int32_t;
};

#define pair_a_(v) ((union i16){.as_uint16_t = pair_ua_(v)}.as_int16_t)
static inline int16_t pair_a(Val v) { return pair_a_(v); }

#define pair_b_(v) ((union i32){.as_uint32_t = pair_ub_(v)}.as_int32_t)
static inline int32_t pair_b(Val v) { return pair_b_(v); }

#define upair_(a, b)                                                           \
  (u_val_(PAIR_DATA_TYPE | ((uint64_t)(a) << 32) | (uint64_t)(b)))
static inline Val upair(uint16_t a, uint32_t b) { return upair_(a, b); }

#define pair_(a, b)                                                            \
  (upair_((union i16){.as_int16_t = (a)}.as_uint16_t,                          \
          (union i32){.as_int32_t = (b)}.as_uint32_t))
static inline Val pair(int16_t a, int32_t b) { return pair_(a, b); }

// Next is the Symbol value which defines the following symbols:
// FALSE, TRUE, OK and NIL. These symbols and all others that fit
// in the least significant two bytes are reserved. The remaining
// values are available for user-defined symbols and flags.
// 11111111|11110101|........|........|........|........|........|........
#define SYMB_DATA_TYPE BYTES(ff, f5, 00, 00, 00, 00, 00, 00)
IS_DATA_F(symb, SYMB_DATA_TYPE) // is_symb_data

#define FALSEu (SYMB_DATA_TYPE)
#define FALSE u_val_(SYMB_DATA_TYPE)

#define TRUEu (SYMB_DATA_TYPE + 1)
#define TRUE u_val_(TRUEu)

#define OKu (SYMB_DATA_TYPE + 2)
#define OK u_val_(OKu)

#define NILu (SYMB_DATA_TYPE + 3)
#define NIL u_val_(NILu)

#define is_false_(v) (val_biteq_((v), FALSE))
static bool is_false(Val v) { return is_false_(v); }

#define is_true_(v) (val_biteq_((v), TRUE))
static bool is_true(Val v) { return is_true_(v); }

#define is_ok_(v) (val_biteq_((v), OK))
static bool is_ok(Val v) { return is_ok_(v); }

#define is_nil_(v) (val_biteq_((v), NIL))
static bool is_nil(Val v) { return is_nil_(v); }

#define USR_START (SYMB_DATA_TYPE | BYTES(00, 00, 00, 00, 00, 01, 00, 00))
#define USR_END (SYMB_DATA_TYPE | BYTES(00, 00, ff, ff, ff, ff, ff, ff))

static inline bool is_res_symbol(Val v) {
  return val_u(v) >= SYMB_DATA_TYPE && val_u(v) < USR_START;
}

static inline bool is_usr_symbol(Val v) {
  return val_u(v) >= USR_START && val_u(v) <= USR_END;
}

#define val_usr_(v) ((val_u_(v) | ~TYPE_MASK) - USR_START)
static inline uint64_t val_usr(Val v) {
  assert(is_usr_symbol(v));
  return val_usr_(v);
}

#define usr_val_(v) (u_val_(SYMB_DATA_TYPE | (v + USR_START)))
static inline Val usr_val(uint64_t sym) {
  Val res = usr_val_(sym);
  assert(is_usr_symbol(res));
  return res;
}

// The remaining data value types are reserved for later use:
// 11111111|11110110|........|........|........|........|........|........
// 11111111|11110111|........|........|........|........|........|........
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........

void val_destroy(Val);
void val_print(Val);
void val_print_repr(Val);
uint32_t val_hash(Val);
bool val_truthy(Val);

bool val_equals(Val, Val);

#endif
