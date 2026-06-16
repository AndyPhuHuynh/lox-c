#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "chunk.h"

void disassemble_chunk(const Chunk *chunk, const char *name);

#endif // CLOX_DEBUG_H
