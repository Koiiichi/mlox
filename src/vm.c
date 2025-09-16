/**
 * @file vm.c
 * @brief Implements the bytecode virtual machine execution engine.
 */
#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "value.h"

VM vm;

/**
 * @brief Resets the VM stack to an empty state.
 */
static void reset_stack(void);

/**
 * @brief Reports a runtime error and unwinds the stack.
 *
 * @param format printf-style format string.
 * @param ... Format arguments.
 */
static void runtime_error(const char* format, ...);

/**
 * @brief Returns the value at a distance from the stack top without popping.
 *
 * @param distance Distance from the top of the stack.
 * @return Value located at the given distance.
 */
static Value peek(int distance);

/**
 * @brief Invokes a closure ensuring argument counts match.
 *
 * @param closure Closure to call.
 * @param arg_count Number of arguments supplied.
 * @return true on success.
 */
static bool call_closure(ObjClosure* closure, int arg_count);

/**
 * @brief Invokes a callee value which may be various callable types.
 *
 * @param callee Callee value to invoke.
 * @param arg_count Number of arguments supplied.
 * @return true on success.
 */
static bool call_value(Value callee, int arg_count);

/**
 * @brief Captures a stack slot as an upvalue.
 *
 * @param local Pointer to the stack slot to capture.
 * @return Newly created or existing upvalue.
 */
static ObjUpvalue* capture_upvalue(Value* local);

/**
 * @brief Closes all open upvalues referencing slots >= last.
 *
 * @param last Pointer to the first slot that should remain open.
 */
static void close_upvalues(Value* last);

/**
 * @brief Binds a method from a class to an instance.
 *
 * @param klass Class containing the method.
 * @param name Method name.
 * @return true if method was found and bound.
 */
static bool bind_method(ObjClass* klass, ObjString* name);

/**
 * @brief Calls a method on a class by name.
 *
 * @param klass Class containing the method.
 * @param name Method name.
 * @param arg_count Number of arguments supplied.
 * @return true on success.
 */
static bool invoke_from_class(ObjClass* klass, ObjString* name, int arg_count);

/**
 * @brief Invokes a method on the receiver currently on the stack.
 *
 * @param name Method name.
 * @param arg_count Number of arguments supplied.
 * @return true on success.
 */
static bool invoke(ObjString* name, int arg_count);

/**
 * @brief Defines a method on the class at the top of the stack.
 *
 * @param name Method name string.
 */
static void define_method(ObjString* name);

/**
 * @brief Concatenates the top two string values on the stack.
 */
static void concatenate(void);

/**
 * @brief Determines whether a value evaluates to false in truthiness checks.
 *
 * @param value Value to evaluate.
 * @return true if the value is considered falsey.
 */
static bool is_falsey(Value value);

/**
 * @brief Executes the bytecode for the current call frame.
 *
 * @return InterpretResult indicating success or error.
 */
static InterpretResult run(void);

void init_vm(void) {
  reset_stack();
  vm.objects = NULL;
  vm.bytes_allocated = 0;
  vm.next_gc = 1024 * 1024;
  vm.gray_count = 0;
  vm.gray_capacity = 0;
  vm.gray_stack = NULL;
  init_table(&vm.globals);
  init_table(&vm.strings);
  vm.init_string = copy_string("init", 4);
  register_core_natives();
}

void free_vm(void) {
  free_table(&vm.globals);
  free_table(&vm.strings);
  vm.init_string = NULL;
  free_objects();
}

InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  ObjClosure* closure = new_closure(function);
  pop();
  push(OBJ_VAL(closure));
  if (!call_closure(closure, 0)) {
    pop();
    return INTERPRET_RUNTIME_ERROR;
  }

  InterpretResult result = run();
  return result;
}

void push(Value value) {
  *vm.stack_top = value;
  vm.stack_top++;
}

Value pop(void) {
  vm.stack_top--;
  return *vm.stack_top;
}

void vm_define_native(const char* name, NativeFn function) {
  push(OBJ_VAL(copy_string(name, (int)strlen(name))));
  push(OBJ_VAL(new_native(function)));
  table_set(&vm.globals, AS_STRING(vm.stack_top[-2]), vm.stack_top[-1]);
  pop();
  pop();
}

static void reset_stack(void) {
  vm.stack_top = vm.stack;
  vm.frame_count = 0;
  vm.open_upvalues = NULL;
}

static void runtime_error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frame_count - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in %s\n", function->chunk.lines[instruction], function->name == NULL ? "script" : function->name->chars);
  }

  reset_stack();
}

static Value peek(int distance) {
  return vm.stack_top[-1 - distance];
}

static bool call_closure(ObjClosure* closure, int arg_count) {
  if (arg_count != closure->function->arity) {
    runtime_error("Expected %d arguments but got %d.", closure->function->arity, arg_count);
    return false;
  }

  if (vm.frame_count == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frame_count++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stack_top - arg_count - 1;
  return true;
}

static bool call_value(Value callee, int arg_count) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stack_top[-arg_count - 1] = bound->receiver;
        return call_closure(bound->method, arg_count);
      }
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.stack_top[-arg_count - 1] = OBJ_VAL(new_instance(klass));
        Value initializer;
        if (table_get(&klass->methods, vm.init_string, &initializer)) {
          return call_closure(AS_CLOSURE(initializer), arg_count);
        } else if (arg_count != 0) {
          runtime_error("Expected 0 arguments but got %d.", arg_count);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call_closure(AS_CLOSURE(callee), arg_count);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(arg_count, vm.stack_top - arg_count);
        vm.stack_top -= arg_count + 1;
        push(result);
        return true;
      }
      case OBJ_INSTANCE:
      case OBJ_STRING:
      case OBJ_FUNCTION:
      case OBJ_UPVALUE:
        break;
    }
  }

  runtime_error("Can only call functions and classes.");
  return false;
}

static ObjUpvalue* capture_upvalue(Value* local) {
  ObjUpvalue* prev_upvalue = NULL;
  ObjUpvalue* upvalue = vm.open_upvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* created = new_upvalue(local);
  created->next = upvalue;

  if (prev_upvalue == NULL) {
    vm.open_upvalues = created;
  } else {
    prev_upvalue->next = created;
  }

  return created;
}

static void close_upvalues(Value* last) {
  while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
    ObjUpvalue* upvalue = vm.open_upvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.open_upvalues = upvalue->next;
  }
}

static bool bind_method(ObjClass* klass, ObjString* name) {
  Value method;
  if (!table_get(&klass->methods, name, &method)) {
    runtime_error("Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod* bound = new_bound_method(peek(0), AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static bool invoke_from_class(ObjClass* klass, ObjString* name, int arg_count) {
  Value method;
  if (!table_get(&klass->methods, name, &method)) {
    runtime_error("Undefined property '%s'.", name->chars);
    return false;
  }
  return call_closure(AS_CLOSURE(method), arg_count);
}

static bool invoke(ObjString* name, int arg_count) {
  Value receiver = peek(arg_count);

  if (!IS_INSTANCE(receiver)) {
    runtime_error("Only instances have methods.");
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(receiver);
  Value value;
  if (table_get(&instance->fields, name, &value)) {
    vm.stack_top[-arg_count - 1] = value;
    return call_value(value, arg_count);
  }

  return invoke_from_class(instance->klass, name, arg_count);
}

static void define_method(ObjString* name) {
  Value method = peek(0);
  ObjClass* klass = AS_CLASS(peek(1));
  table_set(&klass->methods, name, method);
  pop();
}

static void concatenate(void) {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = take_string(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static bool is_falsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static InterpretResult run(void) {
  CallFrame* frame = &vm.frames[vm.frame_count - 1];

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[");
      print_value(*slot);
      printf("]");
    }
    printf("\n");
    disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
#endif
    uint8_t instruction;
    switch (instruction = *frame->ip++) {
      case OP_CONSTANT: {
        uint8_t constant_index = *frame->ip++;
        Value constant = frame->closure->function->chunk.constants.values[constant_index];
        push(constant);
        break;
      }
      case OP_NIL:
        push(NIL_VAL);
        break;
      case OP_TRUE:
        push(BOOL_VAL(true));
        break;
      case OP_FALSE:
        push(BOOL_VAL(false));
        break;
      case OP_POP:
        pop();
        break;
      case OP_GET_LOCAL: {
        uint8_t slot = *frame->ip++;
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = *frame->ip++;
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        Value value;
        if (!table_get(&vm.globals, name, &value)) {
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        table_set(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        if (table_set(&vm.globals, name, peek(0))) {
          table_delete(&vm.globals, name);
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = *frame->ip++;
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = *frame->ip++;
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtime_error("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);

        Value value;
        if (table_get(&instance->fields, name, &value)) {
          pop();
          push(value);
          break;
        }

        if (!bind_method(instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtime_error("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(1));
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        table_set(&instance->fields, name, peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_GET_SUPER: {
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        ObjClass* superclass = AS_CLASS(pop());
        if (!bind_method(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(values_equal(a, b)));
        break;
      }
      case OP_GREATER: {
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtime_error("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(BOOL_VAL(a > b));
        break;
      }
      case OP_LESS: {
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtime_error("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(BOOL_VAL(a < b));
        break;
      }
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtime_error("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: {
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtime_error("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a - b));
        break;
      }
      case OP_MULTIPLY: {
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtime_error("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a * b));
        break;
      }
      case OP_DIVIDE: {
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
          runtime_error("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a / b));
        break;
      }
      case OP_NOT:
        push(BOOL_VAL(is_falsey(pop())));
        break;
      case OP_NEGATE: {
        if (!IS_NUMBER(peek(0))) {
          runtime_error("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        double value = AS_NUMBER(pop());
        push(NUMBER_VAL(-value));
        break;
      }
      case OP_PRINT:
        print_value(pop());
        printf("\n");
        break;
      case OP_JUMP: {
        uint16_t offset = (uint16_t)(*frame->ip++ << 8);
        offset |= *frame->ip++;
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = (uint16_t)(*frame->ip++ << 8);
        offset |= *frame->ip++;
        if (is_falsey(peek(0))) {
          frame->ip += offset;
        }
        break;
      }
      case OP_LOOP: {
        uint16_t offset = (uint16_t)(*frame->ip++ << 8);
        offset |= *frame->ip++;
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        uint8_t arg_count = *frame->ip++;
        if (!call_value(peek(arg_count), arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_INVOKE: {
        ObjString* method = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        uint8_t arg_count = *frame->ip++;
        if (!invoke(method, arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString* method = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        uint8_t arg_count = *frame->ip++;
        ObjClass* superclass = AS_CLASS(pop());
        if (!invoke_from_class(superclass, method, arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(frame->closure->function->chunk.constants.values[*frame->ip++]);
        ObjClosure* closure = new_closure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalue_count; i++) {
          uint8_t is_local = *frame->ip++;
          uint8_t index = *frame->ip++;
          closure->upvalues[i] = is_local ? capture_upvalue(frame->slots + index) : frame->closure->upvalues[index];
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        close_upvalues(vm.stack_top - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        close_upvalues(frame->slots);
        vm.frame_count--;
        if (vm.frame_count == 0) {
          pop();
          push(result);
          return INTERPRET_OK;
        }
        vm.stack_top = frame->slots;
        push(result);
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_CLASS: {
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        push(OBJ_VAL(new_class(name)));
        break;
      }
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtime_error("Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjClass* subclass = AS_CLASS(peek(0));
        table_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);
        pop();
        pop();
        push(OBJ_VAL(subclass));
        break;
      }
      case OP_METHOD: {
        ObjString* name = AS_STRING(frame->closure->function->chunk.constants.values[*frame->ip++]);
        define_method(name);
        break;
      }
    }
  }
}
