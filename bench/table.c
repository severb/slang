#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common.h"
#include "../mem.h"
#include "../str.h"
#include "../table.h"
#include "../val.h"

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

  for (int i = 0; i < CYCLES; i++) {
    int n = rand() % keyspace;
    Val key = VAL_LIT_NUMBER(n);
    Val val = VAL_LIT_NUMBER(i);
    table_set(&tbench, &key, &val);
  }
  clock_t duration = clock() - start;

  printf("numbers\n");
  printf("cap:%8lu len:%8lu duration:%8lu\n", tbench.cap, tbench.len, duration);

  keyspace += 12;
  char *c = GROW_ARRAY(0, char, 0, keyspace);
  char charset[] = "0123456789"
                   "abcdefghijklmnopqrstuvwxyz"
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char *dest = c;
  int i = keyspace;
  while (i-- > 0) {
    size_t index = (double)rand() / RAND_MAX * (sizeof charset - 1);
    *dest++ = charset[index];
  }

  table_destroy(&tbench);
  start = clock();

  for (int i = 0; i < CYCLES; i++) {
    Slice k_v;
    slice_init(&k_v, &c[i % (keyspace - 12)], 12);
    Val key = VAL_LIT_SLICE(k_v);
    Val val = VAL_LIT_SLICE(k_v);
    table_set(&tbench, &key, &val);
  }
  duration = clock() - start;

  printf("slices\n");
  printf("cap:%8lu len:%8lu duration:%8lu\n", tbench.cap, tbench.len, duration);

  table_destroy(&tbench);

  return EXIT_SUCCESS;
}
