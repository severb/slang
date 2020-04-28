#include "types/list.h"

#include "types/tag.h" // tag_free

#include <stdio.h> // putchar, puts

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
  for (size_t i = 0; i < list_len(l); i++) {
    tag_free(*list_get(l, i));
  }
  dynarray_free(Tag)(&l->array);
}

void list_print(const List *l) {
  putchar('[');
  size_t len = list_len(l);
  for (size_t i = 0; i < len; i++) {
    tag_repr(*list_get(l, i));
    if (i + 1 < len) {
      puts(", ");
    }
  }
  putchar(']');
}

extern inline size_t list_len(const List *);
extern inline Tag *list_get(const List *, size_t);
extern inline bool list_getbool(const List *, size_t, Tag *);

extern inline Tag list_pop(List *);
extern inline bool list_popbool(List *, Tag *);

extern inline Tag *list_last(const List *);
extern inline bool list_lastbool(const List *, Tag *);

extern inline size_t list_append(List *, Tag);
extern inline bool list_find(const List *, Tag, size_t *);
