#include "chunk.h"
#include "debug.h"

int main() {
    Chunk chunk;
    chunk_init(&chunk);

    const size_t const_index = chunk_write_constant(&chunk, 123.456);
    chunk_write(&chunk, OP_CONSTANT, 123);
    chunk_write(&chunk,(uint8_t)const_index, 123);

    chunk_write(&chunk, OP_CONSTANT, 55);
    chunk_write(&chunk,(uint8_t)const_index, 55);

    chunk_write(&chunk, OP_CONSTANT, 70);
    chunk_write(&chunk,(uint8_t)const_index, 186);

    chunk_write(&chunk, OP_CONSTANT, 70);
    chunk_write(&chunk,(uint8_t)const_index, 186);

    chunk_write(&chunk, OP_RETURN, 123);

    disassemble_chunk(&chunk, "test chunk");

    chunk_free(&chunk);
    return 0;
}