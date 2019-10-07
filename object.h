#ifndef clox_object_h
#define clox_object_h

#include "common.h"

typedef enum {
  OBJ_STRING,
  OBJ_STRING_STATIC,
} ObjType;

typedef struct _Obj {
  ObjType type;
} Obj;

typedef struct {
  Obj obj;
  uint32_t hash;
  int length;
  char chars[]; // flexible array member
} ObjString;

typedef struct {
  Obj obj;
  uint32_t hash;
  int length;
  char *chars;
} ObjStringStatic;

#define OBJ_IS(obj, objtype) (((Obj *)obj)->type == (objtype))
#define OBJ_IS_ANY(obj, objtype1, objtype2)                                    \
  (_objIsAny((Obj *)(obj), (objtype1), (objtype2)))

#define OBJ_IS_ANYSTRING(obj) OBJ_IS_ANY(obj, OBJ_STRING, OBJ_STRING_STATIC)
#define OBJ_AS_OBJSTRING(obj) ((ObjString *)(obj))
#define OBJ_AS_OBJSTRINGSTATIC(obj) ((ObjStringStatic *)(obj))

static inline bool _objIsAny(Obj *obj, ObjType type1, ObjType type2) {
  return OBJ_IS(obj, type1) || OBJ_IS(obj, type2);
}

ObjStringStatic *objStringStaticNew(const char *, int length);
ObjString *objStringNew(const char *, int length);
ObjString *objStringsConcat(Obj *, Obj *);

bool objsEqual(Obj *, Obj *);
void objPrint(Obj *);
void objPrintRepr(Obj *);
void objFree(Obj *);
uint32_t objHash(Obj *);

#endif
