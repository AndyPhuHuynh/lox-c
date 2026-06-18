#include "chunk.h"
#include "vm.h"

#include <stdio.h>

int main(void) {
    VM vm;
    vm_init(&vm);

    Chunk chunk;
    chunk_init(&chunk);

    chunk_write_constant(&chunk, 1.2, 0);
    chunk_write_constant(&chunk, 3.4, 1);
    chunk_write(&chunk, OP_ADD, 2);
    chunk_write_constant(&chunk, 5.6, 3);
    chunk_write(&chunk, OP_DIV, 4);
    chunk_write(&chunk, OP_NEGATE, 5);
    chunk_write(&chunk, OP_RETURN, 6);
    vm_interpret(&vm, &chunk);

    chunk_free(&chunk);
    vm_free(&vm);

    return 0;
}