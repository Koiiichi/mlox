/**
 * @file scanner.h
 * @brief Lexical scanner converting source code into tokens.
 */
#ifndef MLOX_SCANNER_H
#define MLOX_SCANNER_H

#include "common.h"

/**
 * @brief Token types recognized by the MLOX scanner.
 */
typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
  TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,

  // One or two character tokens.
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,

  // Literals.
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

  // Keywords.
  TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
  TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
  TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

  TOKEN_ERROR,
  TOKEN_EOF
} TokenType;

/**
 * @brief Represents a single lexed token with associated metadata.
 */
typedef struct {
  TokenType type; /**< Token type identifier. */
  const char* start; /**< Pointer into the source string. */
  int length; /**< Length of the lexeme. */
  int line; /**< Source line number for diagnostics. */
} Token;

/**
 * @brief Initializes scanner state for a new source string.
 *
 * @param source Null-terminated source buffer to scan.
 */
void init_scanner(const char* source);

/**
 * @brief Scans and returns the next token from the source.
 *
 * @return The next token in sequence.
 */
Token scan_token(void);

#endif /* MLOX_SCANNER_H */
