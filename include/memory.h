#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include <stddef.h>

typedef struct VM VM;

#define CLOX_ALLOCATE_RAW(type, count) \
    (type *)reallocate_raw(NULL, count * sizeof(type))

#define CLOX_ALLOCATE_GC(vm, type, count) \
    (type *)reallocate_gc(vm, NULL, 0, sizeof(type) * (count))

#define CLOX_FREE_RAW(type, pointer) \
    (type *)reallocate_raw(pointer, 0)

#define CLOX_FREE_GC(vm, type, pointer) \
    (type *)reallocate_gc(vm, pointer, sizeof(type), 0)

#define CLOX_GROW_CAPACITY(capacity) \
    ((capacity < 8) ? 8 : capacity * 2)

#define CLOX_SHRINK_CAPACITY(capacity) \
    ((capacity <= 8) ? 8 : capacity / 2)

#define CLOX_RESIZE_ARRAY_RAW(type, pointer, new_capacity) \
    (type *)reallocate_raw(pointer, sizeof(type) * new_capacity)

#define CLOX_RESIZE_ARRAY_GC(vm, type, pointer, old_capacity, new_capacity) \
    (type *)reallocate(vm, pointer, sizeof(type) * old_capacity, sizeof(type) *new_capacity)

#define CLOX_FREE_ARRAY_RAW(type, pointer) \
    (type *)reallocate_raw(pointer, 0)

#define CLOX_FREE_ARRAY_GC(vm, type, pointer, old_capacity) \
    (type *)reallocate(vm, pointer, sizeof(type) * old_capacity, 0)

void *reallocate_raw (void *ptr, size_t new_size);
void *reallocate_gc  (VM *vm, void *ptr, size_t old_size, size_t new_size);

void gc_collect (VM *vm);

#endif // CLOX_MEMORY_H