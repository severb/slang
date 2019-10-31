#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mem.h"
#include "str.h"
#include "table.h"
#include "val.h"

#define CYCLES 10000000

int main(int argc, const char *argv[]) {

  size_t keyspace = 10000;
  if (argc == 2) {
    size_t k = strtoul(argv[1], 0, 0);
    keyspace = k ? k : keyspace;
  }

  Table tbench;
  table_init(&tbench);

  srand(1337);

  clock_t start = clock();
  for (uint32_t i = 0; i < CYCLES; i++) {
    int n = rand() % keyspace;
    Val key = pair(0, n);
    Val val = upair(1, i);
    table_set(&tbench, key, val);
  }
  clock_t duration = clock() - start;

  fprintf(stderr, "numbers\n");
  fprintf(stderr, "cap:%8lu len:%8lu duration:%8lu\n", tbench.cap, tbench.len,
          duration);

  keyspace += 12;
  char *c = GROW_ARRAY(0, char, 0, keyspace);
  char charset[] = "0123456789"
                   "abcdefghijklmnopqrstuvwxyz"
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char *dest = c;
  int j = keyspace;
  while (j-- > 0) {
    size_t index = (double)rand() / RAND_MAX * (sizeof charset - 1);
    *dest++ = charset[index];
  }

  table_destroy(&tbench);

  table_init(&tbench);
  Slice *slices = GROW_ARRAY(0, Slice, 0, keyspace);
  for (uint32_t i = 0; i < keyspace; i++) {
    slices[i] = slice(&c[i % (keyspace - 1)], 12);
  }
  start = clock();

  for (int i = 0; i < CYCLES; i++) {
    uint32_t n = i % keyspace;
    Val kv = slice_ref(&slices[n]);
    table_set(&tbench, kv, kv);
  }
  duration = clock() - start;

  fprintf(stderr, "slices\n");
  fprintf(stderr, "cap:%8lu len:%8lu duration:%8lu\n", tbench.cap, tbench.len,
          duration);

  table_destroy(&tbench);

  return EXIT_SUCCESS;
}
