/**
 * @file object.h
 * @brief Heap allocated object definitions for the MLOX runtime.
 */
#ifndef MLOX_OBJECT_H
#define MLOX_OBJECT_H

#include "chunk.h"
#include "value.h"
#include "table.h"

/**
 * @brief Retrieves the concrete object type for a value.
 */
#define OBJ_TYPE(value) (AS_OBJ(value)->type)
/**
 * @brief Determines whether a value is a string object.
 */
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
/**
 * @brief Determines whether a value is a function object.
 */
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
/**
 * @brief Determines whether a value is a closure object.
 */
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
/**
 * @brief Determines whether a value is a class object.
 */
#define IS_CLASS(value) is_obj_type(value, OBJ_CLASS)
/**
 * @brief Determines whether a value is a native function object.
 */
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
/**
 * @brief Determines whether a value is an instance object.
 */
#define IS_INSTANCE(value) is_obj_type(value, OBJ_INSTANCE)
/**
 * @brief Determines whether a value is a bound method object.
 */
#define IS_BOUND_METHOD(value) is_obj_type(value, OBJ_BOUND_METHOD)

/**
 * @brief Casts a generic value to a string object pointer.
 */
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
/**
 * @brief Retrieves the underlying C string from a string object value.
 */
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
/**
 * @brief Casts a generic value to a function object pointer.
 */
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
/**
 * @brief Casts a generic value to a closure object pointer.
 */
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
/**
 * @brief Casts a generic value to a class object pointer.
 */
#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
/**
 * @brief Retrieves the native function pointer from a native object value.
 */
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)
/**
 * @brief Casts a generic value to an instance object pointer.
 */
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
/**
 * @brief Casts a generic value to a bound method object pointer.
 */
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

/**
 * @brief Enumerates all supported heap object types.
 */
typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE
} ObjType;

/**
 * @brief Function signature for native functions exposed to MLOX.
 */
typedef Value (*NativeFn)(int arg_count, Value* args);

/**
 * @brief Base object header embedded at the start of every object.
 */
typedef struct Obj {
  ObjType type; /**< Concrete object type discriminator. */
  bool is_marked; /**< Garbage collection mark flag. */
  struct Obj* next; /**< Singly linked list pointer for global allocation tracking. */
} Obj;

typedef struct ObjUpvalue ObjUpvalue;

/**
 * @brief Function object representing a user defined chunk of bytecode.
 */
typedef struct {
  Obj obj; /**< Base object header. */
  int arity; /**< Number of arguments required by the function. */
  int upvalue_count; /**< Number of captured upvalues. */
  Chunk chunk; /**< Bytecode chunk implementing the function body. */
  ObjString* name; /**< Optional function name for debugging. */
} ObjFunction;

/**
 * @brief Closure object wrapping a function with captured upvalues.
 */
typedef struct {
  Obj obj; /**< Base object header. */
  ObjFunction* function; /**< Underlying function definition. */
  ObjUpvalue** upvalues; /**< Captured upvalue array. */
  int upvalue_count; /**< Number of upvalues captured. */
} ObjClosure;

/**
 * @brief Native function object wrapping a C callback.
 */
typedef struct {
  Obj obj; /**< Base object header. */
  NativeFn function; /**< Pointer to the native implementation. */
} ObjNative;

/**
 * @brief Interned string object storing byte sequences with cached hash codes.
 */
typedef struct ObjString {
  Obj obj; /**< Base object header. */
  int length; /**< String length in bytes. */
  char* chars; /**< Heap allocated character data. */
  uint32_t hash; /**< Cached FNV-1a hash. */
} ObjString;

/**
 * @brief Upvalue object referencing a closed-over stack slot.
 */
typedef struct ObjUpvalue {
  Obj obj; /**< Base object header. */
  Value* location; /**< Pointer to the variable's storage location. */
  Value closed; /**< Stored value when the upvalue is closed. */
  struct ObjUpvalue* next; /**< Linked list for open upvalues. */
} ObjUpvalue;

/**
 * @brief Class object representing user defined classes.
 */
typedef struct {
  Obj obj; /**< Base object header. */
  ObjString* name; /**< Name of the class. */
  Table methods; /**< Method table mapping names to closures. */
} ObjClass;

/**
 * @brief Instance object storing per-instance fields.
 */
typedef struct {
  Obj obj; /**< Base object header. */
  ObjClass* klass; /**< Reference to the class definition. */
  Table fields; /**< Field table mapping property names to values. */
} ObjInstance;

/**
 * @brief Bound method pairing a receiver with a closure.
 */
typedef struct {
  Obj obj; /**< Base object header. */
  Value receiver; /**< Receiver value bound to the method. */
  ObjClosure* method; /**< Method closure to invoke. */
} ObjBoundMethod;

/**
 * @brief Allocates a new function object with an empty chunk.
 */
ObjFunction* new_function(void);

/**
 * @brief Wraps a function in a closure object with allocated upvalue array.
 *
 * @param function Function to wrap.
 * @return Newly created closure instance.
 */
ObjClosure* new_closure(ObjFunction* function);

/**
 * @brief Creates a native function object from a C callback.
 *
 * @param function Native implementation pointer.
 * @return Newly created native function object.
 */
ObjNative* new_native(NativeFn function);

/**
 * @brief Takes ownership of a character buffer and interns it as a string object.
 *
 * @param chars Heap allocated character buffer.
 * @param length Length of the buffer.
 * @return Interned string object.
 */
ObjString* take_string(char* chars, int length);

/**
 * @brief Copies a character buffer into a new interned string object.
 *
 * @param chars Character data to copy.
 * @param length Length of the buffer.
 * @return Interned string object.
 */
ObjString* copy_string(const char* chars, int length);

/**
 * @brief Creates a new open upvalue referencing the provided stack slot.
 *
 * @param slot Pointer to the stack slot captured by the upvalue.
 * @return Newly created upvalue instance.
 */
ObjUpvalue* new_upvalue(Value* slot);

/**
 * @brief Allocates a class object with the provided name.
 *
 * @param name Interned class name string.
 * @return Newly created class object.
 */
ObjClass* new_class(ObjString* name);

/**
 * @brief Instantiates a new instance of the specified class.
 *
 * @param klass Class to instantiate.
 * @return Newly created instance object.
 */
ObjInstance* new_instance(ObjClass* klass);

/**
 * @brief Creates a bound method tying a receiver and method closure.
 *
 * @param receiver Value to bind as the method's receiver.
 * @param method Method closure to invoke.
 * @return Newly created bound method object.
 */
ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method);

/**
 * @brief Prints a textual representation of the object stored in the value.
 *
 * @param value Object value to display.
 */
void print_object(Value value);

/**
 * @brief Utility to compare a value's object type with a given type.
 *
 * @param value Value to inspect.
 * @param type Expected object type.
 * @return true if the value contains an object of the specified type.
 */
bool is_obj_type(Value value, ObjType type);

#endif /* MLOX_OBJECT_H */
