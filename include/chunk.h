#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include <stdint.h>

#include "value.h"

typedef struct {
    size_t line_number;
    size_t repeat_count;
} Line;

typedef struct {
    size_t count;
    size_t capacity;
    Line *lines;
} LineArray;

typedef struct {
    const LineArray *array;
    size_t index;
    size_t previous_index;
    size_t repeats_encountered;
} LineView;

void   line_array_init(LineArray *array);
void   line_array_free(LineArray *array);
void   line_array_write(LineArray *array, size_t line_number);
size_t line_array_get(const LineArray *array, size_t instruction_offset);

LineView line_view_init(const LineArray *array);
size_t   line_view_get_current(const LineView *view);
void     line_view_advance(LineView *view, size_t increment);

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEGATE,
    OP_RETURN
} OpCode;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t *code;
    LineArray lines;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, size_t line);

void chunk_write_constant(Chunk *chunk, Value constant, size_t line);

#endif // CLOX_CHUNK_H