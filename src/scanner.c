/**
 * @file scanner.c
 * @brief Implements lexical analysis for MLOX source code.
 */
#include "scanner.h"

#include <string.h>

/**
 * @brief Maintains scanner state while tokenizing a source string.
 */
typedef struct {
  const char* start; /**< Start of the current lexeme. */
  const char* current; /**< Current scanning position. */
  int line; /**< Current line number. */
} Scanner;

static Scanner scanner;

/**
 * @brief Determines if the scanner has reached the end of the source.
 *
 * @return true if no more characters remain.
 */
static bool is_at_end(void);

/**
 * @brief Advances the scanner and returns the consumed character.
 *
 * @return Previously current character.
 */
static char advance(void);

/**
 * @brief Peeks at the current character without consuming it.
 *
 * @return Current character or '\0' at end of source.
 */
static char peek(void);

/**
 * @brief Peeks at the next character without consuming it.
 *
 * @return Next character or '\0' at end of source.
 */
static char peek_next(void);

/**
 * @brief Conditionally consumes the next character when matching expected.
 *
 * @param expected Character to match.
 * @return true if the character was consumed.
 */
static bool match(char expected);

/**
 * @brief Consumes whitespace and comments between tokens.
 */
static void skip_whitespace(void);

/**
 * @brief Emits a token of the provided type.
 *
 * @param type Token type to emit.
 * @return Constructed token.
 */
static Token make_token(TokenType type);

/**
 * @brief Emits an error token with the provided message.
 *
 * @param message Error message to store in the token.
 * @return Constructed error token.
 */
static Token error_token(const char* message);

/**
 * @brief Parses an identifier token, resolving keyword types.
 *
 * @return Parsed identifier token.
 */
static Token identifier(void);

/**
 * @brief Determines the keyword token type for the current identifier.
 *
 * @return Keyword token type or TOKEN_IDENTIFIER when not a keyword.
 */
static TokenType identifier_type(void);

/**
 * @brief Helper to match a keyword with two substrings.
 *
 * @param start Offset from the lexeme start.
 * @param length Expected length of the suffix.
 * @param rest Expected suffix string.
 * @param type Token type to return on match.
 * @return Matched token type or TOKEN_IDENTIFIER.
 */
static TokenType check_keyword(int start, int length, const char* rest, TokenType type);

/**
 * @brief Parses a numeric literal token.
 *
 * @return Parsed number token.
 */
static Token number(void);

/**
 * @brief Parses a string literal token.
 *
 * @return Parsed string token or error token on unterminated literal.
 */
static Token string(void);

/**
 * @brief Determines whether a character is alphabetical or underscore.
 *
 * @param c Character to test.
 * @return true when the character is alphabetical.
 */
static bool is_alpha(char c);

/**
 * @brief Determines whether a character is a digit.
 *
 * @param c Character to test.
 * @return true when the character is a digit.
 */
static bool is_digit(char c);

/**
 * @see init_scanner
 */
void init_scanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

/**
 * @see scan_token
 */
Token scan_token(void) {
  skip_whitespace();
  scanner.start = scanner.current;

  if (is_at_end()) return make_token(TOKEN_EOF);

  char c = advance();
  if (is_alpha(c)) return identifier();
  if (is_digit(c)) return number();

  switch (c) {
    case '(': return make_token(TOKEN_LEFT_PAREN);
    case ')': return make_token(TOKEN_RIGHT_PAREN);
    case '{': return make_token(TOKEN_LEFT_BRACE);
    case '}': return make_token(TOKEN_RIGHT_BRACE);
    case ';': return make_token(TOKEN_SEMICOLON);
    case ',': return make_token(TOKEN_COMMA);
    case '.': return make_token(TOKEN_DOT);
    case '-': return make_token(TOKEN_MINUS);
    case '+': return make_token(TOKEN_PLUS);
    case '/':
      return make_token(TOKEN_SLASH);
    case '*': return make_token(TOKEN_STAR);
    case '!':
      return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
      return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
      return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
      return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"':
      return string();
  }

  return error_token("Unexpected character.");
}

/**
 * @see skip_whitespace
 */
static void skip_whitespace(void) {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ': 
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        scanner.line++;
        advance();
        break;
      case '/':
        if (peek_next() == '/') {
          while (peek() != '\n' && !is_at_end()) advance();
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

/**
 * @see is_at_end
 */
static bool is_at_end(void) {
  return *scanner.current == '\0';
}

/**
 * @see advance
 */
static char advance(void) {
  scanner.current++;
  return scanner.current[-1];
}

/**
 * @see peek
 */
static char peek(void) {
  return *scanner.current;
}

/**
 * @see peek_next
 */
static char peek_next(void) {
  if (is_at_end()) return '\0';
  return scanner.current[1];
}

/**
 * @see match
 */
static bool match(char expected) {
  if (is_at_end()) return false;
  if (*scanner.current != expected) return false;
  scanner.current++;
  return true;
}

/**
 * @see make_token
 */
static Token make_token(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

/**
 * @see error_token
 */
static Token error_token(const char* message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

/**
 * @see identifier
 */
static Token identifier(void) {
  while (is_alpha(peek()) || is_digit(peek())) advance();
  return make_token(identifier_type());
}

/**
 * @see identifier_type
 */
static TokenType identifier_type(void) {
  switch (scanner.start[0]) {
    case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
    case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
    case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
          case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
        }
      }
      break;
    case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
    case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
    case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
    case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
    case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
    case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
          case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
    case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
  }
  return TOKEN_IDENTIFIER;
}

/**
 * @see check_keyword
 */
static TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
  if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}

/**
 * @see number
 */
static Token number(void) {
  while (is_digit(peek())) advance();

  if (peek() == '.' && is_digit(peek_next())) {
    advance();
    while (is_digit(peek())) advance();
  }

  return make_token(TOKEN_NUMBER);
}

/**
 * @see string
 */
static Token string(void) {
  while (peek() != '"' && !is_at_end()) {
    if (peek() == '\n') scanner.line++;
    advance();
  }

  if (is_at_end()) return error_token("Unterminated string.");

  advance();
  return make_token(TOKEN_STRING);
}

/**
 * @see is_alpha
 */
static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * @see is_digit
 */
static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}
