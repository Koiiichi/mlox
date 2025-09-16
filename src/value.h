/**
 * @file value.h
 * @brief Runtime value representations and helper utilities.
 */
#ifndef MLOX_VALUE_H
#define MLOX_VALUE_H

#include "common.h"

struct Obj;
typedef struct Obj Obj;
struct ObjString;
typedef struct ObjString ObjString;

/**
 * @brief Enumerates runtime value variants supported by the VM.
 */
typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ
} ValueType;

/**
 * @brief Tagged union describing a dynamically typed value at runtime.
 */
typedef struct {
  ValueType type; /**< Discriminant describing the stored payload. */
  union {
    bool boolean; /**< Boolean payload. */
    double number; /**< Numeric payload. */
    Obj* obj; /**< Pointer to heap allocated object payload. */
  } as; /**< Variant payload storage. */
} Value;

/**
 * @brief Convenience macro for creating boolean values.
 */
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = (value)}})
/**
 * @brief Convenience macro for the nil sentinel value.
 */
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
/**
 * @brief Convenience macro for creating numeric values.
 */
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = (value)}})
/**
 * @brief Convenience macro for creating object values.
 */
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)(object)}})

/**
 * @brief Checks whether a value stores a boolean.
 */
#define IS_BOOL(value) ((value).type == VAL_BOOL)
/**
 * @brief Checks whether a value stores the nil sentinel.
 */
#define IS_NIL(value) ((value).type == VAL_NIL)
/**
 * @brief Checks whether a value stores a number.
 */
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
/**
 * @brief Checks whether a value stores a heap object reference.
 */
#define IS_OBJ(value) ((value).type == VAL_OBJ)

/**
 * @brief Extracts the boolean payload from a value.
 */
#define AS_BOOL(value) ((value).as.boolean)
/**
 * @brief Extracts the numeric payload from a value.
 */
#define AS_NUMBER(value) ((value).as.number)
/**
 * @brief Extracts the object pointer payload from a value.
 */
#define AS_OBJ(value) ((value).as.obj)

/**
 * @brief Resizable array container for Value elements.
 */
typedef struct {
  int capacity; /**< Allocated slots. */
  int count; /**< Number of elements used. */
  Value* values; /**< Pointer to the contiguous storage buffer. */
} ValueArray;

/**
 * @brief Determines equality between two runtime values.
 *
 * @param a First value to compare.
 * @param b Second value to compare.
 * @return true if the values are equivalent, false otherwise.
 */
bool values_equal(Value a, Value b);

/**
 * @brief Initializes a value array to an empty state.
 *
 * @param array Array instance to initialize.
 */
void init_value_array(ValueArray* array);

/**
 * @brief Appends a value to the resizable array, expanding capacity as needed.
 *
 * @param array Target array to mutate.
 * @param value Value to append.
 */
void write_value_array(ValueArray* array, Value value);

/**
 * @brief Releases memory owned by the array and resets it to empty.
 *
 * @param array Array to free.
 */
void free_value_array(ValueArray* array);

/**
 * @brief Prints a human readable representation of a value to stdout.
 *
 * @param value Value instance to render.
 */
void print_value(Value value);

#endif /* MLOX_VALUE_H */
