#include "list.h"  // List, list_*
#include "mem.h"   // mem_stats
#include "table.h" // Table, table_*
#include "tag.h"   // Tag, *_tag, tag_*
#include "util.h"  // randint, randstr

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define KEYSPACE 10000
#define STRSIZE 16
#define CYCLES 10000000

int main(int argc, const char *argv[]) {
  unsigned long long keyspace = KEYSPACE;
  if (argc > 1) {
    keyspace = strtoull(argv[1], NULL, 10);
    keyspace = keyspace ? keyspace : KEYSPACE;
  }
  srand(1337);
  Table t = (Table){0};
  clock_t start = clock();
  for (unsigned int i = 0; i < CYCLES; i++) {
    int n = randint(keyspace);
    Tag key = i49_to_tag(n);
    Tag val = i49_to_tag(i);
    table_set(&t, key, val);
  }
  clock_t duration = clock() - start;

  fprintf(stderr, "pairs\n");
  fprintf(stderr, "real_len:%8zu duration:%8lu\n", table_len(&t), duration);
#ifdef SLANG_DEBUG
  TableStats s1;
  s1 = table_stats();
  fprintf(stderr, "queries:%8" PRIu64 " collisions:%8" PRIu64 "\n", s1.queries,
          s1.collisions);
#endif
  table_destroy(&t);

  List strings = {0};
  for (size_t i = 0; i < keyspace; i++) {
    list_append(&strings, randstr(STRSIZE));
  }

  t = (Table){0};
  srand(1337);
  start = clock();
  for (unsigned int i = 0; i < CYCLES; i++) {
    int n = randint(keyspace);
    Tag kv = tag_to_ref(*list_get(&strings, n));
    table_set(&t, kv, kv);
  }
  duration = clock() - start;

  fprintf(stderr, "slices\n");
  fprintf(stderr, "real_len:%8zu duration:%8lu\n", table_len(&t), duration);
#ifdef SLANG_DEBUG
  TableStats s2;
  s2 = table_stats();
  fprintf(stderr, "queries:%8" PRIu64 " collisions:%8" PRIu64 "\n",
          s2.queries - s1.queries, s2.collisions - s1.collisions);
#endif
  table_destroy(&t);
  list_destroy(&strings);

#ifdef SLANG_DEBUG
  assert(mem_stats.bytes == 0 && "unfreed memory");
#endif

  return EXIT_SUCCESS;
}
