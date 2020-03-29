#include "mem.h"
#include "types.h"
#include "val.h"

#include <stdint.h> // int64_t
#include <stdio.h>  // printf

#define MYSYMBOL USR_SYMBOL(0)

int main(int argc, char *argv[]) {
  int64_t i = -77;
  Val v_i = val_ptr2ref(val_ptr4int64(&i));

  assert(val_is_ptr(v_i));
  assert(&i == val_ptr2ptr(v_i));

  val_print(v_i);
  printf("\n");

  int64_t *ip = mem_allocate(sizeof(int64_t));
  *ip = -223;
  Val v_ip = val_ptr4int64(ip);
  val_print(val_ptr2ref(v_ip));
  printf("\n");

  List l_a = (List){0};
  List l_b = (List){0};

  list_append(&l_a, val_ptr2ref(val_ptr4slice(&SLICE(abc))));
  list_append(&l_a, val_ptr2ref(val_ptr4slice(&SLICE(def))));
  list_append(&l_a, val_ptr2ref(val_ptr4slice(&SLICE(ghi))));
  list_append(&l_a, v_ip);

  Val v_l_a = val_ptr2ref(val_ptr4list(&l_a));

  list_append(&l_b, v_l_a);
  list_append(&l_b, v_l_a);
  list_append(&l_b, val_ptr2ref(val_ptr4slice(&SLICE(test))));

  val_print(val_ptr2ref(val_ptr4list(&l_b)));
  printf("\n");

  Slice slice = (Slice){.c = "this is a test\n", .len = 15};

  Val v_s = val_ptr2ref(val_ptr4slice(&slice));
  val_print(v_s);

  List *l_c = mem_allocate(sizeof(List));
  *l_c = (List){0};
  list_append(l_c, val_ptr2ref(val_ptr4slice(&SLICE(this is another test))));
  Val v_l_c = val_ptr4list(l_c);
  val_print(v_l_c);
  printf("\n");

  int64_t *ip2 = mem_allocate(sizeof(int64_t));
  *ip2 = 200;
  Val *vp_ip2 = mem_allocate(sizeof(Val));
  *vp_ip2 = val_ptr4int64(ip2);
  Val v_err = val_ptr4err(vp_ip2);
  val_print(v_err);

  val_print(val_data4pair(-2, 70));
  printf("\n");

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
  printf("slice is %i\n", val_is_true(v_s));
  Val zero_pair = val_data4upair(0, 0);
  printf("pair 0, 0 is %i\n", val_is_true(zero_pair));
  printf("err: pair 0, 0 is %i\n", val_is_true(val_ptr4err(&zero_pair)));

  printf("VAL_FALSE hash is %zu\n", val_hash(VAL_FALSE));
  printf("VAL_TRUE hash is %zu\n", val_hash(VAL_TRUE));
  printf("pair 0,0 hash is %zu\n", val_hash(zero_pair));
  printf("slice hash is %zu\n", val_hash(v_s));

  val_free(v_err);
  val_free(v_l_c);
  val_free(v_s);
  val_free(v_l_a);
  list_free(&l_b);
  list_free(&l_a);
  val_free(v_i);

  mem_allocation_summary();

  printf("%p\n", (void *)&val_data2double);
}
