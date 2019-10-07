#include <stdio.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "object.h"

static Obj *_objNew(Obj *obj, ObjType type) {
  obj->type = type;
  return obj;
}

#define objNew(type, objType)                                                  \
  ((type *)_objNew((Obj *)ALLOCATE(type), (objType)))

#define objNewFlex(type, arrayType, length, objType)                           \
  ((type *)_objNew((Obj *)ALLOCATE_FLEX(type, arrayType, length), (objType)))

static void objStringBaseInit(ObjStringBase *base, int length) {
  base->length = length;
  base->hash = 0;
}

ObjStringStatic *objStringStaticNew(const char *str, int length) {
  ObjStringStatic *obj = objNew(ObjStringStatic, OBJ_STRING_STATIC);
  objStringBaseInit(&obj->base, length);
  obj->chars = (char *)str;
  return obj;
}

ObjString *objStringNew(const char *str, int length) {
  ObjString *obj = objNewFlex(ObjString, char, length, OBJ_STRING);
  objStringBaseInit(&obj->base, length);
  memcpy(obj->chars, str, length);
  return obj;
}

ObjString *objConcat(ObjStringBase *a, ObjStringBase *b) {
  int length = a->length + b->length;
  ObjString *obj = objNewFlex(ObjString, char, length, OBJ_STRING);
  objStringBaseInit(&obj->base, length);
  memcpy(obj->chars, OBJ_AS_CHARS(a), a->length);
  memcpy(obj->chars + a->length, OBJ_AS_CHARS(b), b->length);
  return obj;
}

void objPrint(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING: // fallthrough
  case OBJ_STRING_STATIC: {
    printf("%.*s", OBJ_AS_OBJSTRINGBASE(obj)->length, OBJ_AS_CHARS(obj));
    break;
  }
  default:
    return; // Unreachable
  }
}

void objPrintRepr(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING:
    printf("<OBJ_STRING ");
    break;
  case OBJ_STRING_STATIC:
    printf("<OBJ_STRING_STATIC ");
    break;
  }
  printf("(%d) ", OBJ_AS_OBJSTRINGBASE(obj)->length);
  objPrint(obj);
  printf(">");
}

bool objsEqual(Obj *a, Obj *b) {
  if (!(OBJ_IS_ANYSTRING(a) && OBJ_IS_ANYSTRING(b)))
    return false;

  if (a == b)
    return true;

  ObjStringBase *aBase = OBJ_AS_OBJSTRINGBASE(a);
  ObjStringBase *bBase = OBJ_AS_OBJSTRINGBASE(b);

  if (aBase->length != bBase->length)
    return false;

  if (objHash(aBase) != objHash(bBase))
    return false;

  return memcmp(OBJ_AS_CHARS(a), OBJ_AS_CHARS(b), aBase->length) == 0;
}

void objFree(Obj *obj) {
#ifdef DEBUG_PRINT_FREE
  printf("-- free obj -- ");
  objPrintRepr(obj);
  printf("\n");
#endif
  switch (obj->type) {
  case OBJ_STRING: {
    FREE_FLEX(obj, ObjString, char, OBJ_AS_OBJSTRINGBASE(obj)->length);
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

uint32_t objHash(ObjStringBase *obj) {
  if (obj->hash != 0)
    return obj->hash;
  obj->hash = hashString(OBJ_AS_CHARS(obj), obj->length);
  return obj->hash;
}
