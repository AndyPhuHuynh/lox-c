#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Could not open file \"%s\"\n", path);
        exit(74);
    }

    fseek(f, 0, SEEK_END);
    const size_t file_size = (size_t)ftell(f);
    rewind(f);

    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\"\n", path);
        exit(74);
    }
    const size_t bytes_read = fread(buffer, sizeof(char), file_size, f);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file \"%s\"\n", path);
        exit(74);
    }
    buffer[bytes_read] = '\0';

    fclose(f);
    return buffer;
}

static void repl(void) {
    VM vm;
    vm_init(&vm);

    char line[1024];
    while (true) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        vm_interpret(&vm, line);
    }

    vm_free(&vm);
}

static void run_file(const char *path) {
    VM vm;
    vm_init(&vm);

    char *source = read_file(path);
    const InterpretResult result = vm_interpret(&vm, source);
    free(source);

    vm_free(&vm);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(const int argc, const char* argv[]) {
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }
    return 0;
}
