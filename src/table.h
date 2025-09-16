/**
 * @file table.h
 * @brief Open addressing hash table implementation used by the VM.
 */
#ifndef MLOX_TABLE_H
#define MLOX_TABLE_H

#include "common.h"
#include "value.h"

typedef struct ObjString ObjString;

/**
 * @brief Single entry within the open addressing hash table.
 */
typedef struct {
  ObjString* key; /**< Interned string key. */
  Value value; /**< Associated value. */
} Entry;

/**
 * @brief Hash table storing string keys and value payloads.
 */
typedef struct {
  int count; /**< Number of valid key/value pairs. */
  int capacity; /**< Allocated entry capacity. */
  Entry* entries; /**< Entry array buffer. */
} Table;

/**
 * @brief Initializes a table instance to empty.
 *
 * @param table Table to initialize.
 */
void init_table(Table* table);

/**
 * @brief Releases memory owned by the table and resets its state.
 *
 * @param table Table to clean up.
 */
void free_table(Table* table);

/**
 * @brief Looks up a key within the table.
 *
 * @param table Table to search.
 * @param key Interned key to retrieve.
 * @param value Out parameter receiving the associated value when found.
 * @return true if the key exists within the table.
 */
bool table_get(Table* table, ObjString* key, Value* value);

/**
 * @brief Inserts or updates a key/value pair in the table.
 *
 * @param table Table to mutate.
 * @param key Key to set.
 * @param value Value to associate with the key.
 * @return true if a new key was added, false if an existing key was updated.
 */
bool table_set(Table* table, ObjString* key, Value value);

/**
 * @brief Removes a key from the table if present.
 *
 * @param table Table to mutate.
 * @param key Key to delete.
 * @return true if the key was removed.
 */
bool table_delete(Table* table, ObjString* key);

/**
 * @brief Copies all entries from one table into another.
 *
 * @param from Source table.
 * @param to Destination table.
 */
void table_add_all(Table* from, Table* to);

/**
 * @brief Finds an interned string within the table matching the provided data.
 *
 * @param table Table to search.
 * @param chars Character buffer to match.
 * @param length Length of the character buffer.
 * @param hash Precomputed hash value.
 * @return Pointer to the interned string when found, otherwise NULL.
 */
ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash);

/**
 * @brief Removes entries whose keys are unmarked during garbage collection.
 *
 * @param table Table to compact.
 */
void table_remove_white(Table* table);

/**
 * @brief Marks all values and keys within the table during GC tracing.
 *
 * @param table Table to mark.
 */
void mark_table(Table* table);

#endif /* MLOX_TABLE_H */
