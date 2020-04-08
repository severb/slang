#include "array.h" // arraylist_*
#include "mem.h"   // mem_allocation_summary

#include <assert.h> // assert
#include <stdio.h>  // printf
#include <stdlib.h> // EXIT_SUCCESS

arraylist_declare(int);
arraylist_define(int);

int main(void) {

  ArrayList(int) l = {0};
  arraylist_append(int)(&l, 10);
  arraylist_append(int)(&l, 20);
  arraylist_append(int)(&l, 30);
  printf("%d %d %d\n", arraylist_get(int)(&l, 0), arraylist_get(int)(&l, 1),
         arraylist_get(int)(&l, 2));
  printf("len: %zu\n", arraylist_len(int)(&l));
  arraylist_free(int)(&l);

#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
