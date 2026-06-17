#include "chunk.h"
#include "debug.h"

int main() {
    Chunk chunk;
    chunk_init(&chunk);

    for (size_t i = 0; i <= 256; i++) {
        chunk_write_constant(&chunk, (Value)i, i);
    }

    chunk_write(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");

    chunk_free(&chunk);
    return 0;
}