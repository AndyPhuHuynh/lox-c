#include "compiler.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

#define LOCAL_NOT_FOUND ((size_t)-1)
#define LOCAL_UNINITIALIZED ((size_t)-1)

typedef struct {
    size_t count;
    size_t capacity;
    size_t *items;
} JumpArray;

typedef struct {
    Token name;
    size_t depth;
    bool is_const;
} Local;

typedef struct {
    size_t count;
    size_t capacity;
    Local *locals;
} LocalStack;

typedef struct {
    LocalStack locals;
    size_t scope_depth;
    Chunk *chunk;
} Compiler;

typedef struct {
    Scanner scanner;
    Token previous;
    Token current;
    bool had_error;
    bool panic_mode;

    VM * vm;
    Chunk *chunk;
    Compiler compiler;
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

typedef void (*ParseFn)(Parser *parser, bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static const ParseRule *get_rule(TokenType type);
static bool identifiers_equal(const Token *a, const Token *b);

static void jump_array_init(JumpArray *array);
static void jump_array_free(JumpArray *array);
static void jump_array_push(JumpArray *array, size_t value);

static void local_stack_init (LocalStack *stack);
static void local_stack_free (LocalStack *stack);
static void local_stack_push (LocalStack *stack, Local local);
static void local_stack_pop  (LocalStack *stack);

static void compiler_init (Compiler *compiler, Chunk *chunk);
static void compiler_free (Compiler *compiler);

static size_t parser_resolve_local (Parser *parser, const Token *name);
static void   parser_begin_scope   (Parser *parser);
static void   parser_end_scope     (Parser *parser);

static void parser_init        (Parser *parser, VM *vm, const char *source, Chunk *chunk);
static void parser_free        (Parser *parser);
static void parser_advance     (Parser *parser);
static void parser_consume     (Parser *parser, TokenType type, const char *message);
static bool parser_check       (const Parser *parser, TokenType type);
static bool parser_match       (Parser *parser, TokenType type);
static void parser_synchronize (Parser *parser);

static void parser_error_at          (Parser *parser, const Token *token, const char *message);
static void parser_error_at_previous (Parser *parser, const char *message);
static void parser_error_at_current  (Parser *parser, const char *message);

static void parser_emit_byte      (const Parser *parser, uint8_t byte);
static void parser_emit_constant  (const Parser *parser, Value value);
static void parser_emit_get_local (const Parser *parser, size_t index);
static void parser_emit_set_local (const Parser *parser, size_t index);
static void parser_emit_return    (const Parser *parser);

static size_t parser_emit_jump  (const Parser *parser, uint8_t instruction);
static void   parser_patch_jump (Parser *parser, size_t jump_offset);
static void   parser_emit_loop  (Parser *parser, size_t loop_offset);

static size_t parser_identifier_constant (const Parser *parser, const Token *name);
static size_t parser_parse_variable      (Parser *parser, const char *message, bool is_const);
static void   parser_define_variable     (const Parser *parser, size_t constant_index, size_t line, bool is_const);
static void   parser_declare_variable    (Parser *parser, bool is_const);
static void   parser_named_variable      (Parser *parser, const Token *name, bool can_assign);
static void   parser_add_local           (Parser *parser, Token name, bool is_const);

static void parser_parse_false         (const Parser *parser, bool can_assign);
static void parser_parse_true          (const Parser *parser, bool can_assign);
static void parser_parse_nil           (const Parser *parser, bool can_assign);
static void parser_parse_number        (const Parser *parser, bool can_assign);
static void parser_parse_string        (const Parser *parser, bool can_assign);
static void parser_parse_variable_expr (Parser *parser, bool can_assign);
static void parser_parse_grouping      (Parser *parser, bool can_assign);
static void parser_parse_unary         (Parser *parser, bool can_assign);
static void parser_parse_binary        (Parser *parser, bool can_assign);
static void parser_parse_and           (Parser *parser, bool can_assign);
static void parser_parse_or            (Parser *parser, bool can_assign);
static void parser_parse_expression    (Parser *parser);
static void parser_parse_precedence    (Parser *parser, Precedence precedence);

static void parser_parse_declaration          (Parser *parser);
static void parser_parse_var_declaration      (Parser *parser, bool is_const);
static void parser_parse_statement            (Parser *parser);
static void parser_parse_expression_statement (Parser *parser);
static void parser_parse_for_statement        (Parser *parser);
static void parser_parse_if_statement         (Parser *parser);
static void parser_parse_switch_statement     (Parser *parser);
static void parser_parse_print_statement      (Parser *parser);
static void parser_parse_while_statement      (Parser *parser);
static void parser_parse_block                (Parser *parser);

static void parser_end(const Parser *parser);

const ParseRule parse_rules[] = {
    [TOKEN_LEFT_PAREN]    = {parser_parse_grouping,               NULL,                PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_COMMA]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_DOT]           = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_MINUS]         = {parser_parse_unary,                  parser_parse_binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,                                parser_parse_binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_SLASH]         = {NULL,                                parser_parse_binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,                                parser_parse_binary, PREC_FACTOR},
    [TOKEN_BANG]          = {parser_parse_unary,                  NULL,                PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,                                parser_parse_binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,                                parser_parse_binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,                                parser_parse_binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,                                parser_parse_binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,                                parser_parse_binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,                                parser_parse_binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {(ParseFn)parser_parse_variable_expr, NULL,                PREC_NONE},
    [TOKEN_STRING]        = {(ParseFn)parser_parse_string,        NULL,                PREC_NONE},
    [TOKEN_NUMBER]        = {(ParseFn)parser_parse_number,        NULL,                PREC_NONE},
    [TOKEN_AND]           = {NULL,                                parser_parse_and,    PREC_AND},
    [TOKEN_CLASS]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_ELSE]          = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_FALSE]         = {(ParseFn)parser_parse_false,         NULL,                PREC_NONE},
    [TOKEN_FOR]           = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_FUN]           = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_IF]            = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_NIL]           = {(ParseFn)parser_parse_nil,           NULL,                PREC_NONE},
    [TOKEN_OR]            = {NULL,                                parser_parse_or,     PREC_OR},
    [TOKEN_PRINT]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_RETURN]        = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_SUPER]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_THIS]          = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_TRUE]          = {(ParseFn)parser_parse_true,          NULL,                PREC_NONE},
    [TOKEN_VAR]           = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_WHILE]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_ERROR]         = {NULL,                                NULL,                PREC_NONE},
    [TOKEN_EOF]           = {NULL,                                NULL,                PREC_NONE},
};

static const ParseRule * get_rule(const TokenType type) {
    return &parse_rules[type];
}

static bool identifiers_equal(const Token *a, const Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length * sizeof(char)) == 0;
}

static void jump_array_init(JumpArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->items = NULL;
}

static void jump_array_free(JumpArray *array) {
    CLOX_FREE_ARRAY(size_t, array->items, array->capacity);
    jump_array_init(array);
}

static void jump_array_push(JumpArray *array, const size_t value) {
    if (array->capacity < array->count + 1) {
        const size_t old_capacity = array->capacity;
        array->capacity = CLOX_GROW_CAPACITY(old_capacity);
        array->items = CLOX_RESIZE_ARRAY(size_t, array->items, old_capacity, array->capacity);
    }

    array->items[array->count] = value;
    array->count++;
}

static void local_stack_init(LocalStack *stack) {
    stack->count = 0;
    stack->capacity = 0;
    stack->locals = NULL;
}

static void local_stack_free(LocalStack *stack) {
    CLOX_FREE_ARRAY(Local, stack->locals, stack->capacity);
    local_stack_init(stack);
}

static void local_stack_push(LocalStack *stack, Local local) {
    if (stack->capacity < stack->count + 1) {
        const size_t old_capacity = stack->capacity;
        stack->capacity = CLOX_GROW_CAPACITY(old_capacity);
        stack->locals = CLOX_RESIZE_ARRAY(Local, stack->locals, old_capacity, stack->capacity);
    }

    stack->locals[stack->count] = local;
    stack->count++;
}

static void local_stack_pop(LocalStack *stack) {
    if (stack->count == 0) return;

    stack->count--;
    if (stack->count == stack->capacity / 4) {
        const size_t old_capacity = stack->capacity;
        stack->capacity = stack->capacity / 2;
        stack->locals = CLOX_RESIZE_ARRAY(Local, stack->locals, old_capacity, stack->capacity);
    }
}

static void compiler_init(Compiler *compiler, Chunk *chunk) {
    local_stack_init(&compiler->locals);
    compiler->scope_depth = 0;
    compiler->chunk = chunk;
}

static void compiler_free(Compiler *compiler) {
    local_stack_free(&compiler->locals);
}

static size_t parser_resolve_local(Parser *parser, const Token *name) {
    for (size_t i = parser->compiler.locals.count; i-- > 0;) {
        const Local *local = &parser->compiler.locals.locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == LOCAL_UNINITIALIZED) {
                parser_error_at_previous(parser, "Cannot read local variable in its own initializer");
            }
            return i;
        }
    }
    return LOCAL_NOT_FOUND;
}

static void parser_begin_scope(Parser *parser) {
    parser->compiler.scope_depth++;
}

static void parser_end_scope(Parser *parser) {
    parser->compiler.scope_depth--;

    while (parser->compiler.locals.count > 0
        && parser->compiler.locals.locals[parser->compiler.locals.count - 1].depth > parser->compiler.scope_depth)
    {
        parser_emit_byte(parser, OP_POP);
        local_stack_pop(&parser->compiler.locals);
    }
}

static void parser_init(Parser *parser, VM *vm, const char *source, Chunk *chunk) {
    scanner_init(&parser->scanner, source);
    parser->had_error = false;
    parser->panic_mode = false;
    parser->vm = vm;
    parser->chunk = chunk;
    compiler_init(&parser->compiler, parser->chunk);
    parser_advance(parser);
}

static void parser_free(Parser *parser) {
    compiler_free(&parser->compiler);
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

static void parser_emit_get_local(const Parser *parser, const size_t index) {
    if (index <= UINT8_MAX) {
        parser_emit_byte(parser, OP_GET_LOCAL);
        parser_emit_byte(parser, (uint8_t)index);
    } else {
        parser_emit_byte(parser, OP_GET_LOCAL_LONG);
        parser_emit_byte(parser, (uint8_t)index);
        parser_emit_byte(parser, (uint8_t)(index >> 8));
        parser_emit_byte(parser, (uint8_t)(index >> 16));
    }
}

static void parser_emit_set_local(const Parser *parser, const size_t index) {
    if (index <= UINT8_MAX) {
        parser_emit_byte(parser, OP_SET_LOCAL);
        parser_emit_byte(parser, (uint8_t)index);
    } else {
        parser_emit_byte(parser, OP_SET_LOCAL_LONG);
        parser_emit_byte(parser, (uint8_t)index);
        parser_emit_byte(parser, (uint8_t)(index >> 8));
        parser_emit_byte(parser, (uint8_t)(index >> 16));
    }
}

static void parser_emit_return(const Parser *parser) {
    parser_emit_byte(parser, OP_RETURN);
}

static size_t parser_emit_jump(const Parser *parser, const uint8_t instruction) {
    parser_emit_byte(parser, instruction); 
    parser_emit_byte(parser, 0xFF);
    parser_emit_byte(parser, 0xFF);
    return parser->chunk->count - 2;
}

static void parser_patch_jump(Parser *parser, const size_t jump_offset) {
    // -2 to adjust for the operands
    // The distance should be the distance after the operands to the instruction we want to jump to
    const size_t jump = parser->chunk->count - jump_offset - 2;

    if (jump > UINT16_MAX) {
        parser_error_at_previous(parser, "Too much code to jump over");
    }

    parser->chunk->code[jump_offset]     = (uint8_t)(jump & 0xFF);
    parser->chunk->code[jump_offset + 1] = (uint8_t)((jump >> 8) & 0xFF);
}

static void parser_emit_loop(Parser *parser, size_t loop_offset) {
    parser_emit_byte(parser, OP_LOOP);

    const size_t offset = parser->chunk->count - loop_offset + 2;
    if (offset > UINT16_MAX) {
        parser_error_at_previous(parser, "Loop body too large");
    }

    parser_emit_byte(parser, (uint8_t)(offset & 0xFF));
    parser_emit_byte(parser, (uint8_t)(offset >> 8) & 0xFF);
}

static size_t parser_identifier_constant(const Parser *parser, const Token *name) {
    return chunk_write_constant(
        parser->chunk,
        OBJ_VAL(object_string_copy(parser->vm, name->start, name->length)));
}

static size_t parser_parse_variable(Parser *parser, const char *message, const bool is_const) {
    parser_consume(parser, TOKEN_IDENTIFIER, message);

    parser_declare_variable(parser, is_const);
    if (parser->compiler.scope_depth > 0) return 0;

    return parser_identifier_constant(parser, &parser->previous);
}

static void parser_define_variable(const Parser *parser, const size_t constant_index, const size_t line, const bool is_const) {
    if (parser->compiler.scope_depth > 0) {
        parser->compiler.locals.locals[parser->compiler.locals.count - 1].depth =
            parser->compiler.scope_depth;
        return;
    }
    chunk_write_constant_op(parser->chunk, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, constant_index, line);
    parser_emit_byte(parser, is_const ? VM_GLOBAL_VAR_CONST : VM_GLOBAL_VAR_MUT);
}

void parser_declare_variable(Parser *parser, const bool is_const) {
    if (parser->compiler.scope_depth == 0) return;

    const Token *name = &parser->previous;
    for (size_t i = parser->compiler.locals.count; i-- > 0;) {
        const Local *local = &parser->compiler.locals.locals[i];
        if (local->depth != (size_t)-1 && local->depth < parser->compiler.scope_depth) {
            break;
        }
        if (identifiers_equal(name, &local->name)) {
            parser_error_at_previous(parser, "Already a variable with this name in scope");
        }
    }

    parser_add_local(parser, *name, is_const);
}

static void parser_named_variable(Parser *parser, const Token *name, const bool can_assign) {
    const size_t constant_index = parser_identifier_constant(parser, name);
    const size_t local_index = parser_resolve_local(parser, name);

    if (can_assign && parser_match(parser, TOKEN_EQUAL)) {
        Token equal = parser->previous;

        parser_parse_expression(parser);
        if (local_index == LOCAL_NOT_FOUND) {
            chunk_write_constant_op(parser->chunk, OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, constant_index, name->line);
        } else {
            if (parser->compiler.locals.locals[local_index].is_const) {
                parser_error_at(parser, &equal, "Unable to assign to a constant variable. "
                                                "Consider declaring variable with 'var' keyword to allow for assignment");
                return;
            }
            parser_emit_set_local(parser, local_index);
        }
    } else {
        if (local_index == LOCAL_NOT_FOUND) {
            chunk_write_constant_op(parser->chunk, OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, constant_index, name->line);
        } else {
            parser_emit_get_local(parser, local_index);
        }
    }
}

static void parser_add_local(Parser *parser, const Token name, const bool is_const) {
    local_stack_push(&parser->compiler.locals, (Local){
        .name = name,
        .depth = LOCAL_UNINITIALIZED,
        .is_const = is_const,
    });
}

static void parser_parse_false(const Parser *parser, const bool can_assign) {
    (void)can_assign;
    parser_emit_byte(parser, OP_FALSE);
}

static void parser_parse_true(const Parser *parser, const bool can_assign) {
    (void)can_assign;
    parser_emit_byte(parser, OP_TRUE);
}

static void parser_parse_nil(const Parser *parser, const bool can_assign) {
    (void)can_assign;
    parser_emit_byte(parser, OP_NIL);
}

static void parser_parse_number(const Parser *parser, const bool can_assign) {
    (void)can_assign;
    const double value = strtod(parser->previous.start, NULL);
    parser_emit_constant(parser, NUMBER_VAL(value));
}

static void parser_parse_string(const Parser *parser, const bool can_assign) {
    (void)can_assign;
    parser_emit_constant(parser, OBJ_VAL(object_string_copy(
        parser->vm, parser->previous.start + 1, parser->previous.length - 2)));
}

static void parser_parse_variable_expr(Parser *parser, const bool can_assign) {
    (void)can_assign;
    parser_named_variable(parser, &parser->previous, can_assign);
}

static void parser_parse_grouping(Parser *parser, const bool can_assign) {
    (void)can_assign;
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void parser_parse_unary(Parser *parser, const bool can_assign) {
    (void)can_assign;
    const TokenType op_type = parser->previous.type;
    parser_parse_precedence(parser, PREC_UNARY);

    switch (op_type) {
        case TOKEN_MINUS: parser_emit_byte(parser, OP_NEGATE); break;
        case TOKEN_BANG: parser_emit_byte(parser, OP_NOT); break;
        default: {} // unreachable
    }
}

static void parser_parse_binary(Parser *parser, const bool can_assign) {
    (void)can_assign;
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

static void parser_parse_and(Parser *parser, bool can_assign) {
    (void)can_assign;

    const size_t jump = parser_emit_jump(parser, OP_JUMP_IF_FALSE);

    parser_emit_byte(parser, OP_POP);
    parser_parse_precedence(parser, PREC_AND);

    parser_patch_jump(parser, jump);
}

static void parser_parse_or(Parser *parser, bool can_assign) {
    (void)can_assign;

    const size_t else_jump = parser_emit_jump(parser, OP_JUMP_IF_FALSE);
    const size_t end_jump = parser_emit_jump(parser, OP_JUMP);

    parser_patch_jump(parser, else_jump);
    parser_emit_byte(parser, OP_POP);
    parser_parse_precedence(parser, PREC_OR);

    parser_patch_jump(parser, end_jump);
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

    const bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(parser, can_assign);

    while (precedence <= get_rule(parser->current.type)->precedence) {
        parser_advance(parser);
        const ParseFn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(parser, can_assign);
    }

    if (can_assign && parser_match(parser, TOKEN_EQUAL)) {
        parser_error_at_previous(parser, "Invalid assignment target");
    }
}

static void parser_parse_declaration(Parser *parser) {
    if (parser_match(parser, TOKEN_LET)) {
        parser_parse_var_declaration(parser, true);
    } else if (parser_match(parser, TOKEN_VAR)) {
        parser_parse_var_declaration(parser, false);
    } else {
        parser_parse_statement(parser);
    }

    if (parser->panic_mode) {
        parser_synchronize(parser);
    }
}

static void parser_parse_var_declaration(Parser *parser, const bool is_const) {
    const size_t global_index = parser_parse_variable(parser, "Expect variable name", is_const);
    const size_t line = parser->previous.line;

    if (parser_match(parser, TOKEN_EQUAL)) {
        parser_parse_expression(parser);
    } else {
        parser_emit_byte(parser, OP_NIL);
    }
    parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    parser_define_variable(parser, global_index, line, is_const);
}

static void parser_parse_statement(Parser *parser) {
    if (parser_match(parser, TOKEN_FOR)) {
        parser_parse_for_statement(parser);
    } else if (parser_match(parser, TOKEN_IF)) {
        parser_parse_if_statement(parser);
    } else if (parser_match(parser, TOKEN_SWITCH)) {
        parser_parse_switch_statement(parser);
    } else if (parser_match(parser, TOKEN_PRINT)) {
        parser_parse_print_statement(parser);
    } else if (parser_match(parser, TOKEN_WHILE)) {
        parser_parse_while_statement(parser);
    } else if (parser_match(parser, TOKEN_LEFT_BRACE)) {
        parser_begin_scope(parser);
        parser_parse_block(parser);
        parser_end_scope(parser);
    }
    else {
        parser_parse_expression_statement(parser);
    }
}

static void parser_parse_expression_statement(Parser *parser) {
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression");
    parser_emit_byte(parser, OP_POP);
}


void parser_parse_for_statement(Parser *parser) {
    parser_begin_scope(parser);

    parser_consume(parser, TOKEN_LEFT_PAREN, "Expect '(' before 'for'");
    if (parser_match(parser, TOKEN_SEMICOLON)) {
        // No initializer
    } else if (parser_match(parser, TOKEN_LET)) {
        parser_parse_var_declaration(parser, true);
    } else if (parser_match(parser, TOKEN_VAR)) {
        parser_parse_var_declaration(parser, false);
    } else {
        parser_parse_expression_statement(parser);
    }

    size_t loop_start = parser->chunk->count;
    size_t end_jump = (size_t)-1;
    if (!parser_match(parser, TOKEN_SEMICOLON)) {
        parser_parse_expression(parser);
        parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition");

        end_jump = parser_emit_jump(parser, OP_JUMP_IF_FALSE);
        parser_emit_byte(parser, OP_POP);
    }

    if (!parser_match(parser, TOKEN_RIGHT_PAREN)) {
        const size_t body_jump = parser_emit_jump(parser, OP_JUMP);
        const size_t increment_start = parser->chunk->count;
        parser_parse_expression(parser);
        parser_emit_byte(parser, OP_POP);
        parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for loop clauses");

        parser_emit_loop(parser, loop_start);
        loop_start = increment_start;
        parser_patch_jump(parser, body_jump);
    }


    parser_parse_statement(parser);
    parser_emit_loop(parser, loop_start);

    if (end_jump != (size_t)-1) {
        parser_patch_jump(parser, end_jump);
        parser_emit_byte(parser, OP_POP);
    }

    parser_end_scope(parser);
}

static void parser_parse_if_statement(Parser *parser) {
    parser_consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after if statement");
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");
    
    const size_t then_jump = parser_emit_jump(parser, OP_JUMP_IF_FALSE);

    parser_emit_byte(parser, OP_POP);
    parser_parse_statement(parser);

    const size_t else_jump = parser_emit_jump(parser, OP_JUMP);
    parser_patch_jump(parser, then_jump);

    parser_emit_byte(parser, OP_POP);
    if (parser_match(parser, TOKEN_ELSE)) {
        parser_parse_statement(parser);
    }
    parser_patch_jump(parser, else_jump);
}

static void parser_parse_switch_statement(Parser *parser) {
    parser_consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after switch statement");
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    JumpArray jumps;
    jump_array_init(&jumps);

    parser_consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before switch body");
    while (parser_match(parser, TOKEN_CASE)) {
        parser_emit_byte(parser, OP_DUP);
        parser_parse_expression(parser);
        parser_consume(parser, TOKEN_COLON, "Expect ':' after condition in switch case");
        parser_emit_byte(parser, OP_EQUAL);

        const size_t skip_case = parser_emit_jump(parser, OP_JUMP_IF_FALSE);
        parser_emit_byte(parser, OP_POP); // POP result of equals
        parser_emit_byte(parser, OP_POP); // POP condition of the switch statement

        parser_parse_statement(parser);
        const size_t end_switch = parser_emit_jump(parser, OP_JUMP);
        jump_array_push(&jumps, end_switch);

        parser_patch_jump(parser, skip_case);
        parser_emit_byte(parser, OP_POP); // POP result of equals
    }

    printf("Before default check\n");
    if (parser_match(parser, TOKEN_DEFAULT)) {
        printf("Default token matched!\n");
        parser_emit_byte(parser, OP_POP); // POP condition of the switch statement
        parser_consume(parser, TOKEN_COLON, "Expect ':' after default case");
        parser_parse_statement(parser);

        const size_t end_switch = parser_emit_jump(parser, OP_JUMP);
        jump_array_push(&jumps, end_switch);
    }
    printf("After default check\n");
    parser_consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after switch body");

    parser_emit_byte(parser, OP_POP); // POP condition of the switch statement
    for (size_t i = 0; i < jumps.count; i++) {
        parser_patch_jump(parser, jumps.items[i]);
    }
    jump_array_free(&jumps);
}

static void parser_parse_print_statement(Parser *parser) {
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expect ';' after print statement");
    parser_emit_byte(parser, OP_PRINT);
}

static void parser_parse_while_statement(Parser *parser) {
    const size_t loop_start = parser->chunk->count;

    parser_consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after while");
    parser_parse_expression(parser);
    parser_consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    const size_t end_jump = parser_emit_jump(parser, OP_JUMP_IF_FALSE);
    parser_emit_byte(parser, OP_POP);
    parser_parse_statement(parser);

    parser_emit_loop(parser, loop_start);
    parser_patch_jump(parser, end_jump);
    parser_emit_byte(parser, OP_POP);
}

static void parser_parse_block(Parser *parser) {
    while (!parser_check(parser, TOKEN_RIGHT_BRACE) && !parser_check(parser, TOKEN_EOF)) {
        parser_parse_declaration(parser);
    }
    parser_consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block declaration");
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
    parser_free(&parser);

    const bool had_error = parser.had_error;
#ifdef CLOX_DEBUG_PRINT_CODE
    if (!had_error) {
        disassemble_chunk(chunk, "code");
    }
#endif
    return !had_error;
}

