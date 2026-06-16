#include "debug.h"

#include <stdio.h>

static size_t disassemble_op_constant(const Chunk *chunk, const size_t offset) {
    const uint8_t constant_index = chunk->code[offset + 1];
    printf("%-16s %4d ", "OP_CONSTANT", constant_index);
    value_print(chunk->constants.values[constant_index]);
    printf("\n");
    return offset + 2;
}

static size_t disassemble_op_simple(const size_t offset) {
    printf("%s\n", "OP_RETURN");
    return offset + 1;
}

static size_t disassemble_instruction(const Chunk *chunk, const LineView *view, const size_t offset) {
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
            return disassemble_op_constant(chunk, offset);
        case OP_RETURN:
            return disassemble_op_simple(offset);
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