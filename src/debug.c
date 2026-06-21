#include "debug.h"

#include <stdio.h>

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

static size_t disassemble_op_simple(const char *name, const size_t offset) {
    printf("%s\n", name);
    return offset + 1;
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
        case OP_DEFINE_GLOBAL:
            return disassemble_op_constant("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL_LONG:
            return disassemble_op_constant_long("OP_DEFINE_GLOBAL_LONG", chunk, offset);
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
        case OP_RETURN:
            return disassemble_op_simple("OP_RETURN", offset);
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