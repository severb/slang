#include "test/util.h"

#include "val.h"   // Val, val_ptr4string
#include "types.h" // string_new

#include <stdlib.h> // rand(), RAND_MAX

static char const charset[] = "0123456789"
                              "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int randint(int limit) { return (double)rand() / RAND_MAX * limit; }

Val randstr(size_t size) {
  char buffer[size];
  for (size_t i = 0; i < size; i++) {
    buffer[i] = charset[randint(sizeof(charset) - 1)];
  }
  return val_ptr4string(string_new(buffer, size));
}
