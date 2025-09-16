/**
 * @file table.c
 * @brief Implements an open addressing hash table for string keys.
 */
#include "table.h"

#include <string.h>

#include "memory.h"
#include "object.h"

/**
 * @brief Maximum table load factor before resizing.
 */
#define TABLE_MAX_LOAD 0.75

/**
 * @brief Finds an entry slot for the specified key.
 *
 * @param entries Entry array to search.
 * @param capacity Capacity of the entry array.
 * @param key Key to locate.
 * @return Pointer to the matching entry or insertion point.
 */
static Entry* find_entry(Entry* entries, int capacity, ObjString* key);

/**
 * @brief Allocates storage for the table at the requested capacity.
 *
 * @param table Table to resize.
 * @param capacity Desired entry capacity.
 */
static void adjust_capacity(Table* table, int capacity);

/**
 * @see init_table
 */
void init_table(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

/**
 * @see free_table
 */
void free_table(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  init_table(table);
}

/**
 * @see table_get
 */
bool table_get(Table* table, ObjString* key, Value* value) {
  if (table->entries == NULL) return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

/**
 * @see table_set
 */
bool table_set(Table* table, ObjString* key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjust_capacity(table, capacity);
  }

  Entry* entry = find_entry(table->entries, table->capacity, key);
  bool is_new_key = entry->key == NULL;
  if (is_new_key && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return is_new_key;
}

/**
 * @see table_delete
 */
bool table_delete(Table* table, ObjString* key) {
  if (table->entries == NULL) return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

/**
 * @see table_add_all
 */
void table_add_all(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->key != NULL) {
      table_set(to, entry->key, entry->value);
    }
  }
}

/**
 * @see table_find_string
 */
ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash) {
  if (table->entries == NULL) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry* entry = &table->entries[index];
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return NULL;
    } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

/**
 * @see table_remove_white
 */
void table_remove_white(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->obj.is_marked) {
      table_delete(table, entry->key);
    }
  }
}

/**
 * @see mark_table
 */
void mark_table(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL) {
      mark_object((Obj*)entry->key);
      mark_value(entry->value);
    }
  }
}

/**
 * @see find_entry
 */
static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
  uint32_t index = key->hash % capacity;
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        return tombstone != NULL ? tombstone : entry;
      } else {
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      return entry;
    }
    index = (index + 1) % capacity;
  }
}

/**
 * @see adjust_capacity
 */
static void adjust_capacity(Table* table, int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;

    Entry* dest = find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}
