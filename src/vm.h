/**
 * @file vm.h
 * @brief Bytecode virtual machine execution engine interface.
 */
#ifndef MLOX_VM_H
#define MLOX_VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

/**
 * @brief Maximum number of values that can be stored on the stack.
 */
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

/**
 * @brief Maximum number of call frames in the VM.
 */
#define FRAMES_MAX 64

/**
 * @brief Maximum number of local variables or upvalues that can be addressed.
 */
#define UINT8_COUNT (UINT8_MAX + 1)

/**
 * @brief Result codes returned by the interpreter.
 */
typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

/**
 * @brief Activation record for a single function call.
 */
typedef struct {
  ObjClosure* closure; /**< Closure being executed. */
  uint8_t* ip; /**< Instruction pointer into the closure's chunk. */
  Value* slots; /**< Pointer to the first slot of the call's local variables. */
} CallFrame;

/**
 * @brief Global virtual machine state.
 */
typedef struct {
  CallFrame frames[FRAMES_MAX]; /**< Call frame stack. */
  int frame_count; /**< Number of active call frames. */
  Value stack[STACK_MAX]; /**< Operand stack storage. */
  Value* stack_top; /**< Points just past the top value on the stack. */
  Table globals; /**< Global variable table. */
  Table strings; /**< Interned string table. */
  ObjString* init_string; /**< Cached initializer method name. */
  ObjUpvalue* open_upvalues; /**< Linked list of open upvalues. */
  size_t bytes_allocated; /**< Total bytes allocated tracked by GC. */
  size_t next_gc; /**< Threshold for triggering the next GC cycle. */
  Obj* objects; /**< Linked list of all allocated objects. */
  int gray_count; /**< Number of objects in the gray stack. */
  int gray_capacity; /**< Allocated capacity for the gray stack. */
  Obj** gray_stack; /**< Stack used for GC marking. */
} VM;

extern VM vm;

/**
 * @brief Initializes the virtual machine state and supporting subsystems.
 */
void init_vm(void);

/**
 * @brief Releases VM resources including heap allocations and tables.
 */
void free_vm(void);

/**
 * @brief Compiles and executes source code.
 *
 * @param source Null-terminated source string to execute.
 * @return Result code describing interpreter success or failure.
 */
InterpretResult interpret(const char* source);

/**
 * @brief Pushes a value onto the VM operand stack.
 *
 * @param value Value to push.
 */
void push(Value value);

/**
 * @brief Pops and returns the top value from the VM operand stack.
 *
 * @return The popped value.
 */
Value pop(void);

/**
 * @brief Registers a native function within the global namespace.
 *
 * @param name Name of the native function.
 * @param function Implementation pointer.
 */
void vm_define_native(const char* name, NativeFn function);

#endif /* MLOX_VM_H */
