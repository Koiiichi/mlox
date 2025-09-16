/**
 * @file compiler.c
 * @brief Implements the Pratt parser and bytecode compiler.
 */
#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "vm.h"

/**
 * @brief Parser state tracking the current and previous tokens.
 */
typedef struct {
  Token current; /**< Current token under consideration. */
  Token previous; /**< Most recently consumed token. */
  bool had_error; /**< Indicates a compile error was encountered. */
  bool panic_mode; /**< Indicates synchronization mode following an error. */
} Parser;

/**
 * @brief Parsing precedence levels used by the Pratt parser.
 */
typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

/**
 * @brief Function pointer type for parse functions.
 */
typedef void (*ParseFn)(bool can_assign);

/**
 * @brief Mapping between tokens and the functions handling their parsing.
 */
typedef struct {
  ParseFn prefix; /**< Parse function for prefix position. */
  ParseFn infix; /**< Parse function for infix position. */
  Precedence precedence; /**< Precedence level of the infix operator. */
} ParseRule;

/**
 * @brief Local variable metadata tracked during compilation.
 */
typedef struct {
  Token name; /**< Token representing the variable name. */
  int depth; /**< Scope depth where the variable was declared. */
  bool is_captured; /**< Indicates whether the variable is captured by a closure. */
} Local;

/**
 * @brief Descriptor representing an upvalue captured by a closure.
 */
typedef struct {
  uint8_t index; /**< Slot index referenced by the upvalue. */
  bool is_local; /**< True when the upvalue captures a local from the immediately enclosing function. */
} Upvalue;

/**
 * @brief Enumerates the kinds of functions handled by the compiler.
 */
typedef enum {
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT
} FunctionType;

/**
 * @brief Compiler state for the current function being compiled.
 */
typedef struct Compiler {
  struct Compiler* enclosing; /**< Enclosing compiler for nested functions. */
  ObjFunction* function; /**< Function object being constructed. */
  FunctionType type; /**< Kind of function. */

  Local locals[UINT8_COUNT]; /**< Local variable array. */
  int local_count; /**< Number of locals in use. */
  Upvalue upvalues[UINT8_COUNT]; /**< Upvalue descriptors. */
  int scope_depth; /**< Current lexical scope depth. */
} Compiler;

/**
 * @brief State information while compiling class bodies.
 */
typedef struct ClassCompiler {
  struct ClassCompiler* enclosing; /**< Enclosing class compiler. */
  bool has_superclass; /**< Indicates whether the class has a superclass. */
} ClassCompiler;

static Parser parser;
static Compiler* current = NULL;
static ClassCompiler* current_class = NULL;

static Chunk* current_chunk(void);
static void error_at(Token* token, const char* message);
static void error(const char* message);
static void error_at_current(const char* message);
static void advance_parser(void);
static void consume(TokenType type, const char* message);
static bool check(TokenType type);
static bool match(TokenType type);
static void emit_byte(uint8_t byte);
static void emit_bytes(uint8_t byte1, uint8_t byte2);
static void emit_loop(int loop_start);
static int emit_jump(uint8_t instruction);
static void emit_return(void);
static uint8_t make_constant(Value value);
static void emit_constant(Value value);
static void patch_jump(int offset);
static void init_compiler(Compiler* compiler, FunctionType type);
static ObjFunction* end_compiler(void);
static void begin_scope(void);
static void end_scope(void);
static void expression(void);
static void statement(void);
static void declaration(void);
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static uint8_t identifier_constant(Token* name);
static bool identifiers_equal(Token* a, Token* b);
static int resolve_local(Compiler* compiler, Token* name);
static int add_upvalue(Compiler* compiler, uint8_t index, bool is_local);
static int resolve_upvalue(Compiler* compiler, Token* name);
static void add_local(Token name);
static void declare_variable(void);
static uint8_t parse_variable(const char* error_message);
static void mark_initialized(void);
static void define_variable(uint8_t global);
static void and_(bool can_assign);
static void or_(bool can_assign);
static void expression_statement(void);
static void for_statement(void);
static void if_statement(void);
static void print_statement(void);
static void return_statement(void);
static void while_statement(void);
static void synchronize(void);
static void block(void);
static void function(FunctionType type);
static void method(void);
static void class_declaration(void);
static void fun_declaration(void);
static void var_declaration(void);
static void named_variable(Token name, bool can_assign);
static void variable(bool can_assign);
static void unary(bool can_assign);
static void binary(bool can_assign);
static void call(bool can_assign);
static void dot(bool can_assign);
static void literal(bool can_assign);
static void literal_rule(bool can_assign);
static void grouping(bool can_assign);
static void number(bool can_assign);
static void string_rule(bool can_assign);
static void this_(bool can_assign);
static void super_(bool can_assign);
static int argument_list(void);
static Token synthetic_token(const char* text);

/**
 * @brief Retrieves the chunk currently being compiled.
 *
 * @return Pointer to the active chunk.
 */
static Chunk* current_chunk(void) {
  return &current->function->chunk;
}

/**
 * @brief Reports a compile error at the given token.
 *
 * @param token Token where the error occurred.
 * @param message Diagnostic message to emit.
 */
static void error_at(Token* token, const char* message) {
  if (parser.panic_mode) return;
  parser.panic_mode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.had_error = true;
}

/**
 * @brief Reports an error using the previously consumed token.
 *
 * @param message Error message to emit.
 */
static void error(const char* message) {
  error_at(&parser.previous, message);
}

/**
 * @brief Reports an error at the current token.
 *
 * @param message Error message to emit.
 */
static void error_at_current(const char* message) {
  error_at(&parser.current, message);
}

/**
 * @brief Advances the parser to the next token.
 */
static void advance_parser(void) {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR) break;

    error_at_current(parser.current.start);
  }
}

/**
 * @brief Consumes the current token if it matches the expected type.
 *
 * @param type Expected token type.
 * @param message Error message when the type does not match.
 */
static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance_parser();
    return;
  }
  error_at_current(message);
}

/**
 * @brief Checks whether the current token matches the expected type.
 *
 * @param type Token type to test for.
 * @return true if the current token matches.
 */
static bool check(TokenType type) {
  return parser.current.type == type;
}

/**
 * @brief Consumes the current token when it matches the expected type.
 *
 * @param type Token type to match.
 * @return true if the token was consumed.
 */
static bool match(TokenType type) {
  if (!check(type)) return false;
  advance_parser();
  return true;
}

/**
 * @brief Emits a single byte into the current chunk.
 *
 * @param byte Bytecode instruction to emit.
 */
static void emit_byte(uint8_t byte) {
  write_chunk(current_chunk(), byte, parser.previous.line);
}

/**
 * @brief Emits two sequential bytes into the current chunk.
 *
 * @param byte1 First byte to emit.
 * @param byte2 Second byte to emit.
 */
static void emit_bytes(uint8_t byte1, uint8_t byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

/**
 * @brief Emits a loop instruction to jump back to loop_start.
 *
 * @param loop_start Offset to jump back to.
 */
static void emit_loop(int loop_start) {
  emit_byte(OP_LOOP);

  int offset = current_chunk()->count - loop_start + 2;
  if (offset > UINT16_MAX) error("Loop body too large.");

  emit_byte((offset >> 8) & 0xff);
  emit_byte(offset & 0xff);
}

/**
 * @brief Emits a jump instruction with a placeholder offset.
 *
 * @param instruction Jump opcode to emit.
 * @return Offset of the placeholder to patch later.
 */
static int emit_jump(uint8_t instruction) {
  emit_byte(instruction);
  emit_byte(0xff);
  emit_byte(0xff);
  return current_chunk()->count - 2;
}

/**
 * @brief Emits a return instruction depending on function type.
 */
static void emit_return(void) {
  if (current->type == TYPE_INITIALIZER) {
    emit_bytes(OP_GET_LOCAL, 0);
  } else {
    emit_byte(OP_NIL);
  }
  emit_byte(OP_RETURN);
}

/**
 * @brief Adds a constant to the chunk and returns its index.
 *
 * @param value Constant value to add.
 * @return Index of the constant in the pool.
 */
static uint8_t make_constant(Value value) {
  int constant = add_constant(current_chunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }
  return (uint8_t)constant;
}

/**
 * @brief Emits an OP_CONSTANT instruction for the provided value.
 *
 * @param value Value to store.
 */
static void emit_constant(Value value) {
  emit_bytes(OP_CONSTANT, make_constant(value));
}

/**
 * @brief Patches a previously emitted jump with the current offset.
 *
 * @param offset Offset returned by emit_jump.
 */
static void patch_jump(int offset) {
  int jump = current_chunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }
  current_chunk()->code[offset] = (jump >> 8) & 0xff;
  current_chunk()->code[offset + 1] = jump & 0xff;
}

/**
 * @brief Initializes a compiler for a new function scope.
 *
 * @param compiler Compiler instance to initialize.
 * @param type Function type being compiled.
 */
static void init_compiler(Compiler* compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->local_count = 0;
  compiler->scope_depth = 0;
  compiler->function = new_function();
  current = compiler;
  if (type != TYPE_SCRIPT) {
    current->function->name = copy_string(parser.previous.start, parser.previous.length);
  }

  Local* local = &current->locals[current->local_count++];
  local->depth = 0;
  local->is_captured = false;
  if (type != TYPE_FUNCTION) {
    local->name = synthetic_token("this");
  } else {
    local->name = synthetic_token("");
  }
}

/**
 * @brief Finalizes the current compiler and returns the constructed function.
 *
 * @return Compiled function object.
 */
static ObjFunction* end_compiler(void) {
  emit_return();
  ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
  }
#endif
  current = current->enclosing;
  return function;
}

/**
 * @brief Begins a new lexical scope.
 */
static void begin_scope(void) {
  current->scope_depth++;
}

/**
 * @brief Ends the current lexical scope, popping locals.
 */
static void end_scope(void) {
  current->scope_depth--;
  while (current->local_count > 0 && current->locals[current->local_count - 1].depth > current->scope_depth) {
    if (current->locals[current->local_count - 1].is_captured) {
      emit_byte(OP_CLOSE_UPVALUE);
    } else {
      emit_byte(OP_POP);
    }
    current->local_count--;
  }
}

/**
 * @see compile
 */
ObjFunction* compile(const char* source) {
  init_scanner(source);
  Compiler compiler;
  init_compiler(&compiler, TYPE_SCRIPT);
  parser.had_error = false;
  parser.panic_mode = false;

  advance_parser();
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction* function = end_compiler();
  return parser.had_error ? NULL : function;
}

/**
 * @see mark_compiler_roots
 */
void mark_compiler_roots(void) {
  Compiler* compiler = current;
  while (compiler != NULL) {
    mark_object((Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}

/**
 * @brief Parses and compiles a declaration.
 */
static void declaration(void) {
  if (match(TOKEN_CLASS)) {
    class_declaration();
  } else if (match(TOKEN_FUN)) {
    fun_declaration();
  } else if (match(TOKEN_VAR)) {
    var_declaration();
  } else {
    statement();
  }

  if (parser.panic_mode) synchronize();
}

/**
 * @brief Parses and compiles a statement.
 */
static void statement(void) {
  if (match(TOKEN_PRINT)) {
    print_statement();
  } else if (match(TOKEN_FOR)) {
    for_statement();
  } else if (match(TOKEN_IF)) {
    if_statement();
  } else if (match(TOKEN_RETURN)) {
    return_statement();
  } else if (match(TOKEN_WHILE)) {
    while_statement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    begin_scope();
    block();
    end_scope();
  } else {
    expression_statement();
  }
}

/**
 * @brief Compiles a block surrounded by braces.
 */
static void block(void) {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

/**
 * @brief Compiles a class declaration.
 */
static void class_declaration(void) {
  consume(TOKEN_IDENTIFIER, "Expect class name.");
  Token class_name = parser.previous;
  uint8_t name_constant = identifier_constant(&parser.previous);
  declare_variable();

  emit_bytes(OP_CLASS, name_constant);
  define_variable(name_constant);

  ClassCompiler class_compiler;
  class_compiler.enclosing = current_class;
  class_compiler.has_superclass = false;
  current_class = &class_compiler;

  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);
    if (identifiers_equal(&class_name, &parser.previous)) {
      error("A class cannot inherit from itself.");
    }

    begin_scope();
    add_local(synthetic_token("super"));
    define_variable(0);

    named_variable(class_name, false);
    emit_byte(OP_INHERIT);
    class_compiler.has_superclass = true;
  }

  named_variable(class_name, false);
  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  emit_byte(OP_POP);

  if (class_compiler.has_superclass) {
    end_scope();
  }

  current_class = class_compiler.enclosing;
}

/**
 * @brief Parses and compiles a method declaration.
 */
static void method(void) {
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifier_constant(&parser.previous);

  FunctionType type = TYPE_METHOD;
  if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }
  function(type);
  emit_bytes(OP_METHOD, constant);
}

/**
 * @brief Compiles a function declaration.
 */
static void fun_declaration(void) {
  uint8_t global = parse_variable("Expect function name.");
  mark_initialized();
  function(TYPE_FUNCTION);
  define_variable(global);
}

/**
 * @brief Compiles a variable declaration.
 */
static void var_declaration(void) {
  uint8_t global = parse_variable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emit_byte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  define_variable(global);
}

/**
 * @brief Compiles an expression statement.
 */
static void expression_statement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(OP_POP);
}

/**
 * @brief Compiles a print statement.
 */
static void print_statement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(OP_PRINT);
}

/**
 * @brief Compiles a return statement.
 */
static void return_statement(void) {
  if (current->type == TYPE_SCRIPT) {
    error("Cannot return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emit_return();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Cannot return a value from an initializer.");
    }
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emit_byte(OP_RETURN);
  }
}

/**
 * @brief Compiles an if statement.
 */
static void if_statement(void) {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();

  int else_jump = emit_jump(OP_JUMP);
  patch_jump(then_jump);
  emit_byte(OP_POP);

  if (match(TOKEN_ELSE)) statement();
  patch_jump(else_jump);
}

/**
 * @brief Compiles a while loop.
 */
static void while_statement(void) {
  int loop_start = current_chunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();
  emit_loop(loop_start);

  patch_jump(exit_jump);
  emit_byte(OP_POP);
}

/**
 * @brief Compiles a for loop.
 */
static void for_statement(void) {
  begin_scope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    var_declaration();
  } else {
    expression_statement();
  }

  int loop_start = current_chunk()->count;
  int exit_jump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int body_jump = emit_jump(OP_JUMP);
    int increment_start = current_chunk()->count;
    expression();
    emit_byte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    emit_loop(loop_start);
    loop_start = increment_start;
    patch_jump(body_jump);
  }

  statement();
  emit_loop(loop_start);

  if (exit_jump != -1) {
    patch_jump(exit_jump);
    emit_byte(OP_POP);
  }

  end_scope();
}

/**
 * @brief Compiles an expression.
 */
static void expression(void) {
  parse_precedence(PREC_ASSIGNMENT);
}

/**
 * @brief Parses an argument list for function calls.
 *
 * @return Number of arguments parsed.
 */
static int argument_list(void) {
  int arg_count = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (arg_count == UINT8_MAX) {
        error("Cannot have more than 255 arguments.");
      }
      arg_count++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return arg_count;
}

/**
 * @brief Parses call expressions.
 */
static void call(bool can_assign) {
  int arg_count = argument_list();
  emit_bytes(OP_CALL, (uint8_t)arg_count);
}

/**
 * @brief Parses property access and assignment.
 */
static void dot(bool can_assign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifier_constant(&parser.previous);

  if (can_assign && match(TOKEN_EQUAL)) {
    expression();
    emit_bytes(OP_SET_PROPERTY, name);
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t arg_count = (uint8_t)argument_list();
    emit_bytes(OP_INVOKE, name);
    emit_byte(arg_count);
  } else {
    emit_bytes(OP_GET_PROPERTY, name);
  }
}

/**
 * @brief Parses unary expressions.
 */
static void unary(bool can_assign) {
  TokenType operator_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (operator_type) {
    case TOKEN_BANG: emit_byte(OP_NOT); break;
    case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
    default:
      return;
  }
}

/**
 * @brief Parses binary expressions.
 */
static void binary(bool can_assign) {
  TokenType operator_type = parser.previous.type;
  ParseRule* rule = get_rule(operator_type);
  parse_precedence((Precedence)(rule->precedence + 1));

  switch (operator_type) {
    case TOKEN_BANG_EQUAL: emit_bytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
    case TOKEN_GREATER: emit_byte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS: emit_byte(OP_LESS); break;
    case TOKEN_LESS_EQUAL: emit_bytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS: emit_byte(OP_ADD); break;
    case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
    case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
    default:
      return;
  }
}

/**
 * @brief Parses literal constants.
 */
static void literal(bool can_assign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: emit_byte(OP_FALSE); break;
    case TOKEN_NIL: emit_byte(OP_NIL); break;
    case TOKEN_TRUE: emit_byte(OP_TRUE); break;
    default:
      return;
  }
}

/**
 * @brief Parses grouping expressions.
 */
static void grouping(bool can_assign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

/**
 * @brief Parses numeric literals.
 */
static void number(bool can_assign) {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

/**
 * @brief Parses string literals.
 */
static void string_rule(bool can_assign) {
  emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * @brief Parses "this" expressions.
 */
static void this_(bool can_assign) {
  if (current_class == NULL) {
    error("Cannot use 'this' outside of a class.");
    return;
  }
  variable(false);
}

/**
 * @brief Parses "super" expressions.
 */
static void super_(bool can_assign) {
  if (current_class == NULL) {
    error("Cannot use 'super' outside of a class.");
  } else if (!current_class->has_superclass) {
    error("Cannot use 'super' in a class with no superclass.");
  }
  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint8_t name = identifier_constant(&parser.previous);

  named_variable(synthetic_token("this"), false);
  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t arg_count = (uint8_t)argument_list();
    named_variable(synthetic_token("super"), false);
    emit_bytes(OP_SUPER_INVOKE, name);
    emit_byte(arg_count);
  } else {
    named_variable(synthetic_token("super"), false);
    emit_bytes(OP_GET_SUPER, name);
  }
}

/**
 * @brief Parses logical 'and' expressions.
 */
static void and_(bool can_assign) {
  int end_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  parse_precedence(PREC_AND);
  patch_jump(end_jump);
}

/**
 * @brief Parses logical 'or' expressions.
 */
static void or_(bool can_assign) {
  int else_jump = emit_jump(OP_JUMP_IF_FALSE);
  int end_jump = emit_jump(OP_JUMP);

  patch_jump(else_jump);
  emit_byte(OP_POP);

  parse_precedence(PREC_OR);
  patch_jump(end_jump);
}

/**
 * @brief Adds a local variable record.
 *
 * @param name Token representing the local name.
 */
static void add_local(Token name) {
  if (current->local_count == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local* local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
  local->is_captured = false;
}

/**
 * @brief Declares a local variable in the current scope.
 */
static void declare_variable(void) {
  if (current->scope_depth == 0) return;

  Token* name = &parser.previous;
  for (int i = current->local_count - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scope_depth) {
      break;
    }

    if (identifiers_equal(name, &local->name)) {
      error("Variable with this name already declared in this scope.");
    }
  }

  add_local(*name);
}

/**
 * @brief Parses a variable name and returns its constant index.
 *
 * @param error_message Message displayed when identifier is missing.
 * @return Index of the variable name constant.
 */
static uint8_t parse_variable(const char* error_message) {
  consume(TOKEN_IDENTIFIER, error_message);

  declare_variable();
  if (current->scope_depth > 0) return 0;

  return identifier_constant(&parser.previous);
}

/**
 * @brief Marks the most recent local as initialized.
 */
static void mark_initialized(void) {
  if (current->scope_depth == 0) return;
  current->locals[current->local_count - 1].depth = current->scope_depth;
}

/**
 * @brief Defines a variable by emitting the appropriate opcode.
 *
 * @param global Constant index for global variables.
 */
static void define_variable(uint8_t global) {
  if (current->scope_depth > 0) {
    mark_initialized();
    return;
  }

  emit_bytes(OP_DEFINE_GLOBAL, global);
}

/**
 * @brief Emits bytecode for a named variable, handling assignment.
 *
 * @param name Token representing the variable name.
 * @param can_assign Indicates whether assignment is allowed in this context.
 */
static void named_variable(Token name, bool can_assign) {
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);
  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  } else if ((arg = resolve_upvalue(current, &name)) != -1) {
    get_op = OP_GET_UPVALUE;
    set_op = OP_SET_UPVALUE;
  } else {
    arg = identifier_constant(&name);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    expression();
    emit_bytes(set_op, (uint8_t)arg);
  } else {
    emit_bytes(get_op, (uint8_t)arg);
  }
}

/**
 * @brief Parses variable access expressions.
 */
static void variable(bool can_assign) {
  named_variable(parser.previous, can_assign);
}

/**
 * @brief Parses primary expressions.
 */
static void literal_rule(bool can_assign) {
  literal(can_assign);
}

/**
 * @brief Parses expressions using Pratt parsing according to precedence.
 *
 * @param precedence Minimum precedence to parse.
 */
static void parse_precedence(Precedence precedence) {
  advance_parser();
  ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    error("Expect expression.");
    return;
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(can_assign);

  while (precedence <= get_rule(parser.current.type)->precedence) {
    advance_parser();
    ParseFn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

/**
 * @brief Returns the parse rule associated with a token type.
 *
 * @param type Token type to query.
 * @return Pointer to the parse rule.
 */
static ParseRule* get_rule(TokenType type) {
  static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string_rule, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal_rule, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal_rule, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal_rule, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
  };
  return &rules[type];
}

/**
 * @brief Synchronizes the parser after an error.
 */
static void synchronize(void) {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;
      default:
        break;
    }
    advance_parser();
  }
}

/**
 * @brief Resolves a local variable within the current compiler.
 *
 * @param compiler Compiler to search.
 * @param name Variable name token.
 * @return Local slot index or -1 when not found.
 */
static int resolve_local(Compiler* compiler, Token* name) {
  for (int i = compiler->local_count - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiers_equal(name, &local->name)) {
      if (local->depth == -1) {
        error("Cannot read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

/**
 * @brief Adds an upvalue to the compiler.
 *
 * @param compiler Compiler receiving the upvalue.
 * @param index Slot index being captured.
 * @param is_local Whether the capture refers to a local in the enclosing compiler.
 * @return Upvalue index.
 */
static int add_upvalue(Compiler* compiler, uint8_t index, bool is_local) {
  int upvalue_count = compiler->function->upvalue_count;
  for (int i = 0; i < upvalue_count; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) {
      return i;
    }
  }

  if (upvalue_count == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalue_count].is_local = is_local;
  compiler->upvalues[upvalue_count].index = index;
  compiler->function->upvalue_count++;
  return upvalue_count;
}

/**
 * @brief Resolves an upvalue by walking enclosing compilers.
 *
 * @param compiler Compiler to resolve for.
 * @param name Variable name token.
 * @return Upvalue index or -1 when not found.
 */
static int resolve_upvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolve_local(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].is_captured = true;
    return add_upvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolve_upvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return add_upvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

/**
 * @brief Computes the constant index for an identifier token.
 *
 * @param name Identifier token.
 * @return Index into the constant pool.
 */
static uint8_t identifier_constant(Token* name) {
  return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

/**
 * @brief Compares two identifier tokens for equality.
 *
 * @param a First token.
 * @param b Second token.
 * @return true if tokens represent the same identifier.
 */
static bool identifiers_equal(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * @brief Compiles a function body.
 *
 * @param type Function type being compiled.
 */
static void function(FunctionType type) {
  Compiler compiler;
  init_compiler(&compiler, type);
  begin_scope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > UINT8_MAX) {
        error_at_current("Cannot have more than 255 parameters.");
      }
      uint8_t constant = parse_variable("Expect parameter name.");
      define_variable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjFunction* function_obj = end_compiler();
  emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(function_obj)));

  for (int i = 0; i < function_obj->upvalue_count; i++) {
    emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
    emit_byte(compiler.upvalues[i].index);
  }
}

/**
 * @brief Produces a synthetic token for compiler-generated identifiers.
 *
 * @param text Text of the synthetic identifier.
 * @return Constructed token.
 */
static Token synthetic_token(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  token.line = 0;
  token.type = TOKEN_IDENTIFIER;
  return token;
}

