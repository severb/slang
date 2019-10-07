#ifndef clox_memory_h
#define clox_memory_h

#include <stddef.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

#define GROW_ARRAY(pointer, type, oldCount, newCount)                          \
  (type *)reallocate(pointer, sizeof(type) * (oldCount),                       \
                     sizeof(type) * (newCount))

#define FREE_ARRAY(pointer, type, oldCount)                                    \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(type) (type *)reallocate(NULL, 0, sizeof(type))

#define FREE(pointer, type) reallocate(pointer, sizeof(type), 0)

#define ALLOCATE_FLEX(type, arrayType, length)                                 \
  (type *)reallocate(NULL, 0, sizeof(type) + sizeof(arrayType) * length)

#define FREE_FLEX(pointer, type, arrayType, length)                            \
  reallocate(pointer, sizeof(type) + sizeof(arrayType) * length, 0)

void *reallocate(void *previous, size_t oldSize, size_t newSize);

#endif
