#include "memory.h"

#include <stdlib.h>

void *reallocate(void *ptr, const size_t old_size, const size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    if (result == NULL) exit(EXIT_FAILURE);
    return result;
}
