#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "object.h"
#include "table.h"
#include "value.h"

#define VM_GLOBAL_VAR_CONST 0
#define VM_GLOBAL_VAR_MUT   1

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
    ObjFunction *function;
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
    Obj *objects;
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
