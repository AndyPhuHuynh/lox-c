#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "chunk.h"

size_t disassemble_instruction(const Chunk *chunk, const LineView *view, size_t offset);
void   disassemble_chunk(const Chunk *chunk, const char *name);

#endif // CLOX_DEBUG_H
