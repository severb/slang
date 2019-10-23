#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include "str.h"

#define FOREACH_VALTYPE(VALTYPE)                                               \
  VALTYPE(VAL_ARRAY)                                                           \
  VALTYPE(VAL_BOOL)                                                            \
  VALTYPE(VAL_NIL)                                                             \
  VALTYPE(VAL_NUMBER)                                                          \
  VALTYPE(VAL_SLICE)                                                           \
  VALTYPE(VAL_STR)                                                             \
  VALTYPE(VAL_TABLE)

#define GENERATE_ENUM(ENUM) ENUM,
typedef enum { FOREACH_VALTYPE(GENERATE_ENUM) } ValType;
#undef GENERATE_ENUM

struct sTable; // forward declaration of Table
struct sArray;

typedef struct {
  ValType type;
  union {
    Slice slice;
    bool boolean;
    double number;
    Str *str;
    struct sTable *table;
    struct sArray *array;
  } as;
} Val;

#define GENERATE_IS_FUNC(ENUM)                                                 \
  static inline bool IS_##ENUM(Val v) { return v.type == ENUM; }
FOREACH_VALTYPE(GENERATE_IS_FUNC)
#undef GENERATE_ENUM

#define LIT_VAL_BOOL(b) ((Val){.type = VAL_BOOL, .as = {.boolean = (b)}})
#define LIT_VAL_NIL ((Val){.type = VAL_NIL, .as = {.boolean = 0}})
#define LIT_VAL_NUMBER(n) ((Val){.type = VAL_NUMBER, .as = {.number = (n)}})
#define LIT_VAL_STR(c) ((Val){.type = VAL_STR, .as = {.str = (c)}})
#define LIT_VAL_TABLE(t) ((Val){.type = VAL_TABLE, .as = {.table = (t)}})

void val_destroy(Val *);

void val_print(Val);
void val_print_repr(Val);
uint32_t val_hash(Val);
bool val_truthy(Val);

bool val_equals(Val, Val);

#endif
