#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "value.h"

#include <stdint.h>

#define ENTRY_NO_FLAGS 0
#define ENTRY_CONST    1 << 0

typedef struct {
    ObjString *key;
    Value value;
    uint8_t flags;
} Entry;

typedef struct Table {
    size_t count;
    size_t capacity;
    Entry *entries;
} Table;

void table_init (Table *table);
void table_free (Table *table);

bool       table_get         (const Table *table, const ObjString *key, Entry **out);
bool       table_set         (Table *table, ObjString *key, Value value, uint8_t flags);
bool       table_delete      (const Table *table, const ObjString *key);
void       table_add_all     (const Table *from, Table *to);
ObjString *table_find_string (const Table *table, const char *chars, size_t length, uint32_t hash);

#endif // CLOX_TABLE_H
