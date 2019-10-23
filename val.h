#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include "str.h"

#define FOREACH_VALTYPE(VALTYPE)                                               \
  VALTYPE(VAL_INVALID)                                                         \
  VALTYPE(VAL_BOOL)                                                            \
  VALTYPE(VAL_NUMBER)                                                          \
  VALTYPE(VAL_NIL)                                                             \
  VALTYPE(VAL_STR)                                                             \
  VALTYPE(VAL_TABLE)

#define GENERATE_ENUM(ENUM) ENUM,
typedef enum { FOREACH_VALTYPE(GENERATE_ENUM) } ValType;
#undef GENERATE_ENUM

struct sTable; // forward declaration of Table
typedef struct sTable Table;

typedef union {
  Slice slice;
  // OR
  struct {
    bool is_slice : 1;
    ValType type : 3;
    union {
      bool boolean;
      double number;
      Str *str;
      Table *table;
    } as;
  } val;
} Val;

#define VAL_IS_SLICE(v) ((v).val.is_slice)

#define VAL_IS_BOOL(v) (val_is((v), VAL_BOOL))
#define VAL_IS_INVALID(v) (val_is((v), VAL_INVALID))
#define VAL_IS_NUMBER(v) (val_is((v), VAL_NUMBER))
#define VAL_IS_NIL(v) (val_is((v), VAL_NIL))
#define VAL_IS_STR(v) (val_is((v), VAL_STR))
#define VAL_IS_TABLE(t) (val_is((t), VAL_TABLE))

static bool inline val_is(Val v, ValType type) {
  return (!VAL_IS_SLICE(v) && v.val.type == type);
}

#define VAL_LIT_SLICE(s) (val_slice((s)))

static Val inline val_slice(Slice s) {
  Val res = {.slice = s};
  res.val.is_slice = true;
  return res;
}

#define VAL_LIT_BOOL(b)                                                        \
  ((Val){.val = {.is_slice = false, .type = VAL_BOOL, .as = {.boolean = (b)}}})
#define VAL_LIT_INVALID(i)                                                     \
  ((Val){.val = {.is_slice = false, .type = VAL_INVALID, .as = {(i)}}})
#define VAL_LIT_NUMBER(n)                                                      \
  ((Val){.val = {.is_slice = false, .type = VAL_NUMBER, .as = {.number = (n)}}})
#define VAL_LIT_NIL                                                            \
  ((Val){.val = {.is_slice = false, .type = VAL_NIL, .as = {0}}})
#define VAL_LIT_STR(c)                                                         \
  ((Val){.val = {.is_slice = false, .type = VAL_STR, .as = {.str = (c)}}})
#define VAL_LIT_TABLE(t)                                                       \
  ((Val){.val = {.is_slice = false, .type = VAL_TABLE, .as = {.table = (t)}}})

Val *val_init(Val *);
void val_destroy(Val *);

void val_print(Val);
void val_print_repr(Val);
uint32_t val_hash(Val);
bool val_truthy(Val);

bool val_equals(Val, Val);

#endif
