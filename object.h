#ifndef clox_object_h
#define clox_object_h

#include "common.h"

typedef enum {
  OBJ_STRING,
  OBJ_STRING_STATIC,
} ObjType;

typedef struct {
  ObjType type;
} Obj;

typedef struct {
  Obj obj;
  uint32_t hash;
  int length;
} ObjStringBase;

typedef struct {
  ObjStringBase base;
  char chars[]; // flexible array member
} ObjString;

typedef struct {
  ObjStringBase base;
  char *chars;
} ObjStringStatic;

#define OBJ_IS(obj, objtype) (((Obj *)obj)->type == (objtype))
#define OBJ_IS_ANY(obj, objtype1, objtype2)                                    \
  (_objIsAny((Obj *)(obj), (objtype1), (objtype2)))

#define OBJ_IS_ANYSTRING(obj) OBJ_IS_ANY(obj, OBJ_STRING, OBJ_STRING_STATIC)

#define OBJ_AS_OBJSTRINGBASE(obj) ((ObjStringBase *)(obj))
#define OBJ_AS_OBJSTRING(obj) ((ObjString *)(obj))
#define OBJ_AS_OBJSTRINGSTATIC(obj) ((ObjStringStatic *)(obj))
#define OBJ_AS_CHARS(obj) (_objAsChars((Obj *)(obj)))

static inline bool _objIsAny(Obj *obj, ObjType type1, ObjType type2) {
  return OBJ_IS(obj, type1) || OBJ_IS(obj, type2);
}

static inline char *_objAsChars(Obj *obj) {
  switch (obj->type) {
  case (OBJ_STRING):
    return OBJ_AS_OBJSTRING(obj)->chars;
  case (OBJ_STRING_STATIC):
    return OBJ_AS_OBJSTRINGSTATIC(obj)->chars;
  }
  return NULL;
}

ObjStringStatic *objStringStaticNew(const char *, int length);
ObjString *objStringNew(const char *, int length);

bool objsEqual(Obj *, Obj *);
void objPrint(Obj *);
void objPrintRepr(Obj *);
void objFree(Obj *);

ObjString *objConcat(ObjStringBase *, ObjStringBase *);
uint32_t objHash(ObjStringBase *);

#endif
