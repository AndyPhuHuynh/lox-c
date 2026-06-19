#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "vm.h"

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

ObjString * object_string_copy(VM *vm, const char *chars, const size_t length) {
    ObjString *obj = object_string_allocate(vm, length + 1);
    memcpy(obj->chars, chars, length);
    obj->length = length;
    obj->chars[length] = '\0';
    return obj;
}

ObjString * object_string_concatenate(VM *vm, const ObjString *a, const ObjString *b) {
    const size_t length = a->length + b->length;
    ObjString *obj = object_string_allocate(vm, length + 1);
    memcpy(obj->chars, a->chars, a->length);
    memcpy(obj->chars + a->length, b->chars, b->length);
    obj->length = length;
    obj->chars[length] = '\0';
    return obj;
}
