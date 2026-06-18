#include "compiler.h"

#include <stdbool.h>
#include <stdio.h>

#include "scanner.h"

void compile(const char *source) {
    Scanner scanner;
    scanner_init(&scanner, source);

    const size_t line = (size_t)-1;
    while (true) {
        const Token token = scanner_scan_token(&scanner);
        if (token.line == line) {
            printf("%4zu ", token.line);
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, (int)token.length, token.start);
        if (token.type == TOKEN_EOF) break;
    }
}
