#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "table.h"
#include "value.h"

#define NATIVE_ARITY_VARIADIC ((size_t)-1)

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) (object_is_type(value, OBJ_BOUND_METHOD))
#define IS_CLASS(value)        (object_is_type(value, OBJ_CLASS))
#define IS_CLOSURE(value)      (object_is_type(value, OBJ_CLOSURE))
#define IS_FUNCTION(value)     (object_is_type(value, OBJ_FUNCTION))
#define IS_INSTANCE(value)     (object_is_type(value, OBJ_INSTANCE))
#define IS_NATIVE(value)       (object_is_type(value, OBJ_NATIVE))
#define IS_STRING(value)       (object_is_type(value, OBJ_STRING))

#define AS_BOUND_METHOD(value) ((ObjBoundMethod *)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass *)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure *)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction *)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance *)AS_OBJ(value))
#define AS_NATIVE(value)       ((ObjNative *)AS_OBJ(value))
#define AS_STRING(value)       ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString *)AS_OBJ(value))->chars)

typedef struct VM VM;
typedef bool (*NativeFn)(VM* vm, Value *values, size_t arg_count, Value *out);

typedef struct Obj            Obj;
typedef struct ObjBoundMethod ObjBoundMethod;
typedef struct ObjClass       ObjClass;
typedef struct ObjClosure     ObjClosure;
typedef struct ObjFunction    ObjFunction;
typedef struct ObjInstance    ObjInstance;
typedef struct ObjNative      ObjNative;
typedef struct ObjString      ObjString;
typedef struct ObjUpvalue     ObjUpvalue;

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

typedef enum {
    UPVALUE_OPEN,
    UPVALUE_CLOSED,
} ObjUpvalueType;

struct Obj {
    ObjType type;
    Obj *next;
    bool is_marked;
};

struct ObjBoundMethod {
    Obj obj;
    Value receiver;
    ObjClosure *method;
};

struct ObjClass {
    Obj obj;
    ObjString *name;
    Table methods;
};

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

struct ObjInstance {
    Obj obj;
    ObjClass *class;
    Table fields;
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
    char chars[1];
};

struct ObjUpvalue {
    Obj obj;
    ObjUpvalueType type;
    union {
        Value closed;
        size_t stack_index;
    } as;
    ObjUpvalue *next;
};

bool object_is_type  (Value value, ObjType type);
void object_print    (Value value);
void object_free     (VM *vm, Obj* obj);
void object_free_all (VM *vm, Obj *head);

ObjBoundMethod *object_bound_method_new   (VM *vm, Value receiver, ObjClosure *method);
void            object_bound_method_free  (VM *vm, ObjBoundMethod *bound_method);
void            object_bound_method_print (const ObjBoundMethod *bound_method);

ObjClass *object_class_new   (VM *vm, ObjString *name);
void      object_class_free  (VM *vm, ObjClass *class);
void      object_class_print (const ObjClass *class);

ObjClosure *object_closure_new   (VM *vm, ObjFunction *function);
void        object_closure_free  (VM *vm, ObjClosure *closure);
void        object_closure_print (const ObjClosure *closure);

ObjFunction *object_function_new   (VM *vm);
void         object_function_free  (VM *vm, ObjFunction *function);
void         object_function_print (const ObjFunction *function);

ObjInstance *object_instance_new   (VM *vm, ObjClass *class);
void         object_instance_free  (VM *vm, ObjInstance *instance);
void         object_instance_print (const ObjInstance *instance);

ObjNative *object_native_new   (VM *vm, NativeFn function, ObjString *name, size_t arity);
void       object_native_free  (VM *vm, ObjNative *native);
void       object_native_print (const ObjNative *native);

ObjString *object_string_copy        (VM *vm, const char *chars, size_t length);
ObjString *object_string_concatenate (VM *vm, const ObjString *a, const ObjString *b);
void       object_string_free        (VM *vm, ObjString *string);
void       object_string_print       (ObjString *string);

ObjUpvalue *object_upvalue_new  (VM *vm, size_t stack_index);
void        object_upvalue_free (VM *vm, ObjUpvalue *upvalue);


#endif // CLOX_OBJECT_H
