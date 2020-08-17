#ifndef slang_tag_h
#define slang_tag_h

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// A Tag is a discriminated union of all known Slang types.
//
// It uses NaN tagging to pack pointers and their type discriminant together. Tags can also embed a
// few small primitives, like floats, 49 bit inegers, and symbols, to save dereferences.
//
// All types, except doubles (because of their NaNs), havethe following invariant: two values are
// equal if their tags are equal bitwise. The reverse in not necessarily true--eg., two pointers can
// reference equal, but distinct, values.
typedef struct {
    union {
        double d;
        uint64_t u; // for bit fiddling
    };
} Tag;

// Tags are 64 bit long
#ifndef NDEBUG
static_assert(sizeof(uint64_t) == sizeof(Tag), "Tag size is not 64");
#endif

// Forward declarations of tag pointer types.
typedef struct String String;
typedef struct Table Table;
typedef struct List List;
typedef struct Slice Slice;
typedef struct Fun Fun;

inline bool tag_biteq(Tag a, Tag b) { return a.u == b.u; }

// An aid to define 64 bit patterns:
#define BYTES(a, b, c, d, e, f, g, h) (UINT64_C(0x##a##b##c##d##e##f##g##h))

// NaN tagging primer
//
// Double layout (s: sign bit, e: exponent, f: fraction)
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
//
// Double NaNs have all exponent bits set high -- the sign bit is ignored. In addition, there are
// two different types of NaNs: quiet (qNaN) and signaling (sNaN). The two are differentiated by the
// most significant bit of the fraction: qNaNs have it set high and sNaNs have it low. sNaNs must
// also have any other fraction bit set (usually the least significant) to distinguish them from
// infinity.
//
// Typically, these NaNs are encoded like so:
// seeeeeee|eeeeffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff|ffffffff
// 01111111|11111000|00000000|00000000|00000000|00000000|00000000|00000000
// 01111111|11110000|00000000|00000000|00000000|00000000|00000000|00000001
//
// This leaves enough space to use for other things, like storing small values or 48-bit pointers.
// To do so, we use the following bit-pattern to tell tags from doubles:
// .1111111|1111.1..|........|........|........|........|........|........
//
// The top four most significant bits form the type discriminant and the remaining 6 bytes store
// pointers or other values that can fit.

#define TAGGED_MASK BYTES(7f, f4, 00, 00, 00, 00, 00, 00)

// We split the tags in two categories: pointers and data. Pointer values have the most significant
// discriminant bit (the sign bit) unset, while data values have it set.

#define SIGN_FLAG BYTES(80, 00, 00, 00, 00, 00, 00, 00)

inline bool tag_is_ptr(Tag t) { return (t.u & (SIGN_FLAG | TAGGED_MASK)) == TAGGED_MASK; }
inline bool tag_is_data(Tag t) { return !tag_is_ptr(t); }

// Pointers have additional tagging applied to their least significant bit. This is possible because
// pointers allocated with malloc are usually aligned; contrary to, for example, pointers to
// arbitrary chars in a string. We assume all pointers are at least two-byte aligned, which means we
// can use the least significant bit as a flag. If the flag is unset, the pointer value "owns" the
// data and is responsible for freeing it; otherwise, it's just a reference.
#ifndef NDEBUG
static_assert(sizeof(max_align_t) >= 2, "pointer alginment is too small");
static_assert(UINTPTR_MAX <= UINT64_MAX, "max pointer value exceedes uint64_t");
#endif

inline bool tag_is_own(Tag t) { return (t.u & (SIGN_FLAG | TAGGED_MASK | 1)) == TAGGED_MASK; }

inline bool tag_is_ref(Tag t) {
    return ((t.u & (SIGN_FLAG | TAGGED_MASK | 1)) == (TAGGED_MASK | 1));
}

inline Tag tag_to_ref(Tag t) {
    assert(tag_is_ptr(t));
    return (Tag){.u = t.u | 1};
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
inline bool tag_is_string(Tag t) { return ((t.u & DISCRIMINANT_MASK) == STRING_DISCRIMINANT); }
inline Tag string_to_tag(const String *s) {
    assert(((uintptr_t)s & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)s | STRING_DISCRIMINANT};
}
inline String *tag_to_string(Tag t) {
    assert(tag_is_string(t));
    return (String *)tag_to_ptr(t);
}

#define TABLE_DISCRIMINANT BYTES(7f, f5, 00, 00, 00, 00, 00, 00)
inline bool tag_is_table(Tag t) { return ((t.u & DISCRIMINANT_MASK) == TABLE_DISCRIMINANT); }
inline Tag table_to_tag(const Table *t) {
    assert(((uintptr_t)t & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)t | TABLE_DISCRIMINANT};
}
inline Table *tag_to_table(Tag t) {
    assert(tag_is_table(t));
    return (Table *)tag_to_ptr(t);
}

#define LIST_DISCRIMINANT BYTES(7f, f6, 00, 00, 00, 00, 00, 00)
inline bool tag_is_list(Tag t) { return ((t.u & DISCRIMINANT_MASK) == LIST_DISCRIMINANT); }
inline Tag list_to_tag(const List *l) {
    assert(((uintptr_t)l & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)l | LIST_DISCRIMINANT};
}
inline List *tag_to_list(Tag t) {
    assert(tag_is_list(t));
    return (List *)tag_to_ptr(t);
}

#define I64_DISCRIMINANT BYTES(7f, f7, 00, 00, 00, 00, 00, 00)
inline bool tag_is_i64(Tag t) { return ((t.u & DISCRIMINANT_MASK) == I64_DISCRIMINANT); }
inline Tag i64_to_tag(const int64_t *i) {
    assert(((uintptr_t)i & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)i | I64_DISCRIMINANT};
}
inline int64_t *tag_to_i64(Tag t) {
    assert(tag_is_i64(t));
    return (int64_t *)tag_to_ptr(t);
}
Tag i64_new(int64_t);

#define ERROR_DISCRIMINANT BYTES(7f, fc, 00, 00, 00, 00, 00, 00)
inline bool tag_is_error(Tag t) { return ((t.u & DISCRIMINANT_MASK) == ERROR_DISCRIMINANT); }
inline Tag error_to_tag(const Tag *t) {
    assert(((uintptr_t)t & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)t | ERROR_DISCRIMINANT};
}
inline Tag *tag_to_error(Tag t) {
    assert(tag_is_error(t));
    return (Tag *)tag_to_ptr(t);
}

#define SLICE_DISCRIMINANT BYTES(7f, fd, 00, 00, 00, 00, 00, 00)
inline bool tag_is_slice(Tag t) { return ((t.u & DISCRIMINANT_MASK) == SLICE_DISCRIMINANT); }
inline Tag slice_to_tag(const Slice *s) {
    assert(((uintptr_t)s & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)s | SLICE_DISCRIMINANT};
}
inline Slice *tag_to_slice(Tag t) {
    assert(tag_is_slice(t));
    return (Slice *)tag_to_ptr(t);
}

#define FUN_DISCRIMANANT BYTES(7f, fe, 00, 00, 00, 00, 00, 00)
inline bool tag_is_fun(Tag t) { return ((t.u & DISCRIMINANT_MASK) == FUN_DISCRIMANANT); }
inline Tag fun_to_tag(const Fun *f) {
    assert(((uintptr_t)f & DISCRIMINANT_MASK) == 0);
    return (Tag){.u = (uintptr_t)f | FUN_DISCRIMANANT};
}
inline Fun *tag_to_fun(Tag t) {
    assert(tag_is_fun(t));
    return (Fun *)tag_to_ptr(t);
}

// There is one pointer tags left:
// 01111111|11111111|........|........|........|........|........|.......o

// Other than the pointer tags, we define a few data tags which embed their values directly in the
// tag. These tags have the most significant discriminant bit (the sign bit) set to 1.
//
// Dobules are also considered data tags, but they don't match the TAGGED_MASK.

// I49 is a sign-maginute representation of small integers
#define I49_DISCRIMINANT BYTES(ff, f4, 00, 00, 00, 00, 00, 00)
#define I49_SIGN BYTES(00, 01, 00, 00, 00, 00, 00, 00)
#define I49_MAX INT64_C(0xffffffffffff)
#define I49_MIN (-I49_MAX)
inline bool tag_is_i49(Tag t) {
    // the least significant bit of the mask is used for sign
    return ((t.u & (DISCRIMINANT_MASK & ~I49_SIGN)) == I49_DISCRIMINANT);
}

inline Tag i49_to_tag(int64_t i) {
    assert((I49_MIN <= i && i <= I49_MAX) && "i49 range");
    if (i < 0) {
        return (Tag){.u = (uint64_t)(-i) | I49_SIGN | I49_DISCRIMINANT};
    }
    return (Tag){.u = (uint64_t)i | I49_DISCRIMINANT};
}

inline int64_t tag_to_i49(Tag t) {
    assert(tag_is_i49(t));
    uint64_t sign = t.u & I49_SIGN;
    int64_t i = (int64_t)(t.u & ~DISCRIMINANT_MASK);
    return sign ? -i : i;
}

inline Tag i49_negate(Tag t) {
    assert(tag_is_i49(t));
    return (Tag){.u = t.u ^ I49_SIGN};
}

inline Tag int_to_tag(int64_t i) {
    if (I49_MIN <= i && i <= I49_MAX) {
        return i49_to_tag(i);
    }
    return i64_new(i);
}

// BYTES(ff, f5, 00, 00, 00, 00, 00, 00) is used by i49

#define SYMBOL_DISCRIMINANT BYTES(ff, f6, 00, 00, 00, 00, 00, 00)
inline bool tag_is_symbol(Tag t) { return ((t.u & DISCRIMINANT_MASK) == SYMBOL_DISCRIMINANT); }

typedef enum { SYM_FALSE, SYM_TRUE, SYM_NIL, SYM_OK, SYM__COUNT } Symbol;

#ifndef NDEBUG
static_assert(0xfff6000000000000 == SYMBOL_DISCRIMINANT, "symbol constant changed");
#endif

#define TAG_FALSE ((Tag){.u = 0xfff6000000000000 | SYM_FALSE})
#define TAG_TRUE ((Tag){.u = (0xfff6000000000000) | SYM_TRUE})
#define TAG_NIL ((Tag){.u = (0xfff6000000000000) | SYM_NIL})
#define TAG_OK ((Tag){.u = (0xfff6000000000000) | SYM_OK})

#define USER_SYMBOL(x) ((Tag){.u = (0xfff5000000000000 | ((x) + SYM__COUNT))})

inline Symbol tag_to_symbol(Tag t) {
    assert(tag_is_symbol(t));
    uint64_t symbol = (t.u & ~DISCRIMINANT_MASK);
    assert(symbol < INT_MAX);
    return symbol;
}

inline bool tag_is_double(Tag t) { return (t.u & TAGGED_MASK) != TAGGED_MASK; }
inline double tag_to_double(Tag t) { return t.d; }
inline Tag double_to_tag(double d) { return (Tag){.d = d}; }

// There are five data tags left:
// 11111111|11110111|........|........|........|........|........|........
// 11111111|11111100|........|........|........|........|........|........
// 11111111|11111101|........|........|........|........|........|........
// 11111111|11111110|........|........|........|........|........|........
// 11111111|11111111|........|........|........|........|........|........

typedef enum { // careful, this order isn't trivial to figure out
    TYPE_STRING = 0,
    TYPE_TABLE,
    TYPE_LIST,
    TYPE_I64,
    TYPE_I49P,
    TYPE_I49N,
    TYPE_SYMBOL,
    TYPE_ERROR = 8,
    TYPE_SLICE,
    TYPE_FUN,
    TYPE_DOUBLE, // this comes last
} TagType;

// tag_type() returns the type discriminant
inline TagType tag_type(Tag t) {
    if ((t.u & TAGGED_MASK) != TAGGED_MASK) {
        return TYPE_DOUBLE;
    }
    // convert the type discriminant to a TagType
    uint64_t a = (t.u & BYTES(00, 0b, 00, 00, 00, 00, 00, 00)) >> 48;
    uint64_t b = (t.u & BYTES(80, 00, 00, 00, 00, 00, 00, 00)) >> 61;
    return (TagType)(a | b);
}

extern const char *tag_type_names[];
inline const char *tag_type_str(TagType tt) { return tag_type_names[tt]; }

void tag_free_ptr(Tag);
inline void tag_free(Tag t) {
    if (!tag_is_own(t)) {
        return; // only free owned pointers
    }
    tag_free_ptr(t);
}
void tag_printf(FILE *, Tag);
inline void tag_print(Tag t) { tag_printf(stdout, t); }
void tag_reprf(FILE *, Tag);
inline void tag_repr(Tag t) { tag_reprf(stdout, t); }
size_t tag_hash(Tag);

bool tag_eq(Tag, Tag);

bool tag_is_true(Tag);
inline Tag tag_to_bool(Tag t) {
    Tag result = tag_is_true(t) ? TAG_TRUE : TAG_FALSE;
    tag_free(t);
    return result;
}

inline bool as_int(Tag src, int64_t *dst) {
    if (tag_is_i49(src)) {
        *dst = tag_to_i49(src);
        return true;
    } else if (tag_is_i64(src)) {
        *dst = *tag_to_i64(src);
        return true;
    }
    return false;
}

// Binary math
Tag tag_add(Tag, Tag);
Tag tag_mul(Tag, Tag);
Tag tag_div(Tag, Tag);
Tag tag_mod(Tag, Tag);
Tag tag_less(Tag, Tag);
Tag tag_greater(Tag, Tag);

Tag tag_negate(Tag);

#undef BYTES
#undef TAGGED_MASK
#undef SIGN_FLAG
#undef PTR_MASK
#undef DISCRIMINANT_MASK
#undef STRING_DISCRIMINANT
#undef TABLE_DISCRIMINANT
#undef LIST_DISCRIMINANT
#undef I64_DISCRIMINANT
#undef ERROR_DISCRIMINANT
#undef SLICE_DISCRIMINANT
#undef FUN_DISCRIMANANT
#undef I49_DISCRIMINANT
#undef I49_SIGN
#undef SYMBOL_DISCRIMINANT

#endif
