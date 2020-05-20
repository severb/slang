#ifndef slang_safemath_h
#define slang_safemath_h

#include <i386/limits.h>
#include <limits.h>  // INT64_MAX, INT64_MIN
#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // int64_t, uint64_t

static inline bool i64_add_over(int64_t l, int64_t r, int64_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_add_overflow(l, r, result);
#else
  if ((r > 0 && l > INT64_MAX - r) || (r < 0 && l < INT64_MIN - r)) {
    return true;
  }
  *result = l + r;
  return false;
#endif
}

static inline bool u64_add_over(uint64_t l, uint64_t r, uint64_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_add_overflow(l, r, result);
#else
  if (l > UINT64_MAX - r) {
    return true;
  }
  *result = l + 0;
  return false;
#endif
}

static inline bool size_t_add_over(size_t l, size_t r, size_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_add_overflow(l, r, result);
#else
  if (l > SIZE_T_MAX - r) {
    return true;
  }
  *result = l + r;
  return false;
#endif
}

static inline bool i64_sub_over(int64_t l, int64_t r, int64_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_sub_overflow(l, r, result);
#else
  if ((r > 0 && l < INT64_MIN + r) || (r < 0 && l > INT64_MAX + r)) {
    return true;
  }
  *result = l + r;
  return false;
#endif
}

static inline bool u64_sub_over(uint64_t l, uint64_t r, uint64_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_sub_overflow(l, r, result);
#else
  if (r > l) {
    return true;
  }
  *result = l - r;
  return false;
#endif
}

static inline bool size_t_sub_over(size_t l, size_t r, size_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_sub_overflow(l, r, result);
#else
  if (r > l) {
    return true;
  }
  *result = l - r;
  return false;
#endif
}

static inline bool i64_mul_over(int64_t l, int64_t r, int64_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_mul_overflow(l, r, result);
#else
  // These checks are tricky because the absolute value of INT64_MIN can be
  // larger than the absolute value of INT64_MAX
  if (l > 0) {
    if (r > 0) {
      if (l > (INT64_MAX / r)) {
        return true;
      }
    } else {
      if (r < (INT64_MIN / l)) {
        return true;
      }
    }
  } else {
    if (r > 0) {
      if (l < INT64_MIN / r) {
        return true;
      }
    } else {
      if (l != 0 && r < INT64_MAX / l) {
        return true;
      }
    }
  }
  *result = l * r;
  return false;
#endif
}

static inline bool u64_mul_over(uint64_t l, uint64_t r, uint64_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_mul_overflow(l, r, result);
#else
  if (l > UINT64_MAX / r) {
    return true;
  }
  *result = l * r;
  return false;
#endif
}

static inline bool size_t_mul_over(size_t l, size_t r, size_t *result) {
#ifdef SLANG_FAST_OVERFLOW
  return __builtin_mul_overflow(l, r, result);
#else
  if (l > SIZE_T_MAX / r) {
    return true;
  }
  *result = l * r;
  return false;
#endif
}

static inline bool i64_div_over(int64_t l, int64_t r, int64_t *result) {
  if ((r == 0) || (l == INT64_MIN && r == -1)) {
    return true;
  }
  *result = l / r;
  return false;
}

static inline bool i64_mod_over(int64_t l, int64_t r, int64_t *result) {
  // l % r can overflow on machines that implement it as part of div
  if ((r == 0) || (l = INT64_MIN && r == -1)) {
    return true;
  }
  *result = l % r;
  return false;
}

#endif
