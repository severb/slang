#ifndef slang_str_h
#define slang_str_h

#include "mem.h" // mem_*

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct String {
    size_t len;
    size_t hash;
    char c[];
} String;

typedef struct Slice {
    size_t len;
    size_t hash;
    const char *c;
} Slice; // Slices don't own char *c

#define SLICE(s)                                                                                   \
    (Slice) { .c = (s), .len = (sizeof(s) - 1) }

inline String *string_new(const char *c, size_t len) {
    String *s = mem_allocate_flex(sizeof(*s), sizeof(s->c[0]), len);
    s->len = len;
    s->hash = 0;
    memcpy(s->c, c, len);
    return s;
}

inline void string_free(String *s) { mem_free_flex(s, sizeof(*s), sizeof(s->c[0]), s->len); }

inline Slice slice(const char *start, const char *end) {
    assert(start <= end);
    return (Slice){.len = end - start, .c = start};
}
inline void slice_free(Slice *s) { mem_free(s, sizeof(*s)); }

// TODO: get rid of the int cast
#define STR_PRINTF fprintf(f, "%.*s", (int)s->len, s->c)
#define STR_REPRF                                                                                  \
    putc('"', f);                                                                                  \
    STR_PRINTF;                                                                                    \
    putc('"', f)

inline void string_printf(FILE *f, const String *s) { STR_PRINTF; }
inline void slice_printf(FILE *f, const Slice *s) { STR_PRINTF; }
inline void string_reprf(FILE *f, const String *s) { STR_REPRF; }
inline void slice_reprf(FILE *f, const Slice *s) {
    STR_REPRF;
#ifdef SLANG_DEBUG
    putc('S', f);
#endif
}

#undef STR_REPRF
#undef STR_PRINTF

inline void string_print(const String *s) { string_printf(stdout, s); }
inline void slice_print(const Slice *s) { slice_printf(stdout, s); }
inline void string_repr(const String *s) { string_reprf(stdout, s); }
inline void slice_repr(const Slice *s) { slice_reprf(stdout, s); }

inline size_t string_len(const String *s) { return s->len; }
inline size_t slice_len(const Slice *s) { return s->len; }

#define STR_HASH return s->hash ? s->hash : (s->hash = str_hash(s->c, s->len))
size_t str_hash(const char *, size_t len);
inline size_t string_hash(String *s) { STR_HASH; }
inline size_t slice_hash(Slice *s) { STR_HASH; }
#undef STR_HASH

#define STR_EQ_STR                                                                                 \
    if (a->len != b->len) {                                                                        \
        return false;                                                                              \
    }                                                                                              \
    if (a->hash != 0 && b->hash != 0 && a->hash != b->hash) {                                      \
        return false;                                                                              \
    }                                                                                              \
    return memcmp(a->c, b->c, a->len) == 0
inline bool string_eq_string(const String *a, const String *b) { STR_EQ_STR; }
inline bool string_eq_slice(const String *a, const Slice *b) { STR_EQ_STR; }
inline bool slice_eq_slice(const Slice *a, const Slice *b) { STR_EQ_STR; }
inline bool slice_eq_string(const Slice *a, const String *b) { return string_eq_slice(b, a); }
#undef STR_EQ_STR

int str_cmp(const char *, size_t, const char *, size_t);
String *string_append(String *, const char *, size_t);
String *str_concat(const char *, size_t, const char *, size_t);

#endif
