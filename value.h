#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include "object.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj *obj;
  } as;
} Value;

#define VAL_AS_BOOL(value) ((value).as.boolean)
#define VAL_AS_NUMBER(value) ((value).as.number)
#define VAL_AS_OBJ(value) ((value).as.obj)

#define VAL_LIT_BOOL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define VAL_LIT_NIL() ((Value){VAL_NIL, {.number = 0}})
#define VAL_LIT_NUMBER(value) ((Value){VAL_NUMBER, {.number = value}})
#define VAL_LIT_OBJ(value) ((Value){VAL_OBJ, {.obj = (Obj *)value}})

#define VAL_IS_BOOL(value) ((value).type == VAL_BOOL)
#define VAL_IS_NIL(value) ((value).type == VAL_NIL)
#define VAL_IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define VAL_IS_OBJ(value) ((value).type == VAL_OBJ)

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void valueArrayInit(ValueArray *array);
void valueArrayWrite(ValueArray *array, Value value);
void valueArrayFree(ValueArray *array);

bool valuesEqual(Value a, Value b);
void valuePrint(Value value);

#endif
