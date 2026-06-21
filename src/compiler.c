#include "compiler.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "debug.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

typedef struct {
    Scanner scanner;
    Token previous;
    Token current;
    bool had_error;
    bool panic_mode;

    VM * vm;
    Chunk *chunk;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(Parser *);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static const ParseRule *get_rule(TokenType type);

static void parser_init        (Parser *parser, VM *vm, const char *source, Chunk *chunk);
static void parser_advance     (Parser *parser);
static void parser_consume     (Parser *parser, TokenType type, const char *message);
static bool parser_check       (const Parser *parser, TokenType type);
static bool parser_match       (Parser *parser, TokenType type);
static void parser_synchronize (Parser *parser);

static void parser_error_at          (Parser *parser, const Token *token, const char *message);
static void parser_error_at_previous (Parser *parser, const char *message);
static void parser_error_at_current  (Parser *parser, const char *message);

static void parser_emit_byte     (const Parser *parser, uint8_t byte);
static void parser_emit_constant (const Parser *parser, Value value);
static void parser_emit_return   (const Parser *parser);

static size_t parser_identifier_constant (const Parser *parser, const Token *name);
static size_t parser_parse_variable      (Parser *parser, const char *message);
static void   parser_define_global       (const Parser *parser, size_t constant_index, size_t line);

static void parser_parse_false      (const Parser *parser);
static void parser_parse_true       (const Parser *parser);
static void parser_parse_nil        (const Parser *parser);
static void parser_parse_number     (const Parser *parser);
static void parser_parse_string     (const Parser *parser);
static void parser_parse_grouping   (Parser *parser);
static void parser_parse_unary      (Parser *parser);
static void parser_parse_binary     (Parser *parser);
static void parser_parse_expression (Parser *parser);
static void parser_parse_precedence (Parser *parser, Precedence precedence);

static void parser_parse_declaration          (Parser *parser);
static void parser_parse_statement            (Parser *parser);
static void parser_parse_print                (Parser *parser);
static void parser_parse_expression_statement (Parser *parser);
static void parser_parse_var_declaration      (Parser *parser);

static void parser_end(const Parser *parser);

const ParseRule parse_rules[] = {
    [TOKEN_LEFT_PAREN]    = {parser_parse_grouping,         NULL,                PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_COMMA]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_DOT]           = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_MINUS]         = {parser_parse_unary,            parser_parse_binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,                          parser_parse_binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_SLASH]         = {NULL,                          parser_parse_binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,                          parser_parse_binary, PREC_FACTOR},
    [TOKEN_BANG]          = {parser_parse_unary,            NULL,                PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,                          parser_parse_binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,                          parser_parse_binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,                          parser_parse_binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,                          parser_parse_binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,                          parser_parse_binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,                          parser_parse_binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_STRING]        = {(ParseFn)parser_parse_string,  NULL,                PREC_NONE},
    [TOKEN_NUMBER]        = {(ParseFn)parser_parse_number,  NULL,                PREC_NONE},
    [TOKEN_AND]           = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_CLASS]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_ELSE]          = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_FALSE]         = {(ParseFn)parser_parse_false,   NULL,                PREC_NONE},
    [TOKEN_FOR]           = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_FUN]           = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_IF]            = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_NIL]           = {(ParseFn)parser_parse_nil,     NULL,                PREC_NONE},
    [TOKEN_OR]            = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_PRINT]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_RETURN]        = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_SUPER]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_THIS]          = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_TRUE]          = {(ParseFn)parser_parse_true,    NULL,                PREC_NONE},
    [TOKEN_VAR]           = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_WHILE]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_ERROR]         = {NULL,                          NULL,                PREC_NONE},
    [TOKEN_EOF]           = {NULL,                          NULL,                PREC_NONE},
};

static const ParseRule * get_rule(const TokenType type) {
    return &parse_rules[type];
}

static void parser_init(Parser *parser, VM *vm, const char *source, Chunk *chunk) {
    scanner_init(&parser->scanner, source);
    parser->had_error = false;
    parser->panic_mode = false;
    parser->vm = vm;
    parser->chunk = chunk;
    parser_advance(parser);
}

static void parser_advance(Parser *parser) {
    parser->previous = parser->current;

    while (true) {
        parser->current = scanner_scan_token(&parser->scanner);
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }
        parser_error_at_current(parser, parser->current.start);
    }
}

void parser_consume(Parser *parser, TokenType type, const char *message) {
    if (parser->current.type == type) {
        parser_advance(parser);
        return;
    }
    parser_error_at_current(parser, message);
}

bool parser_check(const Parser *parser, const TokenType type) {
    return parser->current.type == type;
}

bool parser_match(Parser *parser, TokenType type) {
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

void parser_synchronize(Parser *parser) {
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;

        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
                return;
            default: {}
        }
        parser_advance(parser);
    }
}

static void parser_error_at(Parser *parser, const Token *token, const char *message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;

    fprintf(stderr, "[line %zu] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

static void parser_error_at_previous(Parser *parser, const char *message) {
    parser_error_at(parser, &parser->previous, message);
}

static void parser_error_at_current(Parser *parser, const char *message) {
    parser_error_at(parser, &parser->current, message);
}

static void parser_emit_byte(const Parser *parser, const uint8_t byte) {
    chunk_write(parser->chunk, byte, parser->previous.line);
}

static void parser_emit_constant(const Parser *parser, const Value value) {
    const size_t index = chunk_write_constant(parser->chunk, value);
    chunk_write_constant_op(parser->chunk, OP_CONSTANT, OP_CONSTANT_LONG, index, parser->previous.line);
}

static void parser_emit_return(const Parser *parser) {
    parser_emit_byte(parser, OP_RETURN);
}

static size_t parser_identifier_constant(const Parser *parser, const Token *name) {
    return chunk_write_constant(
        parser->chunk,
        OBJ_VAL(object_string_copy(parser->vm, name->start, name->length)));
}

static size_t parser_parse_variable(Parser *parser, const char *message) {
    parser_consume(parser, TOKEN_IDENTIFIER, message);
    return parser_identifier_constant(parser, &parser->previous);
}

static void parser_define_global(const Parser *parser, const size_t constant_index, const size_t line) {
    chunk_write_constant_op(parser->chunk, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, constant_index, line);
}

static void parser_parse_false(const Parser *parser) {
    parser_emit_byte(parser, OP_FALSE);
}

static void parser_parse_true(const Parser *parser) {
    parser_emit_byte(parser, OP_TRUE);
}

static void parser_parse_nil(const Parser *parser) {
    parser_emit_byte(parser, OP_NIL);
}

static void parser_parse_number(const Parser *parser) {
    const double value = strtod(parser->previous.start, NULL);
    parser_emit_constant(parser, NUMBER_VAL(value));
}

static void parser_parse_string(const Parser *parser) {
    parser_emit_constant(parser, OBJ_VAL(object_string_copy(
        parser->vm, parser->previous.start + 1, parser->previous.length - 2)));
}

static void parser_parse_grouping(Parser *parser) {
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void parser_parse_unary(Parser *parser) {
    const TokenType op_type = parser->previous.type;
    parser_parse_precedence(parser, PREC_UNARY);

    switch (op_type) {
        case TOKEN_MINUS: parser_emit_byte(parser, OP_NEGATE); break;
        case TOKEN_BANG: parser_emit_byte(parser, OP_NOT); break;
        default: {} // unreachable
    }
}

static void parser_parse_binary(Parser *parser) {
    const TokenType op_type = parser->previous.type;
    const ParseRule *rule = get_rule(op_type);
    parser_parse_precedence(parser, rule->precedence + 1);

    switch (op_type) {
        case TOKEN_BANG_EQUAL:    parser_emit_byte(parser, OP_NOT_EQUAL); break;
        case TOKEN_EQUAL_EQUAL:   parser_emit_byte(parser, OP_EQUAL); break;
        case TOKEN_GREATER:       parser_emit_byte(parser, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: parser_emit_byte(parser, OP_GREATER_EQUAL); break;
        case TOKEN_LESS:          parser_emit_byte(parser, OP_LESS); break;
        case TOKEN_LESS_EQUAL:    parser_emit_byte(parser, OP_LESS_EQUAL); break;
        case TOKEN_MINUS:         parser_emit_byte(parser, OP_SUB); break;
        case TOKEN_PLUS:          parser_emit_byte(parser, OP_ADD); break;
        case TOKEN_STAR:          parser_emit_byte(parser, OP_MUL); break;
        case TOKEN_SLASH:         parser_emit_byte(parser, OP_DIV); break;
        default: {} // unreachable
    }
}

static void parser_parse_expression(Parser *parser) {
    parser_parse_precedence(parser, PREC_ASSIGNMENT);
}

static void parser_parse_precedence(Parser *parser, const Precedence precedence) {
    parser_advance(parser);
    const ParseFn prefix_rule = get_rule(parser->previous.type)->prefix;
    if (prefix_rule == NULL) {
        parser_error_at_previous(parser, "Expect expression");
        return;
    }
    prefix_rule(parser);

    while (precedence <= get_rule(parser->current.type)->precedence) {
        parser_advance(parser);
        const ParseFn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(parser);
    }
}

static void parser_parse_declaration(Parser *parser) {
    if (parser_match(parser, TOKEN_VAR)) {
        parser_parse_var_declaration(parser);
    } else {
        parser_parse_statement(parser);
    }

    if (parser->panic_mode) {
        parser_synchronize(parser);
    }
}

static void parser_parse_statement(Parser *parser) {
    if (parser_match(parser, TOKEN_PRINT)) {
        parser_parse_print(parser);
    } else {
        parser_parse_expression_statement(parser);
    }
}

static void parser_parse_print(Parser *parser) {
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after print statement");
    parser_emit_byte(parser, OP_PRINT);
}

static void parser_parse_expression_statement(Parser *parser) {
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression");
    parser_emit_byte(parser, OP_POP);
}

static void parser_parse_var_declaration(Parser *parser) {
    const size_t global_index = parser_parse_variable(parser, "Expect variable name");
    const size_t line = parser->previous.line;

    if (parser_match(parser, TOKEN_EQUAL)) {
        parser_parse_expression(parser);
    } else {
        parser_emit_byte(parser, OP_NIL);
    }
    parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    parser_define_global(parser, global_index, line);
}

static void parser_end(const Parser *parser) {
    parser_emit_return(parser);
}

bool compile(VM * vm, const char *source, Chunk *chunk) {
    Parser parser;
    parser_init(&parser, vm, source, chunk);
    while (!parser_check(&parser, TOKEN_EOF)) {
        parser_parse_declaration(&parser);
    }
    parser_end(&parser);

#ifdef CLOX_DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(chunk, "code");
    }
#endif
    return !parser.had_error;
}
