#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "value.h"

#define NATIVE_ARITY_VARIADIC ((size_t)-1)

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) (object_is_type(value, OBJ_FUNCTION))
#define IS_NATIVE(value)   (object_is_type(value, OBJ_NATIVE))
#define IS_STRING(value)   (object_is_type(value, OBJ_STRING))

#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value)   ((ObjNative *)AS_OBJ(value))
#define AS_STRING(value)   ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value)  (((ObjString *)AS_OBJ(value))->chars)

typedef struct VM VM;

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
} ObjType;

typedef struct Obj {
    ObjType type;
    Obj *next;
} Obj;

typedef struct ObjFunction {
    Obj obj;
    ObjString *name;
    size_t arity;
    Chunk chunk;
} ObjFunction;

typedef bool (*NativeFn)(VM* vm, Value *values, size_t arg_count, Value *out);

typedef struct ObjNative {
    Obj obj;
    NativeFn function;
    ObjString *name;
    size_t arity;
} ObjNative;

typedef struct ObjString {
    Obj obj;
    uint32_t hash;
    size_t length;
    char chars[];
} ObjString;

bool object_is_type(Value value, ObjType type);
void object_print(Value value);
void object_free(Obj* obj);
void object_free_all(Obj *head);

ObjFunction *object_function_new(VM *vm);

ObjNative *object_native_new(VM *vm, NativeFn function, ObjString *name, size_t arity);

ObjString *object_string_copy(VM *vm, const char *chars, size_t length);
ObjString *object_string_concatenate(VM *vm, const ObjString *a, const ObjString *b);

#endif // CLOX_OBJECT_H
