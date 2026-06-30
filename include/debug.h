#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#define CLOX_DEBUG_PRINT_CODE // Print code after it has been compiled
// #define CLOX_DEBUG_TRACE_EXECUTION // Print bytecode as its being executed in the VM
#define CLOX_DEBUG_STRESS_GC // Ensure the garbage collector is run as much as possible for stress testing
// #define CLOX_DEBUG_LOX_GC // Log whenever the gc is activated

#include "chunk.h"

size_t disassemble_instruction(const Chunk *chunk, const LineView *view, size_t offset);
void   disassemble_chunk(const Chunk *chunk, const char *name);

#endif // CLOX_DEBUG_H
