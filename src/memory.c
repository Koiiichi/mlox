/**
 * @file memory.c
 * @brief Implements the garbage collector and memory allocator.
 */
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "object.h"
#include "table.h"
#include "vm.h"

/**
 * @brief Frees a single object instance based on its type.
 *
 * @param object Object to free.
 */
static void free_object(Obj* object);

/**
 * @brief Marks all VM roots prior to tracing.
 */
static void mark_roots(void);

/**
 * @brief Traces gray objects until none remain.
 */
static void trace_references(void);

/**
 * @brief Frees unreachable objects from the allocation list.
 */
static void sweep(void);

/**
 * @brief Marks all values in a dynamic array.
 *
 * @param array Value array to mark.
 */
static void mark_array(ValueArray* array);

/**
 * @brief Moves an object from gray to black by marking its references.
 *
 * @param object Object to blacken.
 */
static void blacken_object(Obj* object);

/**
 * @see reallocate
 */
void* reallocate(void* pointer, size_t old_size, size_t new_size) {
#ifdef DEBUG_STRESS_GC
  if (new_size > old_size) {
    collect_garbage();
  }
#endif
  if (new_size > old_size) {
    vm.bytes_allocated += new_size - old_size;
    if (vm.bytes_allocated > vm.next_gc) {
      collect_garbage();
    }
  } else {
    vm.bytes_allocated -= old_size - new_size;
  }

  if (new_size == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, new_size);
  if (result == NULL) exit(1);
  return result;
}

/**
 * @see mark_object
 */
void mark_object(Obj* object) {
  if (object == NULL) return;
  if (object->is_marked) return;

  object->is_marked = true;

  if (vm.gray_capacity < vm.gray_count + 1) {
    vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
    vm.gray_stack = (Obj**)realloc(vm.gray_stack, sizeof(Obj*) * vm.gray_capacity);
    if (vm.gray_stack == NULL) exit(1);
  }

  vm.gray_stack[vm.gray_count++] = object;
}

/**
 * @see mark_value
 */
void mark_value(Value value) {
  if (IS_OBJ(value)) mark_object(AS_OBJ(value));
}

/**
 * @see mark_array
 */
static void mark_array(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    mark_value(array->values[i]);
  }
}

/**
 * @see blacken_object
 */
static void blacken_object(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("blacken %p type %d\n", (void*)object, object->type);
#endif
  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      mark_value(bound->receiver);
      mark_object((Obj*)bound->method);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      mark_object((Obj*)klass->name);
      mark_table(&klass->methods);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      mark_object((Obj*)closure->function);
      for (int i = 0; i < closure->upvalue_count; i++) {
        mark_object((Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      if (function->name != NULL) mark_object((Obj*)function->name);
      mark_array(&function->chunk.constants);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      mark_object((Obj*)instance->klass);
      mark_table(&instance->fields);
      break;
    }
    case OBJ_NATIVE:
      break;
    case OBJ_STRING:
      break;
    case OBJ_UPVALUE: {
      ObjUpvalue* upvalue = (ObjUpvalue*)object;
      mark_value(upvalue->closed);
      break;
    }
  }
}

/**
 * @see mark_roots
 */
static void mark_roots(void) {
  for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
    mark_value(*slot);
  }

  for (int i = 0; i < vm.frame_count; i++) {
    mark_object((Obj*)vm.frames[i].closure);
  }

  for (ObjUpvalue* upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
    mark_object((Obj*)upvalue);
  }

  mark_table(&vm.globals);
  mark_compiler_roots();
  mark_object((Obj*)vm.init_string);
}

/**
 * @see trace_references
 */
static void trace_references(void) {
  while (vm.gray_count > 0) {
    Obj* object = vm.gray_stack[--vm.gray_count];
    blacken_object(object);
  }
}

/**
 * @see sweep
 */
static void sweep(void) {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->is_marked) {
      object->is_marked = false;
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }
      free_object(unreached);
    }
  }
}

/**
 * @see collect_garbage
 */
void collect_garbage(void) {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytes_allocated;
#endif

  mark_roots();
  trace_references();
  table_remove_white(&vm.strings);
  sweep();

  vm.next_gc = vm.bytes_allocated * 2;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu)\n", before - vm.bytes_allocated, before, vm.bytes_allocated);
#endif
}

/**
 * @see free_objects
 */
void free_objects(void) {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    free_object(object);
    object = next;
  }
  free(vm.gray_stack);
}

/**
 * @see free_object
 */
static void free_object(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("free type %d\n", object->type);
#endif
  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      FREE(ObjBoundMethod, object);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      free_table(&klass->methods);
      FREE(ObjClass, object);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      free_chunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      free_table(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
    case OBJ_NATIVE: {
      FREE(ObjNative, object);
      break;
    }
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(ObjUpvalue, object);
      break;
    }
  }
}
