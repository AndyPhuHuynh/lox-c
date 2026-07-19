#include "scanner.h"

#include <stdbool.h>
#include <string.h>

static bool is_alpha(const char c) {
    return ('a' <= c && c <= 'z')
        || ('A' <= c && c <= 'Z')
        || c == '_';
}

static bool is_digit(const char c) {
    return '0' <= c && c <= '9';
}

static Token scanner_token_make(const Scanner *scanner, TokenType type);
static Token scanner_token_error(const Scanner *scanner, const char *message);

static bool scanner_is_at_end(const Scanner *scanner) {
    return *scanner->current == '\0';
}

static char scanner_peek(const Scanner *scanner) {
    return *scanner->current;
}

static char scanner_peek_next(const Scanner *scanner) {
    if (scanner_is_at_end(scanner)) {
        return '\0';
    }
    return scanner->current[1];
}

static char scanner_advance(Scanner *scanner) {
    return *scanner->current++;
}

static bool scanner_match(Scanner *scanner, const char expected) {
    if (scanner_is_at_end(scanner)) return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static void scanner_skip_whitespace(Scanner *scanner) {
    while (true) {
        const char c = scanner_peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                scanner_advance(scanner);
                break;
            case '\n':
                scanner->line++;
                scanner_advance(scanner);
                break;
            case '/':
                if (scanner_peek_next(scanner) == '/') {
                    while (scanner_peek(scanner) != '\n' && !scanner_is_at_end(scanner)) {
                        scanner_advance(scanner);
                    }
                    break;
                }
                return;
            default:
                return;
        }
    }
}

static Token scanner_scan_string(Scanner *scanner) {
    while (scanner_peek(scanner) != '"' && !scanner_is_at_end(scanner)) {
        if (scanner_peek(scanner) == '\n') scanner->line++;
        scanner_advance(scanner);
    }
    if (scanner_is_at_end(scanner)) {
        return scanner_token_error(scanner, "Unterminated string");
    }
    scanner_advance(scanner);
    return scanner_token_make(scanner, TOKEN_STRING);
}

static Token scanner_scan_number(Scanner *scanner) {
    while (is_digit(scanner_peek(scanner))) {
        scanner_advance(scanner);
    }

    if (scanner_peek(scanner) == '.' && is_digit(scanner_peek_next(scanner))) {
        scanner_advance(scanner);
        while (is_digit(scanner_peek(scanner))) {
            scanner_advance(scanner);
        }
    }

    return scanner_token_make(scanner, TOKEN_NUMBER);
}

static TokenType scanner_check_keyword(
    const Scanner *scanner,
    const size_t start_len,
    const size_t rest_len,
    const char *rest,
    const TokenType type)
{
    if ((size_t)(scanner->current - scanner->start) == start_len + rest_len
        && memcmp(scanner->start + start_len, rest, rest_len) == 0)
    {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType scanner_get_identifier_type(const Scanner *scanner) {
    switch (scanner->start[0]) {
        case 'a': return scanner_check_keyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'b': return scanner_check_keyword(scanner, 1, 4, "reak", TOKEN_BREAK);
        case 'c': {
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return scanner_check_keyword(scanner, 2, 2, "se", TOKEN_CASE);
                    case 'l': return scanner_check_keyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': return scanner_check_keyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
                    default: return TOKEN_IDENTIFIER;
                }
            }
            return TOKEN_IDENTIFIER;
        }
        case 'd': return scanner_check_keyword(scanner, 1, 6, "efault", TOKEN_DEFAULT);
        case 'e': return scanner_check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f': {
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return scanner_check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return scanner_check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return scanner_check_keyword(scanner, 2, 1, "n", TOKEN_FUN);
                    default: return TOKEN_IDENTIFIER;
                }
            }
            return TOKEN_IDENTIFIER;
        }
        case 'i': return scanner_check_keyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'l': return scanner_check_keyword(scanner, 1, 2, "et", TOKEN_LET);
        case 'n': return scanner_check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o': return scanner_check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': return scanner_check_keyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return scanner_check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's': {
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'u': return scanner_check_keyword(scanner, 2, 3, "per", TOKEN_SUPER);
                    case 'w': return scanner_check_keyword(scanner, 2, 4, "itch", TOKEN_SWITCH);
                    default: return TOKEN_IDENTIFIER;
                }
            }
            return TOKEN_IDENTIFIER;
        }
        case 't': {
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return scanner_check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return scanner_check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                    default: return TOKEN_IDENTIFIER;
                }
            }
            return TOKEN_IDENTIFIER;
        }
        case 'v': return scanner_check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return scanner_check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
        default: return TOKEN_IDENTIFIER;
    }
}

static Token scanner_scan_identifier(Scanner *scanner) {
    while (is_alpha(scanner_peek(scanner)) || is_digit(scanner_peek(scanner))) {
        scanner_advance(scanner);
    }
    return scanner_token_make(scanner, scanner_get_identifier_type(scanner));
}

static Token scanner_token_make(const Scanner *scanner, const TokenType type) {
    return (Token) {
        .type = type,
        .start = scanner->start,
        .length = (size_t)(scanner->current - scanner->start),
        .line = scanner->line
    };
}

static Token scanner_token_error(const Scanner *scanner, const char *message) {
    return (Token) {
        .type = TOKEN_ERROR,
        .start = message,
        .length = strlen(message),
        .line = scanner->line
    };
}

void scanner_init(Scanner *scanner, const char *source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

Token scanner_scan_token(Scanner *scanner) {
    scanner_skip_whitespace(scanner);
    scanner->start = scanner->current;
    if (scanner_is_at_end(scanner)) {
        return scanner_token_make(scanner, TOKEN_EOF);
    }

    const char c = scanner_advance(scanner);

    if (is_alpha(c)) return scanner_scan_identifier(scanner);
    if (is_digit(c)) return scanner_scan_number(scanner);

    switch (c) {
        case '(': return scanner_token_make(scanner, TOKEN_LEFT_PAREN);
        case ')': return scanner_token_make(scanner, TOKEN_RIGHT_PAREN);
        case '{': return scanner_token_make(scanner, TOKEN_LEFT_BRACE);
        case '}': return scanner_token_make(scanner, TOKEN_RIGHT_BRACE);
        case '[': return scanner_token_make(scanner, TOKEN_LEFT_BRACK);
        case ']': return scanner_token_make(scanner, TOKEN_RIGHT_BRACK);
        case ';': return scanner_token_make(scanner, TOKEN_SEMICOLON);
        case ':': return scanner_token_make(scanner, TOKEN_COLON);
        case ',': return scanner_token_make(scanner, TOKEN_COMMA);
        case '.': return scanner_token_make(scanner, TOKEN_DOT);
        case '-': return scanner_token_make(scanner, TOKEN_MINUS);
        case '+': return scanner_token_make(scanner, TOKEN_PLUS);
        case '/': return scanner_token_make(scanner, TOKEN_SLASH);
        case '*': return scanner_token_make(scanner, TOKEN_STAR);

        case '!':
            return scanner_token_make(scanner,
                scanner_match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return scanner_token_make(scanner,
                scanner_match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return scanner_token_make(scanner,
                scanner_match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return scanner_token_make(scanner,
                scanner_match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return scanner_scan_string(scanner);
        default: return scanner_token_error(scanner, "Unexpected character");
    }
}