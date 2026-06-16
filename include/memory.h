#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include <stddef.h>

#define CLOX_GROW_CAPACITY(capacity) \
    ((capacity < 8) ? 8 : capacity * 2)

#define CLOX_GROW_ARRAY(type, pointer, old_capacity, new_capacity) \
    (type *)reallocate(pointer, sizeof(type) * old_capacity, sizeof(type) *new_capacity)

#define CLOX_FREE_ARRAY(type, pointer, old_capacity) \
    (type *)reallocate(pointer, sizeof(type) * old_capacity, 0)

void *reallocate(void *ptr, size_t old_size, size_t new_size);

#endif // CLOX_MEMORY_H