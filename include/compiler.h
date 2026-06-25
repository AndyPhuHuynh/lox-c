#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include <stdbool.h>

#include "chunk.h"
#include "object.h"
#include "vm.h"

ObjFunction *compile(VM *vm, const char *source);

#endif // CLOX_COMPILER_H
