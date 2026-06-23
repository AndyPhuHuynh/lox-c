#include "vm.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "value.h"

static void vm_runtime_error(VM *vm, const char *format, ...) {
    const size_t instruction = (size_t)(vm->ip - vm->chunk->code - 1);
    const size_t line = line_array_get(&vm->chunk->lines, instruction);
    fprintf(stderr, "[line %zu] in script: ", line);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    value_stack_free(&vm->stack);
    value_stack_init(&vm->stack);
}

void vm_init(VM *vm) {
    vm->chunk = NULL;
    vm->ip = NULL;
    value_stack_init(&vm->stack);
    table_init(&vm->globals);
    table_init(&vm->strings);
    vm->objects = NULL;
}

void vm_free(VM *vm) {
    value_stack_free(&vm->stack);
    table_free(&vm->globals);
    table_free(&vm->strings);
    object_free_all(vm->objects);
    vm->objects = NULL;
}

static uint8_t read_byte(VM *vm) {
    return *vm->ip++;
}

static size_t read_bytes_long(VM *vm) {
    const size_t bytes =
        (size_t)vm->ip[0]
        | (size_t)vm->ip[1] << 8
        | (size_t)vm->ip[2] << 16;
    vm->ip += 3;
    return bytes;
}

static Value *read_constant(VM *vm) {
    return &vm->chunk->constants.values[read_byte(vm)];
}

static Value *read_constant_long(VM *vm) {
    return &vm->chunk->constants.values[read_bytes_long(vm)];
}

static ObjString *read_string(VM *vm) {
    return AS_STRING(*read_constant(vm));
}

static ObjString *read_string_long(VM *vm) {
    return AS_STRING(*read_constant_long(vm));
}

static InterpretResult vm_run(VM *vm) {
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(value_stack_peek(&vm->stack, 0)) \
            || !IS_NUMBER(value_stack_peek(&vm->stack, 1))) \
        { \
            vm_runtime_error(vm, "Operands for '"#op"' must both be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(value_stack_pop(&vm->stack)); \
        double a = AS_NUMBER(value_stack_pop(&vm->stack)); \
        value_stack_push(&vm->stack, value_type(a op b)); \
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

        switch (read_byte(vm)) {
            case OP_CONSTANT: {
                value_stack_push(&vm->stack, *read_constant(vm));
                break;
            }
            case OP_CONSTANT_LONG: {
                value_stack_push(&vm->stack, *read_constant_long(vm));
                break;
            }
            case OP_NIL: value_stack_push(&vm->stack, NIL_VAL); break;
            case OP_TRUE: value_stack_push(&vm->stack, BOOL_VAL(true)); break;
            case OP_FALSE: value_stack_push(&vm->stack, BOOL_VAL(false)); break;
            case OP_POP: value_stack_pop(&vm->stack); break;
            case OP_GET_LOCAL: {
                const uint8_t slot = read_byte(vm);
                value_stack_push(&vm->stack, vm->stack.array.values[slot]);
                break;
            }
            case OP_GET_LOCAL_LONG: {
                const size_t slot = read_bytes_long(vm);
                value_stack_push(&vm->stack, vm->stack.array.values[slot]);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *var_name = read_string(vm);
                Entry *entry = NULL;
                if (!table_get(&vm->globals, var_name, &entry)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                value_stack_push(&vm->stack, entry->value);
                break;
            }
            case OP_GET_GLOBAL_LONG: {
                ObjString *var_name = read_string_long(vm);
                Entry *entry = NULL;
                if (!table_get(&vm->globals, var_name, &entry)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                value_stack_push(&vm->stack, entry->value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString *var_name = read_string(vm);
                const bool is_const = read_byte(vm);
                table_set(&vm->globals, var_name, value_stack_pop(&vm->stack),
                    is_const ? ENTRY_CONST : ENTRY_NO_FLAGS);
                break;
            }
            case OP_DEFINE_GLOBAL_LONG: {
                ObjString *var_name = read_string_long(vm);
                const bool is_const = read_byte(vm);
                table_set(&vm->globals, var_name, value_stack_pop(&vm->stack),
                    is_const ? ENTRY_CONST : ENTRY_NO_FLAGS);
                break;
            }
            case OP_SET_LOCAL: {
                const uint8_t slot = read_byte(vm);
                vm->stack.array.values[slot] = value_stack_peek(&vm->stack, 0);
                break;
            }
            case OP_SET_LOCAL_LONG: {
                const size_t slot = read_bytes_long(vm);
                vm->stack.array.values[slot] = value_stack_peek(&vm->stack, 0);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *var_name = read_string(vm);
                Entry *entry = NULL;
                if (!table_get(&vm->globals, var_name, &entry)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if ((entry->flags & ENTRY_CONST) == 0) {
                    vm_runtime_error(
                        vm,
                        "Unable to assign to a constant variable '%s'. "
                        "Consider declaring variable with 'var' keyword to allow for assignment",
                        var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                entry->value = value_stack_peek(&vm->stack, 0);
                break;
            }
            case OP_SET_GLOBAL_LONG: {
                ObjString *var_name = read_string_long(vm);
                Entry *entry = NULL;
                if (!table_get(&vm->globals, var_name, &entry)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if ((entry->flags & ENTRY_CONST) == 0) {
                    vm_runtime_error(
                        vm,
                        "Unable to assign to a constant variable '%s'. "
                        "Consider declaring variable with 'var' keyword to allow for assignment",
                        var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                entry->value = value_stack_peek(&vm->stack, 0);
                break;
            }
            case OP_EQUAL: {
                 const Value a = value_stack_pop(&vm->stack);
                 const Value b = value_stack_pop(&vm->stack);
                 value_stack_push(&vm->stack, BOOL_VAL(value_equals(a, b)));
                 break;
            }
            case OP_NOT_EQUAL: {
                 const Value a = value_stack_pop(&vm->stack);
                 const Value b = value_stack_pop(&vm->stack);
                 value_stack_push(&vm->stack, BOOL_VAL(!value_equals(a, b)));
                 break;
            }
            case OP_GREATER:       BINARY_OP(BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
            case OP_LESS:          BINARY_OP(BOOL_VAL, <); break;
            case OP_LESS_EQUAL:    BINARY_OP(BOOL_VAL, <=); break;
            case OP_ADD: {
                if (IS_STRING(value_stack_peek(&vm->stack, 0))
                    && IS_STRING(value_stack_peek(&vm->stack, 1)))
                {
                    const ObjString *b = AS_STRING(value_stack_pop(&vm->stack));
                    const ObjString *a = AS_STRING(value_stack_pop(&vm->stack));
                    const ObjString *result = object_string_concatenate(vm, a, b);
                    value_stack_push(&vm->stack, OBJ_VAL(result));
                }
                else if (IS_NUMBER(value_stack_peek(&vm->stack, 0))
                    && IS_NUMBER(value_stack_peek(&vm->stack, 1)))
                {
                    const double b = AS_NUMBER(value_stack_pop(&vm->stack));
                    const double a = AS_NUMBER(value_stack_pop(&vm->stack));
                    value_stack_push(&vm->stack, NUMBER_VAL(a + b));
                }
                else {
                    vm_runtime_error(vm, "Operands for '+' must either both be numbers, or both be strings");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MUL: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIV: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT: {
                value_stack_push(&vm->stack, BOOL_VAL(value_is_falsey(value_stack_pop(&vm->stack))));
                break;
            }
            case OP_NEGATE: {
                if (!IS_NUMBER(value_stack_peek(&vm->stack, 0))) {
                    vm_runtime_error(vm, "Negation operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->stack.array.values[vm->stack.array.count - 1].as.number *= -1;
                break;
            }
            case OP_PRINT: {
                value_print(value_stack_pop(&vm->stack));
                printf("\n");
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
            default:
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef BINARY_OP
}

InterpretResult vm_interpret(VM *vm, const char *source) {
    Chunk *chunk = malloc(sizeof(Chunk));
    if (chunk == NULL) {
        fprintf(stderr, "Unable to allocate memory for chunk\n");
        exit(EXIT_FAILURE);
    }

    chunk_init(chunk);

    if (!compile(vm, source, chunk)) {
        chunk_free(chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = chunk;
    vm->ip = vm->chunk->code;

    vm_run(vm);

    chunk_free(chunk);
    free(chunk);

    vm->chunk = NULL;
    vm->ip = NULL;
    return INTERPRET_OK;
}