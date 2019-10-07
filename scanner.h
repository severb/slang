#ifndef clox_scanner_h
#define clox_scanner_h

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
extern const char *TOKEN_TO_STRING[];

typedef struct {
  const char *start;
  const char *current;
  int line;
} Scanner;

typedef struct {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

void scannerInit(Scanner *scanner, const char *source);
Token scannerConsumeToken(Scanner *scanner);

#endif
