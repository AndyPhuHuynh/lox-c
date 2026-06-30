#include "memory.h"

#include "debug.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

void * reallocate_raw(void *ptr, const size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (result == NULL) {
        fprintf(stderr, "Failed to reallocate %zu bytes for vm memory", new_size);
        exit(EXIT_FAILURE);
    }
    return result;
}

void *reallocate_gc(VM *vm, void *ptr, const size_t old_size, const size_t new_size) {
    vm->bytes_allocated += new_size - old_size;
#ifdef CLOX_DEBUG_STRESS_GC
    if (new_size > old_size) {
        gc_collect(vm);
    }
#endif

    if (vm->bytes_allocated > vm->next_gc) {
        gc_collect(vm);
    }

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (result == NULL) {
        fprintf(stderr, "Failed to reallocate %zu bytes for vm memory", new_size);
        exit(EXIT_FAILURE);
    }
    return result;
}

static void gc_mark_object(VM *vm, Obj *object) {
    if (object == NULL) return;
    if (object->is_marked) return;

#ifdef CLOX_DEBUG_LOX_GC
    printf("%p mark ", (void *)object);
    value_print(OBJ_VAL(object));
    printf("\n");
#endif

    object->is_marked = true;

    if (vm->gray_capacity < vm->gray_count + 1) {
        vm->gray_capacity = CLOX_GROW_CAPACITY(vm->gray_capacity);
        const size_t new_buffer_size = vm->gray_capacity * sizeof(Obj *);
        Obj **new_buffer = realloc(vm->gray_stack, new_buffer_size);
        if (new_buffer == NULL) {
            fprintf(stderr, "Failed to reallocate %zu bytes for vm gray stack", new_buffer_size);
            exit(EXIT_FAILURE);
        }
        vm->gray_stack = new_buffer;
    }
    vm->gray_stack[vm->gray_count++] = object;
}

static void gc_mark_value(VM *vm, const Value value) {
    if (IS_OBJ(value)) {
        gc_mark_object(vm, AS_OBJ(value));
    }
}

static void gc_mark_value_array(VM *vm, const ValueArray *array) {
    for (size_t i = 0; i < array->count; i++) {
        gc_mark_value(vm, array->values[i]);
    }
}

static void gc_mark_table(VM *vm, const Table *table) {
    for (size_t i = 0; i < table->capacity; i++) {
        const Entry *entry = &table->entries[i];
        gc_mark_object(vm, (Obj *)entry->key);
        gc_mark_value(vm, entry->value);
    }
}

static void gc_mark_compiler_roots(VM *vm) {
    if (vm->current_parser == NULL) return;
    const Compiler *current = vm->current_parser->compiler;
    while (current != NULL) {
        gc_mark_object(vm, (Obj *)current->function);
        current = current->enclosing;
    }
}

static void gc_mark_roots(VM *vm) {
    for (size_t i = 0; i < vm->stack.array.count; i++) {
        gc_mark_value(vm, vm->stack.array.values[i]);
    }

    for (size_t i = 0; i < vm->call_stack.count; i++) {
        gc_mark_object(vm, (Obj *)vm->call_stack.frames[i].closure);
    }

    for (const ObjUpvalue *upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        gc_mark_object(vm, (Obj *)upvalue);
    }

    gc_mark_table(vm, &vm->globals);
    gc_mark_compiler_roots(vm);
}

static void gc_blacken_object(VM *vm, Obj *object) {
#ifdef CLOX_DEBUG_LOX_GC
    printf("%p blacken ", (void *)object);
    value_print(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_CLASS: {
            const ObjClass *class = (ObjClass *)object;
            gc_mark_object(vm, (Obj *)class->name);
            break;
        }
        case OBJ_CLOSURE: {
            const ObjClosure *closure = (ObjClosure *)object;
            gc_mark_object(vm, (Obj *)closure->function);
            for (size_t i = 0; i < closure->upvalue_count; i++) {
                gc_mark_object(vm, (Obj *)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            const ObjFunction *function = (ObjFunction *)object;
            gc_mark_object(vm, (Obj *)function->name);
            gc_mark_value_array(vm, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            const ObjInstance *instance = (ObjInstance *)object;
            gc_mark_object(vm, (Obj *)instance->class);
            gc_mark_table(vm, &instance->fields);
            break;
        }
        case OBJ_NATIVE: {
            const ObjNative *native = (ObjNative *)object;
            gc_mark_object(vm, (Obj *)native->name);
            break;
        }
        case OBJ_STRING:
            break;
        case OBJ_UPVALUE: {
            const ObjUpvalue *upvalue = (ObjUpvalue *)object;
            switch (upvalue->type) {
                case UPVALUE_OPEN:
                    break;
                case UPVALUE_CLOSED: {
                    gc_mark_value(vm, upvalue->as.closed);
                    break;
                }
            }
        }
    }
}

static void gc_trace_references(VM *vm) {
    while (vm->gray_count > 0) {
        Obj *obj = vm->gray_stack[--vm->gray_count];
        gc_blacken_object(vm, obj);
    }
}

static void gc_table_remove_white(const Table *table) {
    for (size_t i = 0; i < table->capacity; i++) {
        const Entry *entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            table_delete(table, entry->key);
        }
    }
}

static void gc_sweep(VM *vm) {
    Obj **link = &vm->objects;
    while (*link != NULL) {
        if ((*link)->is_marked) {
            (*link)->is_marked = false;
            link = &(*link)->next;
        } else {
            Obj *unreached = *link;
            *link = unreached->next;
            object_free(vm, unreached);
        }
    }
}

void gc_collect(VM *vm) {
#ifdef CLOX_DEBUG_LOX_GC
    printf("-- gc begin\n");
    const size_t before = vm->bytes_allocated;
#endif

    gc_mark_roots(vm);
    gc_trace_references(vm);
    gc_table_remove_white(&vm->strings);
    gc_sweep(vm);

    vm->next_gc = vm->bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef CLOX_DEBUG_LOX_GC
    printf("-- gc end\n");
    printf("collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm->bytes_allocated, before, vm->bytes_allocated, vm->next_gc);
#endif
}
