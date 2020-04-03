#ifndef clox_list_h
#define clox_list_h

#include "listgen.h" // LIST_TYPE, LIST_DEF
#include "val.h"     // Val

LIST_TYPE(List, Val)
LIST_DECL(List, list, Val,)

#endif
