#include "debug.h"

#include <stdio.h>

#include "vm.h"

static size_t disassemble_byte_instruction(const char *name, const Chunk *chunk, const size_t offset) {
    const uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static size_t disassemble_byte_instruction_long(const char *name, const Chunk *chunk, const size_t offset) {
    const size_t slot =
        (size_t)chunk->code[offset + 1]
        | (size_t)chunk->code[offset + 2] << 8
        | (size_t)chunk->code[offset + 3] << 16;
    printf("%-16s %4zu\n", name, slot);
    return offset + 4;
}

static size_t disassemble_op_define_global(const Chunk *chunk, const size_t offset) {
    const uint8_t constant_index = chunk->code[offset + 1];
    const uint8_t is_const = chunk->code[offset + 2];
    printf("%-16s %4d ", "OP_DEFINE_GLOBAL", constant_index);
    value_print(chunk->constants.values[constant_index]);
    printf(" %s\n", is_const == VM_GLOBAL_VAR_MUT ? "MUTABLE" : "CONST");
    return offset + 3;
}

static size_t disassemble_op_define_global_long(const Chunk *chunk, const size_t offset) {
    const size_t constant_index =
        (size_t)chunk->code[offset + 1]
        | (size_t)chunk->code[offset + 2] << 8
        | (size_t)chunk->code[offset + 3] << 16;
    const uint8_t is_const = chunk->code[offset + 4];
    printf("%-16s %4zu ", "OP_DEFINE_GLOBAL", constant_index);
    value_print(chunk->constants.values[constant_index]);
    printf(" %s\n", is_const == VM_GLOBAL_VAR_MUT ? "MUTABLE" : "CONST");
    return offset + 5;
}


static size_t disassemble_op_constant(const char *name, const Chunk *chunk, const size_t offset) {
    const uint8_t constant_index = chunk->code[offset + 1];
    printf("%-16s %4d ", name, constant_index);
    value_print(chunk->constants.values[constant_index]);
    printf("\n");
    return offset + 2;
}

static size_t disassemble_op_constant_long(const char *name, const Chunk *chunk, const size_t offset) {
    const size_t constant_index =
        (size_t)chunk->code[offset + 1]
        | (size_t)chunk->code[offset + 2] << 8
        | (size_t)chunk->code[offset + 3] << 16;
    printf("%-16s %4zu ", name, constant_index);
    value_print(chunk->constants.values[constant_index]);
    printf("\n");
    return offset + 4;
}

static size_t disassemble_op_jump(const char *name, const int sign, const Chunk *chunk, const size_t offset) {
    uint16_t jump = chunk->code[offset + 1];
    jump |= (uint16_t)chunk->code[offset + 2] << 8;
    printf("%-16s %4zu -> %zi\n", name, offset, (ssize_t)(offset + 3) + (ssize_t)(sign * jump));
    return offset + 3;
}

static size_t disassemble_op_simple(const char *name, const size_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

static size_t disassemble_op_closure_upvalues(const ObjFunction *function, const Chunk *chunk, size_t offset) {
    for (size_t i = 0; i < function->upvalue_count; i++) {
        const uint8_t upvalue_op = chunk->code[offset++];
        if (upvalue_op == VM_UPVALUE_LOCAL) {
            const uint8_t index = chunk->code[offset++];
            printf("%04zu    |                     local %d\n", offset - 2, index);
        } else if (upvalue_op == VM_UPVALUE_UPVALUE) {
            const uint8_t index = chunk->code[offset++];
            printf("%04zu    |                     upvalue %d\n", offset - 2, index);
        } else if (upvalue_op == VM_UPVALUE_LOCAL_LONG) {
            const size_t index =
                (size_t)chunk->code[offset]
                | (size_t)chunk->code[offset + 1] << 8
                | (size_t)chunk->code[offset + 2] << 16;
            offset += 3;
            printf("%04zu    |                     local %zu\n", offset - 4, index);
        } else if (upvalue_op == VM_UPVALUE_UPVALUE_LONG) {
            const size_t index =
                (size_t)chunk->code[offset]
                | (size_t)chunk->code[offset + 1] << 8
                | (size_t)chunk->code[offset + 2] << 16;
            offset += 3;
            printf("%04zu    |                     upvalue %zu\n", offset - 4, index);
        } else {
            printf("Unknown upvalue op: %d", upvalue_op);
        }
    }

    return offset;
}

static size_t disassemble_op_closure(const Chunk *chunk, size_t offset) {
    offset++;
    const uint8_t function_index = chunk->code[offset++];
    printf("%-16s %4d ", "OP_CLOSURE", function_index);
    value_print(chunk->constants.values[function_index]);
    printf("\n");

    const ObjFunction *function = AS_FUNCTION(chunk->constants.values[function_index]);
    return disassemble_op_closure_upvalues(function, chunk, offset);
}

static size_t disassemble_op_closure_long(const Chunk *chunk, size_t offset) {
    offset++;
    const size_t function_index =
        (size_t)chunk->code[offset]
        | (size_t)chunk->code[offset + 1] << 8
        | (size_t)chunk->code[offset + 2] << 16;
    offset += 3;

    printf("%-16s %4zu ", "OP_CLOSURE_LONG", function_index);
    value_print(chunk->constants.values[function_index]);
    printf("\n");

    const ObjFunction *function = AS_FUNCTION(chunk->constants.values[function_index]);
    return disassemble_op_closure_upvalues(function, chunk, offset);
}

size_t disassemble_instruction(const Chunk *chunk, const LineView *view, const size_t offset) {
    printf("%04zu ", offset);
    if (offset != 0
        && (view->array->lines[view->previous_index].line_number ==
            view->array->lines[view->index].line_number)) {
        printf("   | ");
    } else {
        printf("%04zu ", line_view_get_current(view));
    }

    const uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return disassemble_op_constant("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return disassemble_op_constant_long("OP_CONSTANT_LONG", chunk, offset);
        case OP_NIL:
            return disassemble_op_simple("OP_NIL", offset);
        case OP_TRUE:
            return disassemble_op_simple("OP_TRUE", offset);
        case OP_FALSE:
            return disassemble_op_simple("OP_FALSE", offset);
        case OP_POP:
            return disassemble_op_simple("OP_POP", offset);
        case OP_GET_LOCAL:
            return disassemble_byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_GET_LOCAL_LONG:
            return disassemble_byte_instruction_long("OP_GET_LOCAL_LONG", chunk, offset);
        case OP_GET_UPVALUE:
            return disassemble_byte_instruction("OP_GET_UP_VALUE", chunk, offset);
        case OP_GET_UPVALUE_LONG:
            return disassemble_byte_instruction_long("OP_GET_UP_VALUE_LONG", chunk, offset);
        case OP_GET_PROPERTY:
            return disassemble_op_constant("OP_GET_PROPERTY", chunk, offset);
        case OP_GET_PROPERTY_LONG:
            return disassemble_op_constant_long("OP_GET_PROPERTY_LONG", chunk, offset);
        case OP_GET_GLOBAL:
            return disassemble_op_constant("OP_GET_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL_LONG:
            return disassemble_op_constant_long("OP_GET_GLOBAL_LONG", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return disassemble_op_define_global(chunk, offset);
        case OP_DEFINE_GLOBAL_LONG:
            return disassemble_op_define_global_long(chunk, offset);
        case OP_SET_LOCAL:
            return disassemble_byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_SET_LOCAL_LONG:
            return disassemble_byte_instruction_long("OP_SET_LOCAL_LONG", chunk, offset);
        case OP_SET_UPVALUE:
            return disassemble_byte_instruction("OP_SET_UP_VALUE", chunk, offset);
        case OP_SET_UPVALUE_LONG:
            return disassemble_byte_instruction_long("OP_SET_UP_VALUE_LONG", chunk, offset);
        case OP_SET_PROPERTY:
            return disassemble_op_constant("OP_SET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY_LONG:
            return disassemble_op_constant_long("OP_SET_PROPERTY_LONG", chunk, offset);
        case OP_SET_GLOBAL:
            return disassemble_op_constant("OP_SET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL_LONG:
            return disassemble_op_constant_long("OP_SET_GLOBAL_LONG", chunk, offset);
        case OP_EQUAL:
            return disassemble_op_simple("OP_EQUAL", offset);
        case OP_NOT_EQUAL:
            return disassemble_op_simple("OP_NOT_EQUAL", offset);
        case OP_GREATER:
            return disassemble_op_simple("OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return disassemble_op_simple("OP_GREATER_EQUAL", offset);
        case OP_LESS:
            return disassemble_op_simple("OP_LESS", offset);
        case OP_LESS_EQUAL:
            return disassemble_op_simple("OP_LESS_EQUAL", offset);
        case OP_ADD:
            return disassemble_op_simple("OP_ADD", offset);
        case OP_SUB:
            return disassemble_op_simple("OP_SUB", offset);
        case OP_MUL:
            return disassemble_op_simple("OP_MUL", offset);
        case OP_DIV:
            return disassemble_op_simple("OP_DIV", offset);
        case OP_NOT:
            return disassemble_op_simple("OP_NOT", offset);
        case OP_NEGATE:
            return disassemble_op_simple("OP_NEGATE", offset);
        case OP_PRINT:
            return disassemble_op_simple("OP_PRINT", offset);
        case OP_JUMP:
            return disassemble_op_jump("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return disassemble_op_jump("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return disassemble_op_jump("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return disassemble_byte_instruction("OP_CALL", chunk, offset);
        case OP_CLOSURE:
            return disassemble_op_closure(chunk, offset);
        case OP_CLOSURE_LONG:
            return disassemble_op_closure_long(chunk, offset);
        case OP_CLOSE_UPVALUE:
            return disassemble_op_simple("OP_CLOSE_UPVALUE", offset);
        case OP_DUP:
            return disassemble_op_simple("OP_DUP", offset);
        case OP_RETURN:
            return disassemble_op_simple("OP_RETURN", offset);
        case OP_CLASS:
            return disassemble_op_constant("OP_CLASS", chunk, offset);
        case OP_CLASS_LONG:
            return disassemble_op_constant_long("OP_CLASS_LONG", chunk, offset);
        default:
            printf("Unknown instruction %d\n", instruction);
            return offset + 1;
    }
}

void disassemble_chunk(const Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);
    LineView view = line_view_init(&chunk->lines);
    for (size_t offset = 0; offset < chunk->count;) {
        const size_t new_offset = disassemble_instruction(chunk, &view, offset);
        line_view_advance(&view, new_offset - offset);
        offset = new_offset;
    }
}