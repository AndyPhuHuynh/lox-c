#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;
} Value;

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0.0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

typedef struct {
    size_t count;
    size_t capacity;
    Value *values;
} ValueArray;

typedef struct {
    ValueArray array;
} ValueStack;

void value_print(Value value);

bool value_is_falsey(Value value);
bool value_equals(Value value1, Value value2);

void value_array_init  (ValueArray *array);
void value_array_free  (ValueArray *array);
void value_array_write (ValueArray *array, Value value);

void  value_stack_init (ValueStack *stack);
void  value_stack_free (ValueStack *stack);
void  value_stack_push (ValueStack *stack, Value value);
Value value_stack_pop  (ValueStack *stack);
Value value_stack_peek (const ValueStack *stack, size_t distance);

#endif // CLOX_VALUE_H
