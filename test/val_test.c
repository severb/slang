#include "mem.h"   // mem_allocate, mem_allocation_summary
#include "types.h" // List, list_append, list_free, Slice
#include "val.h"   // Val, val_*, USR_SYMBOL

#include <assert.h> // assert
#include <stdint.h> // int64_t
#include <stdio.h>  // printf
#include <stdlib.h> // EXIT_SUCCESS

#define MYSYMBOL USR_SYMBOL(0)

int main(int argc, char *argv[]) {
  int64_t i1 = -77;
  int64_t i2 = -77;
  Val v = val_ptr2ref(val_ptr4int64(&i1));
  assert(val_is_ptr(v));
  assert(&i1 == val_ptr2ptr(v));
  assert(i1 == *val_ptr2int64(v));
  assert(val_eq(v, v));
  assert(val_eq(v, val_ptr4int64(&i2)));
  printf("int64: ");
  val_print(v);
  printf("\n");
  val_free(v);

  List l_a = (List){0};
  list_append(&l_a, val_ptr2ref(val_ptr4slice(&SLICE(1st slice))));
  list_append(&l_a, val_ptr2ref(val_ptr4slice(&SLICE(2nd slice))));
  list_append(&l_a, val_ptr2ref(val_ptr4slice(&SLICE(3rd slice))));
  list_append(&l_a, val_data4upair(0, 10));
  list_append(&l_a, val_data4upair(0, 20));
  List l_b = (List){0};
  list_append(&l_b, val_ptr2ref(val_ptr4list(&l_a)));
  list_append(&l_b, val_ptr2ref(val_ptr4list(&l_a)));
  list_append(&l_b, val_ptr2ref(val_ptr4slice(&SLICE(test))));
  val_print(val_ptr2ref(val_ptr4list(&l_b)));
  printf("\n");
  list_free(&l_b);
  list_free(&l_a);

  val_print(VAL_FALSE);
  printf("\n");
  val_print(VAL_TRUE);
  printf("\n");
  val_print(VAL_NIL);
  printf("\n");
  val_print(VAL_OK);
  printf("\n");
  val_print(MYSYMBOL);
  printf("\n");

  printf("VAL_FALSE is %i\n", val_is_true(VAL_FALSE));
  printf("VAL_NIL is %i\n", val_is_true(VAL_NIL));
  printf("VAL_TRUE is %i\n", val_is_true(VAL_TRUE));
  printf("VAL_OK is %i\n", val_is_true(VAL_OK));
  printf("double 0.0 is %i\n", val_is_true(val_data4double(0.0)));
  printf("double 0.01 is %i\n", val_is_true(val_data4double(0.01)));
  printf("non empty slice is %i\n",
         val_is_true(val_ptr4slice(&SLICE(non empty))));
  printf("empty slice is %i\n", val_is_true(val_ptr4slice(&SLICE())));
  printf("pair 0, 0 is %i\n", val_is_true(val_data4upair(0, 0)));
  v = VAL_FALSE;
  printf("err: FALSE is %i\n", val_is_true(val_ptr4err(&v)));

  printf("VAL_FALSE hash is %zu\n", val_hash(VAL_FALSE));
  printf("VAL_TRUE hash is %zu\n", val_hash(VAL_TRUE));
  printf("pair 0,0 hash is %zu\n", val_hash(val_data4pair(0, 0)));
  printf("slice hash is %zu\n", val_hash(val_ptr4slice(&SLICE(hash))));

#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif

  return EXIT_SUCCESS;
}
