#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#define GENERATE_STRING(STRING) #STRING,
static const char *TOKEN_TO_STRING[] = {FOREACH_TOKEN(GENERATE_STRING)};
#undef GENERATE_STRING

void scanner_init(Scanner *scanner, const char *source) {
  scanner->start = source;
  scanner->current = source;
  scanner->line = 0;
}

static Token token(Scanner *scanner, TokenType type) {
  return (Token){
      .type = type,
      .start = scanner->start,
      .len = (size_t)(scanner->current - scanner->start),
      .line = scanner->line,
  };
}

static Token token_error(Scanner *scanner, const char *msg) {
  return (Token){
      .type = TOKEN_ERROR,
      .start = msg,
      .len = (size_t)(strlen(msg)),
      .line = scanner->line,
  };
}

static bool is_digit(char c) { return '0' <= c && c <= '9'; }

static bool is_alpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static char peek(Scanner *scanner) { return scanner->current[0]; }

static bool is_at_end(Scanner *scanner) { return peek(scanner) == '\0'; }

static char peek_next(Scanner *scanner) {
  if (is_at_end(scanner))
    return '\0';
  return scanner->current[1];
}

static char advance(Scanner *scanner) {
  char res = peek(scanner);
  scanner->current++;
  return res;
}

static bool match(Scanner *scanner, char expected) {
  if (is_at_end(scanner))
    return false;
  if (peek(scanner) != expected)
    return false;
  advance(scanner);
  return true;
}

static void skip_whitespace(Scanner *scanner) {
  for (;;) {
    char c = peek(scanner);
    switch (c) {
    case '\n':
      scanner->line++;
    case ' ':
    case '\r':
    case '\t':
      advance(scanner);
      break;
    case '/':
      if (peek_next(scanner) != '/')
        return;
      advance(scanner);
      advance(scanner);
      while (peek(scanner) != '\n' && !is_at_end(scanner))
        advance(scanner);
      break;
    default:
      return;
    }
  }
}

static Token string(Scanner *scanner) {
  char c;
  while ((c = peek(scanner)) != '\'' && c != '\n' && !is_at_end(scanner))
    advance(scanner);
  if (peek(scanner) == '\n')
    return token_error(scanner, "Unterminated string at end of line.");
  if (is_at_end(scanner))
    return token_error(scanner, "Unterminated string at end of file.");
  advance(scanner); // the closing quote.
  return token(scanner, TOKEN_STRING);
}

static Token number(Scanner *scanner) {
  char c;
  while (is_digit(c = peek(scanner)))
    advance(scanner);
  if (c == '.' && is_digit(peek_next(scanner))) {
    advance(scanner);
    advance(scanner);
    while (is_digit(peek(scanner)))
      advance(scanner);
  }
  return token(scanner, TOKEN_NUMBER);
}

static TokenType check_keyword(Scanner *scanner, size_t start, size_t len,
                               const char *rest, TokenType type) {
  size_t kw_len = scanner->current - scanner->start;
  size_t expected_len = start + len;
  const char *kw_rest = scanner->start + start;
  if ((kw_len == expected_len) && memcmp(kw_rest, rest, len) == 0)
    return type;
  return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(Scanner *scanner) {
  switch (*scanner->start) {
  case 'a':
    return check_keyword(scanner, 1, 2, "nd", TOKEN_AND);
  case 'c':
    return check_keyword(scanner, 1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
  case 'i':
    return check_keyword(scanner, 1, 1, "f", TOKEN_IF);
  case 'n':
    return check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
  case 'o':
    return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
  case 'p':
    return check_keyword(scanner, 1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return check_keyword(scanner, 1, 4, "uper", TOKEN_SUPER);
  case 'v':
    return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
  case 'w':
    return check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
  case 'f':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'a':
        return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
      case 'u':
        return check_keyword(scanner, 2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 't':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'h':
        return check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
      case 'r':
        return check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
      }
    }
    break;
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
  while (is_alpha(peek(scanner)) || is_digit(peek(scanner)))
    advance(scanner);
  return token(scanner, identifier_type(scanner));
}

Token scanner_consume(Scanner *scanner) {
  skip_whitespace(scanner);
  scanner->start = scanner->current;
  if (is_at_end(scanner))
    return token(scanner, TOKEN_EOF);
  char c = advance(scanner);
  if (is_digit(c))
    return number(scanner);
  if (is_alpha(c))
    return identifier(scanner);
  switch (c) {
  case '\'':
    return string(scanner);
  case '(':
    return token(scanner, TOKEN_LEFT_PAREN);
  case ')':
    return token(scanner, TOKEN_RIGHT_PAREN);
  case '{':
    return token(scanner, TOKEN_LEFT_BRACE);
  case '}':
    return token(scanner, TOKEN_RIGHT_BRACE);
  case ';':
    return token(scanner, TOKEN_SEMICOLON);
  case ',':
    return token(scanner, TOKEN_COMMA);
  case '.':
    return token(scanner, TOKEN_DOT);
  case '-':
    return token(scanner, TOKEN_MINUS);
  case '+':
    return token(scanner, TOKEN_PLUS);
  case '/':
    return token(scanner, TOKEN_SLASH);
  case '*':
    return token(scanner, TOKEN_STAR);
  case '!':
    return token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '<':
    return token(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return token(scanner,
                 match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '=':
    return token(scanner,
                 match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  }
  return token_error(scanner, "Unexpected character.");
}

void scanner_print(Scanner scanner) {
  int line = -1;
  for (;;) {
    Token token = scanner_consume(&scanner);
    if (token.line != line) {
      printf("%4zu ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    printf("%17s %2d %.*s\n", TOKEN_TO_STRING[token.type], token.type,
           (int)token.len, token.start);
    if (token.type == TOKEN_EOF)
      break;
  }
}
