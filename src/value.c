#include "value.h"

#include <stdio.h>

#include "memory.h"

void value_print(const Value value) {
    switch (value.type) {
        case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL:    printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    }
}

bool value_is_falsey(const Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool value_equals(const Value value1, const Value value2) {
    if (value1.type != value2.type) return false;
    switch (value1.type) {
        case VAL_BOOL:   return AS_BOOL(value1) == AS_BOOL(value2);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(value1) == AS_NUMBER(value2);
        default: return false; // unreachable
    }
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

Value value_stack_peek(const ValueStack *stack, const size_t distance) {
    return stack->array.values[stack->array.count - 1 - distance];
}
