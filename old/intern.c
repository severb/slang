#include "intern.h"
#include "common.h"
#include "str.h"
#include "val.h"

Intern *intern_init(Intern *i) {
  if (i == 0)
    return 0;
  table_init(&i->table);
  return i;
}

void intern_destroy(Intern *i) {
  if (i == 0)
    return;
  table_destroy(&i->table);
}

Val intern_slice(Intern *i, Slice slice) {
  Val key = VAL_LIT_SLICE(slice);
  Val val = VAL_LIT_SLICE(slice);
  return *table_setdefault(&i->table, &key, &val);
}

Val intern_str(Intern *i, Str *str) {
  Val key = VAL_LIT_STR(str);
  Val val = VAL_LIT_SLICE(str_slice(str));
  return *table_setdefault(&i->table, &key, &val);
}
