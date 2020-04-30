#ifndef SLANG_TEST_UTIL_H
#define SLANG_TEST_UTIL_H

#include "types/str.h" // string_new
#include "types/tag.h" // Tag, USER_SYMBOL

#include <stdlib.h> // size_t, RAND_MAX

#define SENTINEL USER_SYMBOL(77)

static char const charset[] = "0123456789"
                              "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static inline int randint(int limit) {
  return (double)rand() / RAND_MAX * limit;
}

static inline Tag randstr(size_t size) {
  char buffer[size];
  for (size_t i = 0; i < size; i++) {
    buffer[i] = charset[randint(sizeof(charset) - 1)];
  }
  return string_to_tag(string_new(buffer, size));
}

#endif
