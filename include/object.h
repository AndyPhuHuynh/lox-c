#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "value.h"
#include "vm.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) (object_is_type(value, OBJ_STRING))

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

typedef struct Obj {
    ObjType type;
    Obj *next;
} Obj;

typedef struct ObjString {
    Obj obj;
    size_t length;
    char *chars;
} ObjString;

bool object_is_type(Value value, ObjType type);
void object_print(Value value);
void object_free(Obj* obj);
void object_free_all(Obj *head);

ObjString *object_string_take(VM *vm, char *chars, size_t length);
ObjString *object_string_copy(VM *vm, const char *chars, size_t length);
ObjString *object_string_concatenate(VM *vm, const ObjString *a, const ObjString *b);

#endif // CLOX_OBJECT_H
