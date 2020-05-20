#ifndef slang_safemath_h
#define slang_safemath_h

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // int64_t, uint64_t

// TODO: support more extensions and a portable fallback

static inline bool i64_add_over(int64_t l, int64_t r, int64_t *result) {
  return __builtin_add_overflow(l, r, result);
}

static inline bool u64_add_over(uint64_t l, uint64_t r, uint64_t *result) {
  return __builtin_add_overflow(l, r, result);
}

static inline bool size_t_add_over(size_t l, size_t r, size_t *result) {
  return __builtin_add_overflow(l, r, result);
}

static inline bool i64_sub_over(int64_t l, int64_t r, int64_t *result) {
  return __builtin_sub_overflow(l, r, result);
}

static inline bool u64_sub_over(uint64_t l, uint64_t r, uint64_t *result) {
  return __builtin_sub_overflow(l, r, result);
}

static inline bool size_t_sub_over(size_t l, size_t r, size_t *result) {
  return __builtin_sub_overflow(l, r, result);
}

static inline bool i64_mul_over(int64_t l, int64_t r, int64_t *result) {
  return __builtin_mul_overflow(l, r, result);
}

static inline bool u64_mul_over(uint64_t l, uint64_t r, uint64_t *result) {
  return __builtin_mul_overflow(l, r, result);
}

static inline bool size_t_mul_over(size_t l, size_t r, size_t *result) {
  return __builtin_mul_overflow(l, r, result);
}

// TODO add modulo

#endif
