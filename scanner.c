#include "scanner.h"

#include <assert.h>  // assert
#include <limits.h>  // INT_MAX
#include <stdbool.h> // bool
#include <stdint.h>  // uintptr_t
#include <stdio.h>   // printf
#include <string.h>  // memcmp

#define GENERATE_STRING(STRING) #STRING,
static const char *TOKEN_TO_STRING[] = {FOREACH_TOKEN(GENERATE_STRING)};
#undef GENERATE_STRING

Scanner *scanner_init(Scanner *scanner, const char *source) {
  if (scanner != 0)
    *scanner = (Scanner){
        .start = source,
        .current = source,
        .line = 0,
    };
  return scanner;
}

static void token(Scanner const *scanner, TokenType type, Token *t) {
  assert(scanner->current >= scanner->start);
  *t = (Token){
      .type = type,
      .start = scanner->start,
      .len = (size_t)((uintptr_t)scanner->current - (uintptr_t)scanner->start),
      .line = scanner->line,
  };
}

static void error(Scanner const *scanner, const char *msg, Token *t) {
  *t = (Token){
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

static char peek(Scanner const *scanner) { return *scanner->current; }

static bool is_at_eof(Scanner const *scanner) { return peek(scanner) == '\0'; }
static bool is_at_eol(Scanner const *scanner) { return peek(scanner) == '\n'; }

static char peek_next(Scanner const *scanner) {
  if (is_at_eof(scanner))
    return '\0';
  return scanner->current[1];
}

static char advance(Scanner *scanner) {
  char res = peek(scanner);
  scanner->current++;
  return res;
}

static bool match(Scanner *scanner, char expected) {
  if (is_at_eof(scanner))
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
      // fallthrough
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
      while (!(is_at_eol(scanner) || is_at_eof(scanner))) {
        advance(scanner);
      }
      break;
    default:
      return;
    }
  }
}

static void string(Scanner *scanner, Token *t) {
  while (peek(scanner) != '\'' && !is_at_eol(scanner) && !is_at_eof(scanner)) {
    advance(scanner);
  }
  if (is_at_eol(scanner)) {
    error(scanner, "Unterminated string at end of line.", t);
    return;
  }
  if (is_at_eof(scanner)) {
    error(scanner, "Unterminated string at end of file.", t);
    return;
  }
  advance(scanner); // the closing quote.
  token(scanner, TOKEN_STRING, t);
}

static void number(Scanner *scanner, Token *t) {
  char c;
  while (is_digit(c = peek(scanner))) {
    advance(scanner);
  }
  if (c == '.' && is_digit(peek_next(scanner))) {
    advance(scanner); // dot
    advance(scanner); // 1st digit after dot
    while (is_digit(peek(scanner))) {
      advance(scanner);
    }
  }
  token(scanner, TOKEN_NUMBER, t);
}

static TokenType check_keyword(Scanner const *scanner, uint8_t start,
                               uint8_t len, char const *rest, TokenType type) {
  assert(scanner->current >= scanner->start);
  size_t kw_len = scanner->current - scanner->start;
  size_t expected_len = (size_t)start + (size_t)len;
  const char *kw_rest = scanner->start + start;
  if (kw_len == expected_len && memcmp(kw_rest, rest, len) == 0) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(Scanner const *scanner) {
  assert(scanner->current >= scanner->start);
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

static void identifier(Scanner *scanner, Token *t) {
  while (is_alpha(peek(scanner)) || is_digit(peek(scanner))) {
    advance(scanner);
  }
  token(scanner, identifier_type(scanner), t);
}

void scanner_consume(Scanner *scanner, Token *t) {
  skip_whitespace(scanner);
  scanner->start = scanner->current;
  if (is_at_eof(scanner)) {
    token(scanner, TOKEN_EOF, t);
    return;
  }
  char c = advance(scanner);
  if (is_digit(c)) {
    number(scanner, t);
    return;
  }
  if (is_alpha(c)) {
    identifier(scanner, t);
    return;
  }
  switch (c) {
  case '\'':
    string(scanner, t);
    return;
  case '(':
    token(scanner, TOKEN_LEFT_PAREN, t);
    return;
  case ')':
    token(scanner, TOKEN_RIGHT_PAREN, t);
    return;
  case '{':
    token(scanner, TOKEN_LEFT_BRACE, t);
    return;
  case '}':
    token(scanner, TOKEN_RIGHT_BRACE, t);
    return;
  case ';':
    token(scanner, TOKEN_SEMICOLON, t);
    return;
  case ',':
    token(scanner, TOKEN_COMMA, t);
    return;
  case '.':
    token(scanner, TOKEN_DOT, t);
    return;
  case '-':
    token(scanner, TOKEN_MINUS, t);
    return;
  case '+':
    token(scanner, TOKEN_PLUS, t);
    return;
  case '/':
    token(scanner, TOKEN_SLASH, t);
    return;
  case '*':
    token(scanner, TOKEN_STAR, t);
    return;
  case '!':
    token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG, t);
    return;
  case '<':
    token(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS, t);
    return;
  case '>':
    token(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER,
          t);
    return;
  case '=':
    token(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL, t);
    return;
  }
  error(scanner, "Unexpected character.", t);
}

void scanner_print(Scanner const *scanner) {
  Scanner copy = *scanner;
  size_t line = SIZE_MAX;
  Token token;
  for (;;) {
    scanner_consume(&copy, &token);
    if (token.line != line) {
      printf("%4zu ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    if (token.len < INT_MAX) {
      printf("%17s %2d %.*s\n", TOKEN_TO_STRING[token.type], token.type,
             (int)token.len, token.start);
    } else {
      printf("%17s %2d %.*s...\n", TOKEN_TO_STRING[token.type], token.type,
             INT_MAX, token.start);
    }
    if (token.type == TOKEN_EOF) {
      break;
    }
  }
}
