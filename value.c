#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void valueArrayInit(ValueArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->values = NULL;
}

void valueArrayWrite(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values =
        GROW_ARRAY(array->values, Value, oldCapacity, array->capacity);
  }
  array->values[array->count] = value;
  array->count++;
}

void valueArrayFree(ValueArray *array) {
  FREE_ARRAY(array->values, Value, array->capacity);
  valueArrayInit(array);
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;
  switch (a.type) {
  case VAL_BOOL:
    return VAL_AS_BOOL(a) == VAL_AS_BOOL(b);
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return VAL_AS_NUMBER(a) == VAL_AS_NUMBER(b);
  case VAL_OBJ:
    return objsEqual(VAL_AS_OBJ(a), VAL_AS_OBJ(b));
  }
  return false; // unreachable
}

void valuePrint(Value value) {
  switch (value.type) {
  case VAL_BOOL:
    printf(VAL_AS_BOOL(value) ? "true" : "false");
    break;
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_NUMBER:
    printf("%g", VAL_AS_NUMBER(value));
    break;
  case VAL_OBJ:
    objPrint(VAL_AS_OBJ(value));
    break;
  }
}
