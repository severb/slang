#include "mem.h"   // mem_allocation_summary
#include "types.h" // SLICE
#include "val.h"   // Val, val_*

#include <stdlib.h> // EXIT_SUCCESS

int main(int argc, char *argv[]) {
  Val v = val_ptr2ref(val_ptr4slice(&SLICE(hello world !)));
  val_print(v);
  printf("\n");
  val_free(v);
#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
