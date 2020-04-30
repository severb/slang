#include "mem.h"         // mem_stats
#include "test/util.h"   // randint, randstr, SENTINEL
#include "types/list.h"  // List, list_*
#include "types/table.h" // Table, table_
#include "types/tag.h"   // Tag, tag_eq, tag_type, TAG_NIL

#include <stdio.h>   // printf
#include <inttypes.h> // PRId*
#include <stdbool.h>  // bool
#include <stdint.h>   // uint64_t
#include <stdlib.h>   // EXIT_SUCCESS, rand, RAND_MAX, size_t

#define STRSIZE 16
#define ELEMS 100
#define ITER 10000
#define DEL 3

void testone(void) {
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
      Tag val = upair_to_tag(0, i);
      table_set(&table, key, val);
      *list_get(&vals, idx) = val;
    }
  }

  for (size_t i = 0; i < ELEMS; i++) {
    Tag key = *list_get(&keys, i);
    Tag expected_val = *list_get(&vals, i);
    if (tag_type(expected_val) == TYPE_PAIR) {
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
    expected_len += tag_type(val) == TYPE_PAIR;
  }
  assert(table.real_len == expected_len && "length doesn't match");

  table_free(&table);
  list_free(&keys);
  list_free(&vals);

#ifdef SLANG_DEBUG
  assert(mem_stats().bytes == 0 && "unfreed memory");
#endif
}

int main(int arcg, char const *argv[]) {
  srand(1337);
  uint64_t i = 0;
  while (1) {
    i++;
    testone();
    if (i % 1000 == 0) {
      printf("runs: %" PRIu64 "\n", i);
    }
  }
  return EXIT_SUCCESS;
}
