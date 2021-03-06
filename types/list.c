#include "list.h"

#include "dynarray.h" // dynarray_*
#include "mem.h"      // mem_*
#include "tag.h"      // Tag, tag_*

#include <stdio.h>

dynarray_define(Tag);

bool list_eq(const List *a, const List *b) {
    if (list_len(a) != list_len(b)) {
        return false;
    }
    for (size_t i = 0; i < list_len(a); i++) {
        if (!tag_eq(*list_get(a, i), *list_get(b, i))) {
            return false;
        }
    }
    return true;
}

void list_free(List *l) {
    list_destroy(l);
    mem_free(l, sizeof(*l));
}

void list_destroy(List *l) {
    for (size_t i = 0; i < list_len(l); i++) {
        tag_free(*list_get(l, i));
    }
    dynarray_destroy(Tag)(&l->array);
}

void list_printf(FILE *f, const List *l) {
    putc('[', f);
    size_t len = list_len(l);
    for (size_t i = 0; i < len; i++) {
        tag_reprf(f, *list_get(l, i));
        if (i + 1 < len) {
            fputs(", ", f);
        }
    }
    putc(']', f);
}

extern inline size_t list_len(const List *);
extern inline Tag *list_get(const List *, size_t);
extern inline Tag list_pop(List *);
extern inline void list_trunc(List *, size_t);
extern inline Tag *list_last(const List *);
extern inline void list_append(List *, Tag);
extern inline bool list_find_from(const List *, Tag, size_t *);
extern inline void list_print(const List *);
