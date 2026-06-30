#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

void line_array_init(LineArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->lines = NULL;
}

void line_array_free(LineArray *array) {
    CLOX_FREE_ARRAY_RAW(Line, array->lines);
    line_array_init(array);
}

void line_array_write(LineArray *array, const size_t line_number) {
    if (array->capacity == 0) {
        array->capacity = CLOX_GROW_CAPACITY(array->capacity);
        array->lines = CLOX_RESIZE_ARRAY_RAW(Line, array->lines, array->capacity);

        array->lines[0] = (Line){
            .line_number = line_number,
            .repeat_count = 1
        };
        array->count = 1;
        return;
    }
    if (array->lines[array->count - 1].line_number == line_number) {
        array->lines[array->count - 1].repeat_count++;
        return;
    }
    if (array->capacity < array->count + 1) {
        array->capacity = CLOX_GROW_CAPACITY(array->capacity);
        array->lines = CLOX_RESIZE_ARRAY_RAW(Line, array->lines, array->capacity);
    }
    array->lines[array->count] = (Line){
        .line_number = line_number,
        .repeat_count = 1
    };
    array->count++;
}

size_t line_array_get(const LineArray *array, const size_t instruction_offset) {
    size_t count = 0;
    size_t index = 0;
    while (index < array->count && count < instruction_offset) {
        const size_t repeat_count = array->lines[index].repeat_count;
        if (count + repeat_count > instruction_offset) {
            return array->lines[index].line_number;
        }
        count += repeat_count;
        index++;
    }
    return (size_t)-1;
}

LineView line_view_init(const LineArray *array) {
    return (LineView){
        .array = array,
        .index = 0,
        .previous_index = 0,
        .repeats_encountered = 0,
    };
}

size_t line_view_get_current(const LineView *view) {
    return view->array->lines[view->index].line_number;
}

void line_view_advance(LineView *view, size_t increment) {
    view->previous_index = view->index;
    while (increment != 0) {
        view->repeats_encountered++;
        if (view->repeats_encountered >= view->array->lines[view->index].repeat_count) {
            view->index++;
            view->repeats_encountered = 0;
        }
        increment--;
    }
}

void chunk_init(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    line_array_init(&chunk->lines);
    value_array_init(&chunk->constants);
}

void chunk_free(Chunk *chunk) {
    CLOX_FREE_ARRAY_RAW(uint8_t, chunk->code);
    value_array_free(&chunk->constants);
    chunk_init(chunk);
}

void chunk_write(Chunk *chunk, const uint8_t byte, const size_t line) {
    if (chunk->capacity < chunk->count + 1) {
        chunk->capacity = CLOX_GROW_CAPACITY(chunk->capacity);
        chunk->code = CLOX_RESIZE_ARRAY_RAW(uint8_t, chunk->code, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->count++;
    line_array_write(&chunk->lines, line);
}

size_t chunk_write_constant(Chunk *chunk, const Value constant) {
    value_array_write(&chunk->constants, constant);
    const size_t index = chunk->constants.count - 1;
    return index;
}

void chunk_write_short_or_long_op(
    Chunk *chunk,
    const uint8_t short_op,
    const uint8_t long_op,
    const size_t constant_index, const size_t line
) {
    if (constant_index <= 255) {
        chunk_write(chunk, short_op, line);
        chunk_write(chunk, (uint8_t)(constant_index & 0xFF), line);
    } else if (constant_index <= MAX_24_BIT_NUM) {
        chunk_write(chunk, long_op, line);
        chunk_write(chunk, (uint8_t)(constant_index & 0xFF), line);
        chunk_write(chunk, (uint8_t)(constant_index >> 8 & 0xFF), line);
        chunk_write(chunk, (uint8_t)(constant_index >> 16 & 0xFF), line);
    } else {
        fprintf(stderr, "Out of bounds operand: %zu for op %d\n", constant_index, long_op);
        exit(EXIT_FAILURE);
    }
}
