#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, object_type) \
    (type *)object_allocate(vm, sizeof(type), object_type)

static Obj *object_allocate(VM *vm, const size_t size, const ObjType type) {
    Obj *obj = reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm->objects;
    vm->objects = obj;
    return obj;
}

bool object_is_type(const Value value, const ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void object_print(const Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
    }
}

void object_free(Obj *obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *)obj;
            CLOX_FREE_ARRAY(char, string->chars, string->length + 1);
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

ObjString * object_string_take(VM *vm, char *chars, const size_t length) {
    ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString * object_string_copy(VM *vm, const char *chars, const size_t length) {
    char *heap_chars = CLOX_ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return object_string_take(vm, heap_chars, length);
}

ObjString * object_string_concatenate(VM *vm, const ObjString *a, const ObjString *b) {
    const size_t length = a->length + b->length;
    char *chars = CLOX_ALLOCATE(char, length);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    return object_string_take(vm, chars, length);
}
