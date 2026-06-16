#include "value.h"

#include <stdio.h>

#include "memory.h"

void value_print(const Value value) {
    printf("%g", value);
}

void value_array_init(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_free(ValueArray *array) {
    CLOX_FREE_ARRAY(Value, array->values, array->capacity);
    value_array_init(array);
}

void value_array_write(ValueArray *array, const Value value) {
    if (array->capacity < array->count + 1) {
        const size_t old_capacity = array->capacity;
        array->capacity = CLOX_GROW_CAPACITY(old_capacity);
        array->values = CLOX_GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}
