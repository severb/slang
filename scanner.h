#ifndef clox_scanner_h
#define clox_scanner_h

#include <stddef.h> // size_t

#define TOKEN(ENUM) ENUM,
typedef enum {
#include "tokens.enum"
} TokenType;
#undef TOKEN

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
