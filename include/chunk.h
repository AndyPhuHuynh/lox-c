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
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_GET_LOCAL_LONG,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_LONG,
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_SET_LOCAL,
    OP_SET_LOCAL_LONG,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_LONG,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
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

size_t chunk_write_constant      (Chunk *chunk, Value constant);
void   chunk_write_constant_op   (Chunk *chunk, uint8_t short_op, uint8_t long_op, size_t constant_index, size_t line);
void   chunk_write_load_constant (Chunk *chunk, size_t index, size_t line);

#endif // CLOX_CHUNK_H