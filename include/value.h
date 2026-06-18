#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include <stddef.h>

typedef double Value;

typedef struct {
    size_t count;
    size_t capacity;
    Value *values;
} ValueArray;

typedef struct {
    ValueArray array;
} ValueStack;

void value_print(Value value);

void value_array_init  (ValueArray *array);
void value_array_free  (ValueArray *array);
void value_array_write (ValueArray *array, Value value);

void  value_stack_init (ValueStack *stack);
void  value_stack_free (ValueStack *stack);
void  value_stack_push (ValueStack *stack, Value value);
Value value_stack_pop  (ValueStack *stack);
Value value_stack_peek (const ValueStack *stack);

#endif // CLOX_VALUE_H
