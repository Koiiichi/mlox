/**
 * @file memory.h
 * @brief Memory management helpers and garbage collector interface.
 */
#ifndef MLOX_MEMORY_H
#define MLOX_MEMORY_H

#include "common.h"
#include "value.h"

/**
 * @brief Macro used to grow container capacity exponentially.
 */
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

/**
 * @brief Macro to grow or allocate arrays using the VM allocator.
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

/**
 * @brief Macro to allocate a typed array using the VM allocator.
 */
#define ALLOCATE(type, count) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count))

/**
 * @brief Macro to free a single typed allocation.
 */
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/**
 * @brief Macro to free an array allocation.
 */
#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

struct Obj;
typedef struct Obj Obj;

/**
 * @brief Core allocator used for all VM managed memory operations.
 *
 * @param pointer Existing allocation pointer or NULL when allocating fresh memory.
 * @param old_size Current size of the allocation in bytes.
 * @param new_size Desired size of the allocation in bytes.
 * @return Pointer to the resized memory region, or NULL if freed.
 */
void* reallocate(void* pointer, size_t old_size, size_t new_size);

/**
 * @brief Marks an object as reachable for garbage collection.
 *
 * @param object Object to mark.
 */
void mark_object(Obj* object);

/**
 * @brief Marks the value and underlying object (if any) as reachable.
 *
 * @param value Value to mark.
 */
void mark_value(Value value);

/**
 * @brief Performs a garbage collection cycle reclaiming unreachable objects.
 */
void collect_garbage(void);

/**
 * @brief Releases all allocated objects when the VM shuts down.
 */
void free_objects(void);

#endif /* MLOX_MEMORY_H */
