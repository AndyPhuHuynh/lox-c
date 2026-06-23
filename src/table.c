#include "table.h"

#include <stdbool.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

static Entry *find_entries(Entry *entries, const size_t capacity, const ObjString *key) {
    size_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    while (true) {
        Entry *entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Found an empty entry
                return tombstone != NULL ? tombstone : entry;
            }
            // Found a tombstone
            if (tombstone == NULL) tombstone = entry;
        } else if (entry->key == key) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

static void table_adjust_capacity(Table *table, const size_t capacity) {
    Entry *entries = CLOX_ALLOCATE(Entry, capacity);

    for (size_t i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        const Entry *src = &table->entries[i];
        if (src->key == NULL) continue;

        Entry *dest = find_entries(entries, capacity, src->key);
        dest->key = src->key;
        dest->value = src->value;
        table->count++;
    }

    CLOX_FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

void table_init(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void table_free(Table *table) {
    CLOX_FREE_ARRAY(Entry, table->entries, table->count);
    table_init(table);
}

bool table_get(const Table *table, const ObjString *key, Entry **out) {
    if (table->count == 0) return false;

    Entry *entry = find_entries(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *out = entry;
    return true;
}

bool table_set(Table *table, ObjString *key, const Value value, const uint8_t flags) {
    if (table->count + 1 > (size_t)((double)table->capacity * TABLE_MAX_LOAD)) {
        const size_t new_capacity = CLOX_GROW_CAPACITY(table->capacity);
        table_adjust_capacity(table, new_capacity);
    }

    Entry *entry = find_entries(table->entries, table->capacity, key);
    const bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    entry->flags = flags;
    return is_new_key;
}

bool table_delete(const Table *table, const ObjString *key) {
    if (table->count == 0) return false;

    Entry *entry = find_entries(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone
    entry->key = NULL;
    entry-> value = BOOL_VAL(true);
    return true;
}

void table_add_all(const Table *from, Table *to) {
    for (size_t i = 0; i< from->capacity; i++) {
        const Entry *src = &from->entries[i];
        if (src->key == NULL) continue;
        table_set(to, src->key, src->value, src->flags);
    }
}

ObjString * table_find_string(const Table *table, const char *chars, const size_t length, const uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    while (true) {
        const Entry *entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone
            if (IS_NIL(entry->value)) return NULL;
        } else if (
            entry->key->length == length
            && entry->key->hash == hash
            && memcmp(entry->key->chars, chars, length) == 0
        ) {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}
