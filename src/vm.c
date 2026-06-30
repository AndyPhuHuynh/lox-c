#include "vm.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "debug.h" // IWYU pragma: keep
#include "memory.h"
#include "object.h"
#include "value.h"

static void vm_runtime_error(VM *vm, const char *format, ...) {
    const CallFrame *current_frame = call_stack_peek(&vm->call_stack);

    const size_t instruction = (size_t)(current_frame->ip - current_frame->closure->function->chunk.code - 1);
    const size_t line = line_array_get(&current_frame->closure->function->chunk.lines, instruction);
    fprintf(stderr, "\n[line %zu] in script: ", line);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    fprintf(stderr, "CALLSTACK:\n");
    for (size_t i = vm->call_stack.count; i-- > 0;) {
        const CallFrame *frame = &vm->call_stack.frames[i];
        const ObjFunction *function = frame->closure->function;
        const size_t err_instruction = (size_t)(frame->ip - function->chunk.code - 1);
        fprintf(stderr, "    [line %zu] in ",
            line_array_get(&function->chunk.lines, err_instruction));
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    value_stack_free(&vm->stack);
    value_stack_init(&vm->stack);
}

static void define_native(VM *vm, const char *name, const NativeFn function, const size_t arity) {
    value_stack_push(&vm->stack, OBJ_VAL(object_string_copy(vm, name, strlen(name))));
    ObjString *name_str = AS_STRING(value_stack_peek(&vm->stack, 0));

    value_stack_push(&vm->stack, OBJ_VAL(object_native_new(vm, function, name_str, arity)));
    const Value func = value_stack_peek(&vm->stack, 0);

    table_set(&vm->globals, name_str, func, VM_GLOBAL_VAR_CONST);
    value_stack_pop_n(&vm->stack, 2);
}

static bool native_clock(const VM *vm, const Value *values, const size_t count, Value *out) {
    (void)vm; (void)values; (void)count;
    *out = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return true;
}

static bool native_print(const VM *vm, const Value *values, const size_t count, Value *out) {
    (void)vm;
    for (size_t i = 0; i < count; i++) {
        value_print(values[i]);
    }
    printf("\n");
    *out = NIL_VAL;
    return true;
}

static bool native_sqrt(VM *vm, const Value *values, const size_t count, Value *out) {
    (void)count;
    if (!IS_NUMBER(values[0])) {
        vm_runtime_error(vm, "First argument to sqrt must be a number");
        return false;
    }
    *out = NUMBER_VAL(sqrt(AS_NUMBER(values[0])));
    return true;
}

void call_stack_init(CallStack *stack) {
    stack->count = 0;
    stack->capacity = 0;
    stack->frames = NULL;
}

void call_stack_free(CallStack *stack) {
    CLOX_FREE_ARRAY_RAW(CallFrame, stack->frames);
    call_stack_init(stack);
}

void call_stack_push(CallStack *stack, const CallFrame frame) {
    if (stack->capacity < stack->count + 1) {
        stack->capacity = CLOX_GROW_CAPACITY(stack->capacity);
        stack->frames = CLOX_RESIZE_ARRAY_RAW(CallFrame, stack->frames, stack->capacity);
    }

    stack->frames[stack->count] = frame;
    stack->count++;
}

CallFrame *call_stack_peek(const CallStack *stack) {
    if (stack->count == 0) return NULL;
    return &stack->frames[stack->count-1];
}

void call_stack_pop(CallStack *stack) {
    stack->count--;
    if (stack->capacity > 8 && stack->count <= stack->capacity / 4) {
        stack->capacity = CLOX_SHRINK_CAPACITY(stack->capacity);
        stack->frames = CLOX_RESIZE_ARRAY_RAW(CallFrame, stack->frames, stack->capacity);
    }
}

void vm_init(VM *vm) {
    call_stack_init(&vm->call_stack);
    value_stack_init(&vm->stack);
    table_init(&vm->globals);
    table_init(&vm->strings);
    vm->open_upvalues = NULL;
    vm->objects = NULL;

    vm->current_parser = NULL;
    vm->gray_count = 0;
    vm->gray_capacity = 0;
    vm->gray_stack = NULL;

    define_native(vm, "clock",   (NativeFn)native_clock, 0);
    define_native(vm, "println", (NativeFn)native_print, NATIVE_ARITY_VARIADIC);
    define_native(vm, "sqrt",    (NativeFn)native_sqrt, 1);
}

void vm_free(VM *vm) {
    call_stack_free(&vm->call_stack);
    value_stack_free(&vm->stack);
    table_free(&vm->globals);
    table_free(&vm->strings);
    object_free_all(vm, vm->objects);
    vm->objects = NULL;
}

static uint8_t read_byte(const VM *vm) {
    CallFrame *current = call_stack_peek(&vm->call_stack);
    return *current->ip++;
}

static uint16_t read_short(const VM *vm) {
    CallFrame *current = call_stack_peek(&vm->call_stack);
    uint16_t bytes = current->ip[0];
    bytes |= current->ip[1] << 8;
    current->ip +=2;
    return bytes;
}

static size_t read_bytes_long(const VM *vm) {
    CallFrame *current = call_stack_peek(&vm->call_stack);
    size_t bytes = current->ip[0];
    bytes |= (size_t)current->ip[1] << 8;
    bytes |= (size_t)current->ip[2] << 16;
    current->ip += 3;
    return bytes;
}

static Value *read_constant(const VM *vm) {
    const CallFrame *current = call_stack_peek(&vm->call_stack);
    return &current->closure->function->chunk.constants.values[read_byte(vm)];
}

static Value *read_constant_long(const VM *vm) {
    const CallFrame *current = call_stack_peek(&vm->call_stack);
    return &current->closure->function->chunk.constants.values[read_bytes_long(vm)];
}

static ObjString *read_string(const VM *vm) {
    return AS_STRING(*read_constant(vm));
}

static ObjString *read_string_long(const VM *vm) {
    return AS_STRING(*read_constant_long(vm));
}

static bool call_obj_closure(VM *vm, ObjClosure *closure, const size_t arg_count) {
    if (arg_count != closure->function->arity) {
        vm_runtime_error(vm, "Expected %zu arguments, but got %d arguments when calling '%s'",
            closure->function->arity, arg_count, closure->function->name->chars);
        return false;
    }

    call_stack_push(&vm->call_stack, (CallFrame) {
                        .closure = closure,
                        .ip = closure->function->chunk.code,
                        .slots_start_index = vm->stack.array.count - arg_count - 1
                    });
    return true;
}

static bool call_obj_native(VM *vm, const ObjNative *native, const size_t arg_count) {
    if (native->arity != NATIVE_ARITY_VARIADIC && arg_count != native->arity) {
        vm_runtime_error(vm, "Expected %zu arguments, but got %d arguments when calling '%s'",
            native->arity, arg_count, native->name->chars);
        return false;
    }

    Value result = NIL_VAL;
    const bool success = native->function(
        vm,
        &vm->stack.array.values[vm->stack.array.count - arg_count],
        arg_count,
        &result);

    value_stack_pop_n(&vm->stack, arg_count + 1);
    value_stack_push(&vm->stack, result);
    return success;
}

static bool call_value(VM *vm, const Value callee, const size_t arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE: {
                return call_obj_closure(vm, AS_CLOSURE(callee), arg_count);
            }
            case OBJ_NATIVE: {
                return call_obj_native(vm, AS_NATIVE(callee), arg_count);
            }
            default:
                break;
        }
    }
    vm_runtime_error(vm, "Only functions and classes are callable");
    return false;
}

static ObjUpvalue *capture_upvalue(VM *vm, const size_t stack_index) {
    ObjUpvalue **link = &vm->open_upvalues;
    while (*link != NULL && (*link)->as.stack_index> stack_index) {
        link = &(*link)->next;
    }

    if (*link != NULL && (*link)->as.stack_index == stack_index) {
        return *link;
    }

    ObjUpvalue *created_upvalue = object_upvalue_new(vm, stack_index);
    created_upvalue->next = *link;
    *link = created_upvalue;

    return created_upvalue;
}

static void read_op_closure_upvalues(VM *vm, const ObjClosure *closure) {
    for (size_t i = 0; i < closure->upvalue_count; i++) {
        const uint8_t upvalue_op = read_byte(vm);
        if (upvalue_op == VM_UPVALUE_LOCAL || upvalue_op == VM_UPVALUE_LOCAL_LONG) {
            const size_t index = upvalue_op == VM_UPVALUE_LOCAL ?
                read_byte(vm) : read_bytes_long(vm);
            closure->upvalues[i] = capture_upvalue(vm, call_stack_peek(&vm->call_stack)->slots_start_index + index);
        } else if (upvalue_op == VM_UPVALUE_UPVALUE || upvalue_op == VM_UPVALUE_UPVALUE_LONG) {
            const size_t index = upvalue_op == VM_UPVALUE_UPVALUE ?
                read_byte(vm) : read_bytes_long(vm);
            closure->upvalues[i] = call_stack_peek(&vm->call_stack)->closure->upvalues[index];
        }
    }
}

static void close_upvalues(VM *vm, const size_t stack_index) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->as.stack_index >= stack_index) {
        ObjUpvalue *upvalue = vm->open_upvalues;
        upvalue->as.closed = vm->stack.array.values[upvalue->as.stack_index];
        upvalue->type = UPVALUE_CLOSED;
        vm->open_upvalues = upvalue->next;
    }
}

static Value get_upvalue(const VM *vm, const ObjUpvalue *upvalue) {
    switch (upvalue->type) {
        case UPVALUE_OPEN: {
            return vm->stack.array.values[upvalue->as.stack_index];
        }
        case UPVALUE_CLOSED: {
            return upvalue->as.closed;
        }
    }
    fprintf(stderr, "Invalid type for upvalue");
    return NIL_VAL; // unreachable
}

static void set_upvalue(const VM *vm, ObjUpvalue *upvalue, const Value value) {
    switch (upvalue->type) {
        case UPVALUE_OPEN: {
            vm->stack.array.values[upvalue->as.stack_index] = value;
            return;
        }
        case UPVALUE_CLOSED: {
            upvalue->as.closed = value;
            return;
        }
    }
    fprintf(stderr, "Invalid type for upvalue");
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
    LineView view = line_view_init(&call_stack_peek(&vm->call_stack)->closure->function->chunk.lines);
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

        const size_t old_offset =
            (size_t)(call_stack_peek(&vm->call_stack)->ip -
                     call_stack_peek(&vm->call_stack)->closure->function->chunk.code);
        const size_t new_offset = disassemble_instruction(
            &call_stack_peek(&vm->call_stack)->closure->function->chunk, &view, old_offset);
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
                value_stack_push(&vm->stack, vm->stack.array.values[call_stack_peek(&vm->call_stack)->slots_start_index + slot]);
                break;
            }
            case OP_GET_LOCAL_LONG: {
                const size_t slot = read_bytes_long(vm);
                value_stack_push(&vm->stack, vm->stack.array.values[call_stack_peek(&vm->call_stack)->slots_start_index + slot]);
                break;
            }
            case OP_GET_UPVALUE: {
                const uint8_t slot = read_byte(vm);
                ObjUpvalue **upvalues = call_stack_peek(&vm->call_stack)->closure->upvalues;
                value_stack_push(&vm->stack, get_upvalue(vm, upvalues[slot]));
                break;
            }
            case OP_GET_UPVALUE_LONG: {
                const size_t slot = read_bytes_long(vm);
                ObjUpvalue **upvalues = call_stack_peek(&vm->call_stack)->closure->upvalues;
                value_stack_push(&vm->stack, get_upvalue(vm, upvalues[slot]));
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
                const uint8_t flags = read_byte(vm);
                table_set(&vm->globals, var_name, value_stack_pop(&vm->stack),
                          (flags == VM_GLOBAL_VAR_CONST) ? ENTRY_CONST : ENTRY_NO_FLAGS);
                break;
            }
            case OP_DEFINE_GLOBAL_LONG: {
                ObjString *var_name = read_string_long(vm);
                const uint8_t flags = read_byte(vm);
                table_set(&vm->globals, var_name, value_stack_pop(&vm->stack),
                          (flags == VM_GLOBAL_VAR_CONST) ? ENTRY_CONST : ENTRY_NO_FLAGS);
                break;
            }
            case OP_SET_LOCAL: {
                const uint8_t slot = read_byte(vm);
                vm->stack.array.values[call_stack_peek(&vm->call_stack)->slots_start_index + slot] =
                    value_stack_peek(&vm->stack, 0);
                break;
            }
            case OP_SET_LOCAL_LONG: {
                const size_t slot = read_bytes_long(vm);
                vm->stack.array.values[call_stack_peek(&vm->call_stack)->slots_start_index + slot] =
                    value_stack_peek(&vm->stack, 0);
                break;
            }
            case OP_SET_UPVALUE: {
                const uint8_t slot = read_byte(vm);
                set_upvalue(vm, call_stack_peek(&vm->call_stack)->closure->upvalues[slot], value_stack_peek(&vm->stack, 0));
                break;
            }
            case OP_SET_UPVALUE_LONG: {
                const size_t slot = read_bytes_long(vm);
                set_upvalue(vm, call_stack_peek(&vm->call_stack)->closure->upvalues[slot], value_stack_peek(&vm->stack, 0));
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *var_name = read_string(vm);
                Entry *entry = NULL;
                if (!table_get(&vm->globals, var_name, &entry)) {
                    vm_runtime_error(vm, "Undefined variable '%s'", var_name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if ((entry->flags & ENTRY_CONST) != 0) {
                    vm_runtime_error(
                        vm,
                        "Unable to assign to a constant variable '%s'",
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
                if ((entry->flags & ENTRY_CONST) != 0) {
                    vm_runtime_error(
                        vm,
                        "Unable to assign to a constant variable '%s'",
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
            case OP_JUMP: {
                const uint16_t offset = read_short(vm);
                call_stack_peek(&vm->call_stack)->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                const uint16_t offset = read_short(vm);
                if (value_is_falsey(value_stack_peek(&vm->stack, 0))) {
                    call_stack_peek(&vm->call_stack)->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                const uint16_t offset = read_short(vm);
                call_stack_peek(&vm->call_stack)->ip -= offset;
                break;
            }
            case OP_CALL: {
                const uint8_t arg_count = read_byte(vm);
                if (!call_value(vm, value_stack_peek(&vm->stack, arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_CLOSURE: {
                ObjFunction *function = AS_FUNCTION(*read_constant(vm));
                ObjClosure *closure = object_closure_new(vm, function);
                value_stack_push(&vm->stack, OBJ_VAL(closure));
                read_op_closure_upvalues(vm, closure);
                break;
            }
            case OP_CLOSURE_LONG: {
                ObjFunction *function = AS_FUNCTION(*read_constant_long(vm));
                ObjClosure *closure = object_closure_new(vm, function);
                value_stack_push(&vm->stack, OBJ_VAL(closure));
                read_op_closure_upvalues(vm, closure);
                break;
            }
            case OP_CLOSE_UPVALUE: {
                close_upvalues(vm, vm->stack.array.count - 1);
                value_stack_pop(&vm->stack);
                break;
            }
            case OP_DUP: {
                value_stack_push(&vm->stack, value_stack_peek(&vm->stack, 0));
                break;
            }
            case OP_RETURN: {
                const Value result = value_stack_pop(&vm->stack);

                // Rewind parameters + function off the stack
                const size_t call_stack_start_index = call_stack_peek(&vm->call_stack)->slots_start_index;
                const size_t stack_diff = vm->stack.array.count - call_stack_start_index;
                close_upvalues(vm, call_stack_start_index);
                value_stack_pop_n(&vm->stack, stack_diff);

                // Remove call stack for function
                call_stack_pop(&vm->call_stack);

                // Returning from main script
                if (vm->call_stack.count == 0) {
                    value_stack_pop(&vm->stack); // POP <script> at the beginning of the stack
                    return INTERPRET_OK;
                }

                // Push return value back onto stack
                value_stack_push(&vm->stack, result);
                break;
            }
            default:
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef BINARY_OP
}

InterpretResult vm_interpret(VM *vm, const char *source) {
    ObjFunction *function = compile(vm, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    value_stack_push(&vm->stack, OBJ_VAL(function));
    ObjClosure *closure = object_closure_new(vm, function);
    value_stack_pop(&vm->stack);
    value_stack_push(&vm->stack, OBJ_VAL(closure));

    call_obj_closure(vm, closure, 0);

    return vm_run(vm);
}
