#include "err.h"
#include "list.h"
#include "mem.h"
#include "scanner.h"
#include "str.h"
#include "table.h"
#include "val.h"

#include <stdint.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  List l;
  list_init(&l);
  list_append(&l, NIL);
  list_append(&l, TRUE);
  list_append(&l, FALSE);

  Val err_test = ERROR(test123);
  list_append(&l, err_test);

  list_append(&l, list_ref(&l));

  int64_t x = 140442770411266981;

  list_append(&l, int_ref(&x));

  list_append(&l, pair(-10, -20));
  list_append(&l, pair(10, 20));
  list_append(&l, upair(0xffff, 0xffffffff));

  Slice s = SLICE(test slice);
  // list_append(&l, slice_own(&s));
  list_append(&l, slice_ref(&s));

  String *string = STRING(this is fun);
  list_append(&l, string_own(string));
  list_append(&l, string_ref(string));

  for (size_t i = 0; i < l.len; i++) {
    val_print_repr(l.vals[i]);
    printf("\n");
  }

  printf("pop: ");
  val_print_repr(list_pop(&l));
  printf("\n");

  list_destroy(&l);

  printf("-- TABLE --\n");

  Table t;
  table_init(&t);

  table_set(&t, pair(10, 20), pair(20, 30));
  printf("1\n");
  table_set(&t, NIL, NIL);
  printf("2\n");
  table_set(&t, TRUE, TRUE);
  printf("3\n");
  table_set(&t, FALSE, FALSE);
  printf("4\n");
  table_set(&t, slice_ref(&s), slice_ref(&s));
  printf("5\n");
  int64_t tx = 10000;
  table_set(&t, int_ref(&tx), int_ref(&tx));
  printf("6\n");
  int64_t ty = 10001;
  table_set(&t, int_ref(&ty), int_ref(&ty));
  printf("7\n");
  table_set(&t, err_test, err_test);
  printf("8\n");
  printf("%zu/%zu\n", t.len, t.cap);

  TableIter i;
  tableiter_init(&i, &t);
  Entry *entry;
  while ((entry = tableiter_next(&i))) {
    val_print_repr(entry->key);
    printf(": ");
    val_print_repr(entry->val);
    printf("\n");
  }

  table_destroy(&t);

  Scanner scan;
  scanner_init(
      &scan, "test for 1 != 2 while true\n20;//this is a comment\n'a string';");
  scanner_print(&scan);
}
