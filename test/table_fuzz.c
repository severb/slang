#include "list.h"  // List, list_*
#include "mem.h"   // mem_stats
#include "table.h" // Table, table_*
#include "tag.h"   // Tag, tag_*, TAG_NIL
#include "util.h"  // randint, randstr, SENTINEL

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    list_append(&vals, TAG_NIL);
  }

  for (size_t i = 0; i < ITER; i++) {
    size_t idx = randint(ELEMS);

    Tag key = tag_to_ref(*list_get(&keys, idx));
    if (randint(DEL) == 0) {
      table_del(&table, key);
      *list_get(&vals, idx) = TAG_NIL;
    } else {
      Tag val = i49_to_tag(i);
      table_set(&table, key, val);
      *list_get(&vals, idx) = val;
    }
  }

  for (size_t i = 0; i < ELEMS; i++) {
    Tag key = *list_get(&keys, i);
    Tag expected_val = *list_get(&vals, i);
    if (tag_type(expected_val) == TYPE_I49P) {
      Tag val;
      assert(table_get(&table, key, &val) && "key not found");
      assert(tag_eq(expected_val, val) && "val doesn't match");
    } else {
      assert(!table_get(&table, key, 0) && "unexpected key found");
    }
  }

  size_t expected_len = 0;
  for (size_t i = 0; i < ELEMS; i++) {
    Tag val = *list_get(&vals, i);
    expected_len += tag_type(val) == TYPE_I49P;
  }
  assert(table.real_len == expected_len && "length doesn't match");

#ifdef SLANG_DEBUG
  if (summary) {
    table_print_summary(&table);
  }
#endif

  table_destroy(&table);
  list_destroy(&keys);
  list_destroy(&vals);

#ifdef SLANG_DEBUG
  assert(mem_stats.bytes == 0 && "unfreed memory");
#endif
}

int main(void) {
  srand(1337);
  uint64_t i = 0;
  while (1) {
    i++;
    bool summary = i % 1000 == 0;
    testone(summary);
    if (summary) {
      printf("runs: %" PRIu64 "\n", i);
    }
  }
  return EXIT_SUCCESS;
}
