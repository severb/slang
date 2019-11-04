#ifndef clox_scanner_h
#define clox_scanner_h

#include <stddef.h> // size_t

#define FOREACH_TOKEN(TOKEN)                                                   \
  TOKEN(TOKEN_LEFT_PAREN)                                                      \
  TOKEN(TOKEN_RIGHT_PAREN)                                                     \
  TOKEN(TOKEN_LEFT_BRACE)                                                      \
  TOKEN(TOKEN_RIGHT_BRACE)                                                     \
  TOKEN(TOKEN_COMMA)                                                           \
  TOKEN(TOKEN_DOT)                                                             \
  TOKEN(TOKEN_MINUS)                                                           \
  TOKEN(TOKEN_PLUS)                                                            \
  TOKEN(TOKEN_SEMICOLON)                                                       \
  TOKEN(TOKEN_SLASH)                                                           \
  TOKEN(TOKEN_STAR)                                                            \
                                                                               \
  TOKEN(TOKEN_BANG)                                                            \
  TOKEN(TOKEN_BANG_EQUAL)                                                      \
  TOKEN(TOKEN_EQUAL)                                                           \
  TOKEN(TOKEN_EQUAL_EQUAL)                                                     \
  TOKEN(TOKEN_GREATER)                                                         \
  TOKEN(TOKEN_GREATER_EQUAL)                                                   \
  TOKEN(TOKEN_LESS)                                                            \
  TOKEN(TOKEN_LESS_EQUAL)                                                      \
                                                                               \
  TOKEN(TOKEN_IDENTIFIER)                                                      \
  TOKEN(TOKEN_STRING)                                                          \
  TOKEN(TOKEN_NUMBER)                                                          \
                                                                               \
  TOKEN(TOKEN_AND)                                                             \
  TOKEN(TOKEN_CLASS)                                                           \
  TOKEN(TOKEN_ELSE)                                                            \
  TOKEN(TOKEN_FALSE)                                                           \
  TOKEN(TOKEN_FOR)                                                             \
  TOKEN(TOKEN_FUN)                                                             \
  TOKEN(TOKEN_IF)                                                              \
  TOKEN(TOKEN_NIL)                                                             \
  TOKEN(TOKEN_OR)                                                              \
  TOKEN(TOKEN_PRINT)                                                           \
  TOKEN(TOKEN_RETURN)                                                          \
  TOKEN(TOKEN_SUPER)                                                           \
  TOKEN(TOKEN_THIS)                                                            \
  TOKEN(TOKEN_TRUE)                                                            \
  TOKEN(TOKEN_VAR)                                                             \
  TOKEN(TOKEN_WHILE)                                                           \
                                                                               \
  TOKEN(TOKEN_ERROR)                                                           \
  TOKEN(TOKEN_EOF)

#define GENERATE_ENUM(ENUM) ENUM,
typedef enum { FOREACH_TOKEN(GENERATE_ENUM) } TokenType;
#undef GENERATE_ENUM

typedef struct {
  char const *start;
  char const *current;
  size_t line;
} Scanner;

typedef struct {
  TokenType type;
  char const *start;
  size_t len;
  size_t line;
} Token;

Scanner *scanner_init(Scanner *, char const *);
void scanner_consume(Scanner *, Token *);
void scanner_print(Scanner const *);

#endif
