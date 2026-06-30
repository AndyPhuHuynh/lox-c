#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include <stdbool.h>

#include "object.h"
#include "scanner.h"

typedef struct {
    size_t count;
    size_t capacity;
    size_t *items;
} JumpArray;

typedef struct {
    size_t index;
    bool is_local;
    bool is_const;
} Upvalue;

typedef struct {
    size_t count;
    size_t capacity;
    Upvalue *items;
} UpvalueArray;

typedef struct {
    Token name;
    size_t depth;
    bool is_captured;
    bool is_const;
} Local;

typedef struct {
    size_t count;
    size_t capacity;
    Local *items;
} LocalStack;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    FunctionType type;

    UpvalueArray upvalues;
    LocalStack locals;
    size_t scope_depth;

    size_t enclosing_continue_offset;
    bool in_breakable_scope;
    JumpArray breaks_to_resolve;
} Compiler;

typedef struct Parser {
    Scanner scanner;
    Token previous;
    Token current;
    bool had_error;
    bool panic_mode;

    VM * vm;
    Compiler *compiler;
} Parser;


ObjFunction *compile(VM *vm, const char *source);

#endif // CLOX_COMPILER_H
