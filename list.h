#ifndef clox_list_h
#define clox_list_h

#include "listgen.h" // LIST_DEF
#include "val.h"     // Val, ref

#include <stddef.h>  // size_t

LIST_DECL(val, Val)

typedef List_val List;

#define list_init(l) list_val_init(l)
#define list_destroy(l) list_val_destroy(l)
#define list_grow(l) list_val_grow(l)
#define list_seal(l) list_val_seal(l)
#define list_append(l, v) list_val_append(l, v)
#define list_get(l, i) ref(list_val_get(l, i))
#define list_pop(l) list_val_pop(l)

#endif
