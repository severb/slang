#ifndef CLOX_TEST_UTIL_H
#define CLOX_TEST_UTIL_H

#include "val.h" // Val, USR_SYMBOL

#include <stdlib.h> // size_t

int randint(int);
Val randstr(size_t);
static const Val sentinel = USR_SYMBOL(77);

#endif
