/**
 * @file object.c
 * @brief Implements heap object allocation and utilities.
 */
#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

/**
 * @brief Allocates a new object with the given size and type.
 *
 * @param size Size of the object structure.
 * @param type Object type tag.
 * @return Pointer to the allocated object header.
 */
static Obj* allocate_object(size_t size, ObjType type);

/**
 * @brief Allocates a string object without interning.
 *
 * @param chars Character buffer to adopt.
 * @param length Length of the buffer.
 * @param hash Precomputed hash value.
 * @return Newly allocated string object.
 */
static ObjString* allocate_string(char* chars, int length, uint32_t hash);

/**
 * @brief Computes the FNV-1a hash for a string.
 *
 * @param key Character buffer to hash.
 * @param length Length of the buffer.
 * @return 32-bit hash code.
 */
static uint32_t hash_string(const char* key, int length);

/**
 * @see new_function
 */
ObjFunction* new_function(void) {
  ObjFunction* function = (ObjFunction*)allocate_object(sizeof(ObjFunction), OBJ_FUNCTION);
  function->arity = 0;
  function->upvalue_count = 0;
  function->name = NULL;
  init_chunk(&function->chunk);
  return function;
}

/**
 * @see new_closure
 */
ObjClosure* new_closure(ObjFunction* function) {
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalue_count);
  for (int i = 0; i < function->upvalue_count; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = (ObjClosure*)allocate_object(sizeof(ObjClosure), OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

/**
 * @see new_native
 */
ObjNative* new_native(NativeFn function) {
  ObjNative* native = (ObjNative*)allocate_object(sizeof(ObjNative), OBJ_NATIVE);
  native->function = function;
  return native;
}

/**
 * @see take_string
 */
ObjString* take_string(char* chars, int length) {
  uint32_t hash = hash_string(chars, length);
  ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocate_string(chars, length, hash);
}

/**
 * @see copy_string
 */
ObjString* copy_string(const char* chars, int length) {
  uint32_t hash = hash_string(chars, length);
  ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;

  char* heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';

  return allocate_string(heap_chars, length, hash);
}

/**
 * @see new_upvalue
 */
ObjUpvalue* new_upvalue(Value* slot) {
  ObjUpvalue* upvalue = (ObjUpvalue*)allocate_object(sizeof(ObjUpvalue), OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

/**
 * @see new_class
 */
ObjClass* new_class(ObjString* name) {
  ObjClass* klass = (ObjClass*)allocate_object(sizeof(ObjClass), OBJ_CLASS);
  klass->name = name;
  init_table(&klass->methods);
  return klass;
}

/**
 * @see new_instance
 */
ObjInstance* new_instance(ObjClass* klass) {
  ObjInstance* instance = (ObjInstance*)allocate_object(sizeof(ObjInstance), OBJ_INSTANCE);
  instance->klass = klass;
  init_table(&instance->fields);
  return instance;
}

/**
 * @see new_bound_method
 */
ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method) {
  ObjBoundMethod* bound = (ObjBoundMethod*)allocate_object(sizeof(ObjBoundMethod), OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

/**
 * @see print_object
 */
void print_object(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = AS_BOUND_METHOD(value);
      const char* name = bound->method->function->name != NULL ? bound->method->function->name->chars : "<fn>";
      printf("<bound method %s>", name);
      break;
    }
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      print_object(OBJ_VAL(AS_CLOSURE(value)->function));
      break;
    case OBJ_FUNCTION: {
      ObjFunction* function = AS_FUNCTION(value);
      if (function->name == NULL) {
        printf("<script>");
      } else {
        printf("<fn %s>", function->name->chars);
      }
      break;
    }
    case OBJ_INSTANCE:
      printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
  }
}

/**
 * @see is_obj_type
 */
bool is_obj_type(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

/**
 * @see allocate_object
 */
static Obj* allocate_object(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->is_marked = false;
  object->next = vm.objects;
  vm.objects = object;
#ifdef DEBUG_LOG_GC
  printf("allocate %zu bytes for type %d\n", size, type);
#endif
  return object;
}

/**
 * @see allocate_string
 */
static ObjString* allocate_string(char* chars, int length, uint32_t hash) {
  ObjString* string = (ObjString*)allocate_object(sizeof(ObjString), OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  push(OBJ_VAL(string));
  table_set(&vm.strings, string, NIL_VAL);
  pop();
  return string;
}

/**
 * @see hash_string
 */
static uint32_t hash_string(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}
