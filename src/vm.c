#include "vm.h"

#include <stdbool.h>
#include <stdio.h>

#define CLOX_DEBUG_TRACE_EXECUTION
#ifdef CLOX_DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

#include "value.h"

static InterpretResult vm_run(VM *vm);

void vm_init(VM *vm) {
    vm->chunk = NULL;
    vm->ip = NULL;
    value_stack_init(&vm->stack);
}

void vm_free(VM *vm) {
    value_stack_free(&vm->stack);
}

InterpretResult vm_interpret(VM *vm, Chunk *chunk) {
    vm->chunk = chunk;
    vm->ip = chunk->code;
    return vm_run(vm);
}

static InterpretResult vm_run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define BINARY_OP(op) \
    do { \
        double b = value_stack_pop(&vm->stack); \
        double a = value_stack_pop(&vm->stack); \
        value_stack_push(&vm->stack, a op b); \
    } while (false)

#ifdef CLOX_DEBUG_TRACE_EXECUTION
    LineView view = line_view_init(&vm->chunk->lines);
#endif
    while (true) {
#ifdef CLOX_DEBUG_TRACE_EXECUTION
        printf("          ");
        for (size_t i = 0; i < vm->stack.array.count; i++) {
            printf("[");
            value_print(vm->stack.array.values[i]);
            printf("]");
        }
        printf("\n");

        const size_t old_offset = (size_t)(vm->ip - vm->chunk->code);
        const size_t new_offset = disassemble_instruction(vm->chunk, &view, old_offset);
        line_view_advance(&view, new_offset - old_offset);
#endif

        switch (READ_BYTE()) {
            case OP_CONSTANT: {
                const Value constant = vm->chunk->constants.values[READ_BYTE()];
                value_stack_push(&vm->stack, constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                const size_t index =
                    (size_t)READ_BYTE()
                    | (size_t)READ_BYTE() << 8
                    | (size_t)READ_BYTE() << 16;
                const Value constant = vm->chunk->constants.values[index];
                value_stack_push(&vm->stack, constant);
                break;
            }
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUB: BINARY_OP(-); break;
            case OP_MUL: BINARY_OP(*); break;
            case OP_DIV: BINARY_OP(/); break;
            case OP_NEGATE: {
                vm->stack.array.values[vm->stack.array.count - 1] *= -1;
                break;
            }
            case OP_RETURN: {
                value_print(value_stack_pop(&vm->stack));
                printf("\n");
                return INTERPRET_OK;
            }
            default:
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef BINARY_OP
}