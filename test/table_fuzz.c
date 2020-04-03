#include "test/util.h" // randint, randstr, SENTINEL

#include "mem.h"   // mem_allocation_summary

#include "types.h" // string_new, table_summary
                   // List, list_append, list_get, list_free
                   // Table, table_set, table_get, table_del, table_free

#include "val.h"   // Val, val_ptr4string, USR_SYMBOL, val_ptr2ref, val_eq
                   // VAL_NIL, val_data4upair, val_type, VAL_PAIR

#include <stdbool.h> // bool
#include <stdint.h>  // uint64_t
#include <stdlib.h>  // EXIT_SUCCESS, rand, RAND_MAX, size_t

#define STRSIZE 16
#define ELEMS 100
#define ITER 10000
#define DEL 3

void testone(bool summary) {
  List keys = {0};
  List vals = {0};
  Table table = {0};

  for (size_t i = 0; i < ELEMS; i++) {
    list_append(&keys, randstr(STRSIZE));
    list_append(&vals, VAL_NIL);
  }

  for (size_t i = 0; i < ITER; i++) {
    size_t idx = randint(ELEMS);
    Val key = val_ptr2ref(list_get(&keys, idx, SENTINEL));
    assert(!val_eq(key, SENTINEL));
    if (randint(DEL) == 0) {
      table_del(&table, key);
      list_set(&vals, idx, VAL_NIL);
    } else {
      Val val = val_data4upair(0, i);
      table_set(&table, key, val);
      list_set(&vals, idx, val);
    }
  }

  for (size_t i = 0; i < ELEMS; i++) {
    Val key = list_get(&keys, i, SENTINEL);
    assert(!val_eq(key, SENTINEL));
    Val expected_val = list_get(&vals, i, SENTINEL);
    assert(!val_eq(expected_val, SENTINEL));
    Val val = table_get(&table, key, VAL_NIL);
    assert(val_eq(expected_val, val));
  }

  size_t expected_len = 0;
  for (size_t i = 0; i < ELEMS; i++) {
    Val val = list_get(&vals, i, SENTINEL);
    assert(!val_eq(val, SENTINEL));
    expected_len += val_type(val) == VAL_PAIR;
  }
  assert(table.real_len == expected_len);

  if (summary) {
#ifdef CLOX_DEBUG
    table_summary(&table);
#endif
  }

  table_free(&table);
  list_free(&keys);
  list_free(&vals);
}

int main(int arcg, char const *argv[]) {
  srand(1337);
  uint64_t i = 0;
  while (1) {
    i++;
    testone(i % 1000 == 0);
    if (i % 1000 == 0) {
      printf("runs: %zu\n", i);
#ifdef CLOX_DEBUG
      mem_allocation_summary();
#endif
    }
  }
  return EXIT_SUCCESS;
}
