#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "value.h"

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    ValueStack stack;
} VM;

void vm_init(VM *vm);
void vm_free(VM *vm);

InterpretResult vm_interpret(VM *vm, const char *source);

#endif // CLOX_VM_H
