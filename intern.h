#ifndef clox_intern_h
#define clox_intern_h

#include "common.h"
#include "str.h"
#include "table.h"
#include "val.h"

typedef struct {
  Table table;
} Intern;

Intern *intern_init(Intern *);
void intern_destroy(Intern *);

Val intern_str(Intern *, Str *);
Val intern_slice(Intern *, Slice);

#endif
