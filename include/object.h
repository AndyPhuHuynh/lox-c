#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "value.h"

#define NATIVE_ARITY_VARIADIC ((size_t)-1)

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value)  (object_is_type(value, OBJ_CLOSURE))
#define IS_FUNCTION(value) (object_is_type(value, OBJ_FUNCTION))
#define IS_NATIVE(value)   (object_is_type(value, OBJ_NATIVE))
#define IS_STRING(value)   (object_is_type(value, OBJ_STRING))

#define AS_CLOSURE(value)  ((ObjClosure *)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value)   ((ObjNative *)AS_OBJ(value))
#define AS_STRING(value)   ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value)  (((ObjString *)AS_OBJ(value))->chars)

typedef struct VM VM;
typedef bool (*NativeFn)(VM* vm, Value *values, size_t arg_count, Value *out);

typedef struct ObjClosure   ObjClosure;
typedef struct ObjFunction  ObjFunction;
typedef struct ObjNative    ObjNative;
typedef struct ObjString    ObjString;
typedef struct ObjUpvalue   ObjUpvalue;

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

typedef struct Obj {
    ObjType type;
    Obj *next;
} Obj;

struct ObjClosure {
    Obj obj;
    ObjFunction *function;
    ObjUpvalue **upvalues;
    size_t upvalue_count;
};

struct ObjFunction {
    Obj obj;
    ObjString *name;
    size_t arity;
    size_t upvalue_count;
    Chunk chunk;
};

struct ObjNative {
    Obj obj;
    NativeFn function;
    ObjString *name;
    size_t arity;
};

struct ObjString {
    Obj obj;
    uint32_t hash;
    size_t length;
    char chars[];
};

struct ObjUpvalue {
    Obj obj;
    Value *location;
};

bool object_is_type(Value value, ObjType type);
void object_print(Value value);
void object_free(Obj* obj);
void object_free_all(Obj *head);

ObjClosure *object_closure_new   (VM *vm, ObjFunction *function);
void        object_closure_free  (ObjClosure *closure);
void        object_closure_print (const ObjClosure *closure);

ObjFunction *object_function_new(VM *vm);
void         object_function_free  (ObjFunction *function);
void         object_function_print (const ObjFunction *function);

ObjNative *object_native_new   (VM *vm, NativeFn function, ObjString *name, size_t arity);
void       object_native_free  (ObjNative *native);
void       object_native_print (const ObjNative *native);

ObjString *object_string_copy        (VM *vm, const char *chars, size_t length);
ObjString *object_string_concatenate (VM *vm, const ObjString *a, const ObjString *b);
void       object_string_free        (ObjString *string);
void       object_string_print       (ObjString *string);

ObjUpvalue *object_upvalue_new  (VM *vm, Value *location);
void        object_upvalue_free (ObjUpvalue *upvalue);


#endif // CLOX_OBJECT_H
