#ifndef slang_list_h
#define slang_list_h

#include "dynarray.h" // dynarray_*
#include "tag.h"      // Tag

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

dynarray_declare(Tag);

typedef struct List {
    DynamicArray(Tag) array;
} List;

bool list_eq(const List *, const List *);
void list_destroy(List *);
void list_free(List *);
void list_printf(FILE *, const List *);
inline void list_print(const List *l) { list_printf(stdout, l); }

inline size_t list_len(const List *l) { return dynarray_len(Tag)(&l->array); }

inline Tag *list_get(const List *l, size_t idx) { return dynarray_get(Tag)(&l->array, idx); }

inline Tag list_pop(List *l) {
    size_t len = list_len(l);
    assert((len > 0) && "pop on empty list");
    Tag result = *list_get(l, len - 1);
    dynarray_trunc(Tag)(&l->array, len - 1);
    return result;
}

inline void list_trunc(List *l, size_t len) {
    dynarray_trunc(Tag)(&l->array, len);
}

inline Tag *list_last(const List *l) {
    size_t len = list_len(l);
    assert(len > 0 && "last on empty list");
    return list_get(l, len - 1);
}

inline void list_append(List *l, Tag t) { dynarray_append(Tag)(&l->array, &t); }

inline bool list_find_from(const List *l, Tag needle, size_t *idx) {
    for (size_t i = idx ? *idx : 0; i < list_len(l); i++) {
        Tag ith_tag = *list_get(l, i);
        if (tag_eq(ith_tag, needle)) {
            if (idx) {
                *idx = i;
            }
            return true;
        }
    }
    return false;
}

#endif
