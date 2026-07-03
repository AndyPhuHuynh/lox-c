#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "object.h"
#include "table.h"
#include "value.h"

#define VM_GLOBAL_VAR_CONST 0
#define VM_GLOBAL_VAR_MUT   1

#define VM_UPVALUE_UPVALUE      0
#define VM_UPVALUE_UPVALUE_LONG 1
#define VM_UPVALUE_LOCAL        2
#define VM_UPVALUE_LOCAL_LONG   3

typedef struct Parser Parser;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    size_t slots_start_index;
} CallFrame;

typedef struct {
    size_t count;
    size_t capacity;
    CallFrame *frames;
} CallStack;

typedef struct VM {
    CallStack call_stack;
    ValueStack stack;
    Table globals;
    Table strings;
    ObjString *init_string;
    ObjUpvalue *open_upvalues;
    Obj *objects;
    size_t bytes_allocated;
    size_t next_gc;

    Parser *current_parser;
    size_t gray_count;
    size_t gray_capacity;
    Obj **gray_stack;
} VM;

void       call_stack_init (CallStack *stack);
void       call_stack_free (CallStack *stack);
void       call_stack_push (CallStack *stack, CallFrame frame);
void       call_stack_pop  (CallStack *stack);
CallFrame *call_stack_peek (const CallStack *stack);

void vm_init(VM *vm);
void vm_free(VM *vm);

InterpretResult vm_interpret(VM *vm, const char *source);

#endif // CLOX_VM_H
