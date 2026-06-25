#include "object.h"

#include <stdio.h>
#include <string.h>

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
    Obj *obj = reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm->objects;
    vm->objects = obj;
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
        case OBJ_FUNCTION: {
            const ObjFunction *func = AS_FUNCTION(value);
            if (func->name == NULL) {
                printf("<script>");
            } else {
                printf("<fn %s>", AS_FUNCTION(value)->name->chars);
            }
            break;
        }
        case OBJ_STRING: {
            printf("%s", AS_CSTRING(value));
            break;
        }
    }
}

void object_free(Obj *obj) {
    switch (obj->type) {
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *)obj;
            chunk_free(&function->chunk);
            CLOX_FREE(ObjFunction, function);
            break;
        }
        case OBJ_STRING: {
            ObjString *string = (ObjString *)obj;
            CLOX_FREE(ObjString, string);
            break;
        }
    }
}

void object_free_all(Obj *head) {
    Obj *obj = head;
    while (obj != NULL) {
        Obj *next = obj->next;
        object_free(obj);
        obj = next;
    }
}

ObjFunction * object_function_new(VM *vm) {
    ObjFunction *func = (ObjFunction *)object_allocate(vm, sizeof(ObjFunction), OBJ_FUNCTION);
    func->arity = 0;
    func->name = NULL;
    chunk_init(&func->chunk);
    return func;
}

ObjString * object_string_copy(VM *vm, const char *chars, const size_t length) {
    ObjString *interned = table_find_string(&vm->strings, chars, length, hash_string(chars, length));
    if (interned != NULL) return interned;

    ObjString *obj = object_string_allocate(vm, length + 1);
    memcpy(obj->chars, chars, length);
    obj->hash = hash_string(chars, length);
    obj->length = length;
    obj->chars[length] = '\0';
    table_set(&vm->strings, obj, NIL_VAL, ENTRY_NO_FLAGS);
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
        object_free((Obj *)obj);
        return interned;
    }

    obj->length = length;
    obj->chars[length] = '\0';
    table_set(&vm->strings, obj, NIL_VAL, ENTRY_NO_FLAGS);
    return obj;
}
