#include "test/util.c" // randint, randstr, sentinel

#include "mem.h"   // mem_resize_array
#include "types.h" // List, list_append, list_free, list_get
#include "val.h"   // val_data4pair, val_ptr2ref
                   // Table, table_set, table_free, table_summary
                   // collision_summary, collision_reset

#include <stdio.h>  // fprintf, stderr
#include <stdlib.h> // size_t, EXIT_SUCCESS
#include <time.h>   // clock(), clock_t

#define KEYSPACE 10000
#define STRSIZE 16
#define CYCLES 10000000

int main(int argc, const char *argv[]) {
  srand(1337);
  Table tbench = (Table){0};
  clock_t start = clock();
  for (unsigned int i = 0; i < CYCLES; i++) {
    int n = randint(KEYSPACE);
    Val key = val_data4pair(0, n);
    Val val = val_data4upair(1, i);
    table_set(&tbench, key, val);
  }
  clock_t duration = clock() - start;

  fprintf(stderr, "pairs\n");
  fprintf(stderr, "cap:%8lu real_len:%8lu duration:%8lu\n", tbench.cap,
          tbench.real_len, duration);
#ifdef CLOX_DEBUG
  collision_summary();
  collision_reset();
  table_summary(&tbench);
#endif
  table_free(&tbench);

  List strings = {0};
  for (size_t i = 0; i < KEYSPACE; i++) {
    list_append(&strings, randstr(STRSIZE));
  }

  tbench = (Table){0};
  srand(1337);
  start = clock();
  for (unsigned int i = 0; i < CYCLES; i++) {
    int n = randint(KEYSPACE);
    Val kv = val_ptr2ref(list_get(&strings, n, sentinel));
    table_set(&tbench, kv, kv);
  }
  duration = clock() - start;

  fprintf(stderr, "slices\n");
  fprintf(stderr, "cap:%8lu real_len:%8lu duration:%8lu\n", tbench.cap,
          tbench.real_len, duration);
#ifdef CLOX_DEBUG
  collision_summary();
  collision_reset();
  table_summary(&tbench);
#endif
  table_free(&tbench);
  list_free(&strings);

#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
