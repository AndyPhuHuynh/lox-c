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
        array->values = CLOX_RESIZE_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void value_stack_init(ValueStack *stack) {
    value_array_init(&stack->array);
}

void value_stack_free(ValueStack *stack) {
    value_array_free(&stack->array);
}

void value_stack_push(ValueStack *stack, const Value value) {
    value_array_write(&stack->array, value);
}

Value value_stack_pop(ValueStack *stack) {
    stack->array.count--;
    const Value value = stack->array.values[stack->array.count];
    if (stack->array.count <= stack->array.capacity / 4 && stack->array.capacity > 8) {
        const size_t old_capacity = stack->array.capacity;
        stack->array.capacity = CLOX_SHRINK_CAPACITY(old_capacity);
        stack->array.values = CLOX_RESIZE_ARRAY(Value, stack->array.values, old_capacity, stack->array.capacity);
    }
    return value;
}

Value value_stack_peek(const ValueStack *stack) {
    return stack->array.values[stack->array.count - 1];
}
