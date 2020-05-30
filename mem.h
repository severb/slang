#ifndef slang_mem_h
#define slang_mem_h

#include <assert.h> // assert
#include <stddef.h> // size_t
#include <stdint.h> // SIZE_MAX, uint64_t

#include "safemath.h"

void *mem_reallocate(void *, size_t old_size, size_t new_size);
void mem_error(const char *);

inline void *mem_allocate(size_t item_size) { return mem_reallocate(0, 0, item_size); }

inline void mem_free(void *p, size_t item_size) { mem_reallocate(p, item_size, 0); }

inline void *mem_resize_array(void *p, size_t item_size, size_t old_len, size_t new_len) {
    size_t old;
    if (size_t_mul_over(old_len, item_size, &old)) {
        mem_error("old size overflows on array resize");
        return 0;
    }
    size_t new;
    if (size_t_mul_over(new_len, item_size, &new)) {
        mem_error("new size overflows on array resize");
        return 0;
    }
    return mem_reallocate(p, old, new);
}

inline void mem_free_array(void *p, size_t item_size, size_t old_len) {
    size_t old;
    if (size_t_mul_over(old_len, item_size, &old)) {
        mem_error("old size overflows on array free");
        // if mem_error doesn't abort(), continue with old=0
        mem_reallocate(p, 0, 0);
        return;
    };
    mem_reallocate(p, old, 0);
}

inline void *mem_allocate_flex(size_t type_size, size_t item_size, size_t len) {
    size_t new;
    if (size_t_mul_over(len, item_size, &new)) {
        mem_error("size overflows on flex allocate");
        return 0;
    }
    if (size_t_add_over(new, type_size, &new)) {
        mem_error("size overflows on flex allocate");
        return 0;
    }
    return mem_reallocate(0, 0, new);
}

inline void *mem_resize_flex(void *p, size_t type_size, size_t item_size, size_t old_len,
                             size_t new_len) {
    size_t old = 0;
    if (size_t_mul_over(old_len, item_size, &old)) {
        mem_error("old size overflows on flex resize");
        // if mem_error doesn't abort(), continue with old=0
        old = 0;
    } else {
        if (size_t_add_over(old, type_size, &old)) {
            mem_error("old size overflows on flex resize");
            // if mem_error doesn't abort(), continue with old=0
            old = 0;
        }
    }
    size_t new;
    if (size_t_mul_over(new_len, item_size, &new)) {
        mem_error("new size overflows on flex resize");
        return 0;
    }
    if (size_t_add_over(new, type_size, &new)) {
        mem_error("new size overflows on flex resize");
        return 0;
    }
    return mem_reallocate(p, old, new);
}

inline void mem_free_flex(void *p, size_t type_size, size_t item_size, size_t len) {
    size_t old;
    if (size_t_mul_over(len, item_size, &old)) {
        mem_error("old size overflows on flex free");
        old = 0;
        // if mem_error doesn't abort(), continue with old=0
    } else {
        if (size_t_add_over(old, type_size, &old)) {
            mem_error("old size overflows on flex free");
            old = 0;
            // if mem_error doesn't abort(), continue and leak memory
        }
    }
    mem_reallocate(p, old, 0);
}

// SLANG_DEBUG
typedef struct MemStats {
    uint64_t calls;
    size_t bytes;
} MemStats;

extern MemStats mem_stats;

#endif
