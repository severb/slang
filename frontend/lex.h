#ifndef slang_lex_h
#define slang_lex_h

#include <stddef.h> // size_t

#define TOKEN(ENUM) ENUM,
typedef enum {
#include "lex_tokens.inc"
} TokenType;
#undef TOKEN

typedef struct {
  char const *start;
  char const *current;
  size_t line;
} Lexer;

typedef struct {
  char const *start;
  char const *end;
  size_t line;
  TokenType type;
} Token;

inline Lexer lex(char const *c) {
  return (Lexer){.start = c, .current = c, .line = 0};
}
void lex_consume(Lexer *, Token *);
void lex_print(const Lexer *);

#endif
