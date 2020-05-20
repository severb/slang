#include "safemath.h"

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // int64_t, uint64_t

extern inline bool i64_add_over(int64_t, int64_t, int64_t *);
extern inline bool u64_add_over(uint64_t, uint64_t, uint64_t *);
extern inline bool size_t_add_over(size_t, size_t, size_t *);
extern inline bool i64_sub_over(int64_t, int64_t, int64_t *);
extern inline bool u64_sub_over(uint64_t, uint64_t, uint64_t *);
extern inline bool size_t_sub_over(size_t, size_t, size_t *);
extern inline bool i64_mul_over(int64_t, int64_t, int64_t *);
extern inline bool u64_mul_over(uint64_t, uint64_t, uint64_t *);
extern inline bool size_t_mul_over(size_t, size_t, size_t *);
extern inline bool i64_div_over(int64_t, int64_t, int64_t *);
extern inline bool i64_mod_over(int64_t, int64_t, int64_t *);
extern inline bool i64_neg_over(int64_t, int64_t *);
