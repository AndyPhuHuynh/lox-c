#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "memory.h"
#include "vm.h"

// FNV-la hash
static uint32_t hash_string(const char* key, const size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

static Obj *object_allocate(VM *vm, const size_t size, const ObjType type) {
    Obj *obj = reallocate_gc(vm, NULL, 0, size);
    obj->type = type;
    obj->next = vm->objects;
    obj->is_marked = false;
    vm->objects = obj;

#ifdef CLOX_DEBUG_LOX_GC
    printf("%p allocate %zu for %d\n", (void *)obj, size, type);
#endif

    return obj;
}

static ObjString *object_string_allocate(VM *vm, const size_t string_length) {
    return (ObjString *)object_allocate(vm, sizeof(ObjString) + string_length, OBJ_STRING);
}

bool object_is_type(const Value value, const ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void object_print(const Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_CLOSURE: {
            object_closure_print(AS_CLOSURE(value));
            break;
        }
        case OBJ_FUNCTION: {
            object_function_print(AS_FUNCTION(value));
            break;
        }
        case OBJ_NATIVE: {
            object_native_print(AS_NATIVE(value));
            break;
        }
        case OBJ_STRING: {
            object_string_print(AS_STRING(value));
            break;
        }
        case OBJ_UPVALUE: {
            printf("<upvalue>"); // Should not execute
            break;
        }
    }
}

void object_free(VM *vm, Obj *obj) {
#ifdef CLOX_DEBUG_LOX_GC
    printf("%p free: ", (void *)obj);
    object_print(OBJ_VAL(obj));
    printf("\n");
#endif

    switch (obj->type) {
        case OBJ_CLOSURE: {
            object_closure_free(vm, (ObjClosure *)obj);
            break;
        }
        case OBJ_FUNCTION: {
            object_function_free(vm, (ObjFunction *)obj);
            break;
        }
        case OBJ_NATIVE: {
            object_native_free(vm, (ObjNative *)obj);
            break;
        }
        case OBJ_STRING: {
            object_string_free(vm, (ObjString *)obj);
            break;
        }
        case OBJ_UPVALUE: {
            object_upvalue_free(vm, (ObjUpvalue *)obj);
            break;
        }
    }
}

void object_free_all(VM *vm, Obj *head) {
    Obj *obj = head;
    while (obj != NULL) {
        Obj *next = obj->next;
        object_free(vm, obj);
        obj = next;
    }
    free(vm->gray_stack);
}

ObjClosure * object_closure_new(VM *vm, ObjFunction *function) {
    ObjClosure *closure = (ObjClosure *)object_allocate(vm, sizeof(ObjClosure), OBJ_CLOSURE);
    closure->function = function;

    ObjUpvalue **upvalues = CLOX_ALLOCATE_RAW(ObjUpvalue *, function->upvalue_count);
    for (size_t i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

void object_closure_free(VM *vm, ObjClosure *closure) {
    CLOX_FREE_ARRAY_RAW(ObjUpvalue *, closure->upvalues);
    CLOX_FREE_GC(vm, ObjClosure, closure);
}

void object_closure_print(const ObjClosure *closure) {
    printf("closure: ");
    object_function_print(closure->function);
}

ObjFunction *object_function_new(VM *vm) {
    ObjFunction *func = (ObjFunction *)object_allocate(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    func->name = NULL;
    func->arity = 0;
    func->upvalue_count = 0;
    chunk_init(&func->chunk);
    return func;
}

void object_function_free(VM *vm, ObjFunction *function) {
    chunk_free(&function->chunk);
    CLOX_FREE_GC(vm, ObjClosure, function);
}

void object_function_print(const ObjFunction *function) {
    if (function->name == NULL) {
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

ObjNative * object_native_new(VM *vm, const NativeFn function, ObjString *name, const size_t arity) {
    ObjNative *obj = (ObjNative *)object_allocate(vm, sizeof(ObjNative), OBJ_NATIVE);
    obj->function = function;
    obj->name = name;
    obj->arity = arity;
    return obj;
}

void object_native_free(VM *vm, ObjNative *native) {
    CLOX_FREE_GC(vm, ObjNative, native);
}

void object_native_print(const ObjNative *native) {
    printf("<native fn %s>", native->name->chars);
}

ObjString * object_string_copy(VM *vm, const char *chars, const size_t length) {
    ObjString *interned = table_find_string(&vm->strings, chars, length, hash_string(chars, length));
    if (interned != NULL) return interned;

    ObjString *obj = object_string_allocate(vm, length + 1);
    memcpy(obj->chars, chars, length);
    obj->hash = hash_string(chars, length);
    obj->length = length;
    obj->chars[length] = '\0';

    // Make gc happy
    value_stack_push(&vm->stack, OBJ_VAL(obj));

    table_set(&vm->strings, obj, NIL_VAL, ENTRY_NO_FLAGS);

    value_stack_pop(&vm->stack);
    return obj;
}

ObjString * object_string_concatenate(VM *vm, const ObjString *a, const ObjString *b) {
    const size_t length = a->length + b->length;
    ObjString *obj = object_string_allocate(vm, length + 1);
    memcpy(obj->chars, a->chars, a->length);
    memcpy(obj->chars + a->length, b->chars, b->length);
    obj->hash = hash_string(a->chars, length);

    ObjString *interned = table_find_string(&vm->strings, obj->chars, length, obj->hash);
    if (interned != NULL) {
        object_free(vm, (Obj *)obj);
        return interned;
    }

    obj->length = length;
    obj->chars[length] = '\0';
    table_set(&vm->strings, obj, NIL_VAL, ENTRY_NO_FLAGS);
    return obj;
}

void object_string_free(VM *vm, ObjString *string) {
    CLOX_FREE_GC(vm, ObjString, string);
}

void object_string_print(ObjString *string) {
    printf("%s", string->chars);
}

ObjUpvalue * object_upvalue_new(VM *vm, const size_t stack_index) {
    ObjUpvalue *upvalue = (ObjUpvalue *)object_allocate(vm, sizeof(ObjUpvalue), OBJ_UPVALUE);
    upvalue->type = UPVALUE_OPEN;
    upvalue->as.stack_index = stack_index;
    upvalue->next = NULL;
    return upvalue;
}

void object_upvalue_free(VM *vm, ObjUpvalue *upvalue) {
    CLOX_FREE_GC(vm, ObjUpvalue, upvalue);
}
