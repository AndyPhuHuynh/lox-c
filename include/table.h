#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include <stdint.h>

#include "value.h"

typedef struct {
    ObjString *key;
    Value value;
} Entry;

typedef struct {
    size_t count;
    size_t capacity;
    Entry *entries;
} Table;

void table_init (Table *table);
void table_free (Table *table);

bool       table_get         (const Table *table, const ObjString *key, Value *out);
bool       table_set         (Table *table, ObjString *key, Value value);
bool       table_delete      (const Table *table, const ObjString *key);
void       table_add_all     (const Table *from, Table *to);
ObjString *table_find_string (const Table *table, const char *chars, size_t length, uint32_t hash);

#endif // CLOX_TABLE_H
