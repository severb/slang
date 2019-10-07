#include <stdio.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

typedef struct {
  char *c;
  int len;
} str;

static str toStr(Obj *obj) {
  str result;
  switch (obj->type) {
  case OBJ_STRING_STATIC: {
    ObjStringStatic *objStrStatic = OBJ_AS_OBJSTRINGSTATIC(obj);
    result.c = objStrStatic->chars;
    result.len = objStrStatic->length;
    break;
  }
  case OBJ_STRING: {
    ObjString *objStr = OBJ_AS_OBJSTRING(obj);
    result.c = objStr->chars;
    result.len = objStr->length;
    break;
  }
  }
  return result;
}

static Obj *_objNew(Obj *obj, ObjType type) {
  obj->type = type;
  return obj;
}

#define objNew(type, objType)                                                  \
  ((type *)_objNew((Obj *)ALLOCATE(type), (objType)))

#define objNewFlex(type, arrayType, length, objType)                           \
  ((type *)_objNew((Obj *)ALLOCATE_FLEX(type, arrayType, length), (objType)))

ObjStringStatic *objStringStaticNew(const char *str, int length) {
  ObjStringStatic *obj = objNew(ObjStringStatic, OBJ_STRING_STATIC);
  obj->chars = (char *)str;
  obj->hash = 0;
  obj->length = length;
  return obj;
}

ObjString *objStringNew(const char *str, int length) {
  ObjString *obj = objNewFlex(ObjString, char, length, OBJ_STRING);
  memcpy(obj->chars, str, length);
  obj->hash = 0;
  obj->length = length;
  return obj;
}

ObjString *objStringsConcat(Obj *a, Obj *b) {
  str aStr = toStr(a), bStr = toStr(b);
  int length = aStr.len + bStr.len;
  ObjString *obj = objNewFlex(ObjString, char, length, OBJ_STRING);
  memcpy(obj->chars, aStr.c, aStr.len);
  memcpy(obj->chars + aStr.len, bStr.c, bStr.len);
  obj->hash = 0;
  obj->length = length;
  return obj;
}

void objPrint(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING: // fallthrough
  case OBJ_STRING_STATIC: {
    str s = toStr(obj);
    printf("%.*s", s.len, s.c);
    break;
  }
  default:
    return; // Unreachable
  }
}

void objPrintRepr(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING:
    printf("<OBJ_STRING (%d) ", OBJ_AS_OBJSTRING(obj)->length);
    break;
  case OBJ_STRING_STATIC:
    printf("<OBJ_STRING_STATIC (%d) ", OBJ_AS_OBJSTRINGSTATIC(obj)->length);
    break;
  }
  objPrint(obj);
  printf(">");
}

bool objsEqual(Obj *a, Obj *b) {
  if (!(OBJ_IS_ANYSTRING(a) && OBJ_IS_ANYSTRING(b)))
    return false;

  if (a == b)
    return true;

  if (objHash(a) != objHash(b))
    return false;

  str aStr = toStr(a), bStr = toStr(b);

  if (aStr.len != bStr.len)
    return false;

  return memcmp(aStr.c, bStr.c, aStr.len) == 0;
}

void objFree(Obj *obj) {
#ifdef DEBUG_PRINT_FREE
  printf("-- free obj -- ");
  objPrintRepr(obj);
  printf("\n");
#endif
  switch (obj->type) {
  case OBJ_STRING: {
    FREE_FLEX(obj, ObjString, char, OBJ_AS_OBJSTRING(obj)->length);
    break;
  case OBJ_STRING_STATIC:
    FREE(obj, ObjStringStatic);
    break;
  }
  }
}

static uint32_t hashString(const char *chars, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= chars[i];
    hash *= 16777619u;
  }
  if (hash == 0)
    // 0 is used to indicate no hash value is cached
    return 1;
  return hash;
}

uint32_t objHash(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING: {
    ObjString *str = OBJ_AS_OBJSTRING(obj);
    if (str->hash != 0)
      return str->hash;
    str->hash = hashString(str->chars, str->length);
    return str->hash;
  }
  case OBJ_STRING_STATIC: {
    ObjStringStatic *str = OBJ_AS_OBJSTRINGSTATIC(obj);
    if (str->hash != 0)
      return str->hash;
    str->hash = hashString(str->chars, str->length);
    return str->hash;
  }
  default:
    return 0; // Unreachable
  }
}
