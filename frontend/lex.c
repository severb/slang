#include "frontend/lex.h"

#include <assert.h>  // assert
#include <limits.h>  // INT_MAX
#include <stdbool.h> // bool
#include <stddef.h>  // ptrdiff_t
#include <stdint.h>  // SIZE_MAX
#include <stdio.h>   // printf
#include <string.h>  // strlen

extern inline Lexer lex(char const *);

static void token(const Lexer *lex, TokenType type, Token *t) {
  assert(lex->current >= lex->start);
  *t = (Token){
      .type = type,
      .start = lex->start,
      .end = lex->current,
      .line = lex->line,
  };
}

static void error(const Lexer *lex, const char *msg, Token *t) {
  *t = (Token){
      .type = TOKEN_ERROR,
      .start = msg,
      .end = &msg[strlen(msg) - 1],
      .line = lex->line,
  };
}

static bool is_digit(char c) { return '0' <= c && c <= '9'; }
static bool is_alpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static char peek(const Lexer *lex) { return *lex->current; }
static bool is_at_eof(const Lexer *lex) { return peek(lex) == '\0'; }
static bool is_at_eol(const Lexer *lex) { return peek(lex) == '\n'; }

static char peek_next(const Lexer *lex) {
  if (is_at_eof(lex)) {
    return '\0';
  }
  return lex->current[1];
}

static char advance(Lexer *lex) {
  char next = peek(lex);
  if (!is_at_eof(lex)) {
    lex->current++;
  }
  return next;
}

static bool match(Lexer *lex, char expected) {
  if (!is_at_eof(lex) && peek(lex) == expected) {
    advance(lex);
    return true;
  }
  return false;
}

static void skip_whitespace(Lexer *lex) {
  for (;;) {
    char c = peek(lex);
    switch (c) {
    case '\n':
      lex->line++; // fallthrough
    case ' ':
    case '\r':
    case '\t':
      advance(lex);
      break;
    case '/':
      if (peek_next(lex) != '/') {
        return;
      }
      advance(lex);
      advance(lex);
      while (!is_at_eof(lex) && !is_at_eol(lex)) {
        advance(lex);
      }
      break;
    default:
      return;
    }
  }
}

static void string(Lexer *lex, Token *t, char type) {
  while (peek(lex) != type && !is_at_eol(lex) && !is_at_eof(lex)) {
    advance(lex);
  }
  if (is_at_eol(lex)) {
    error(lex, "unterminated string at end of line", t);
    return;
  }
  if (is_at_eof(lex)) {
    error(lex, "unterminated string at end of file", t);
  }
  advance(lex); // closing char
  token(lex, TOKEN_STRING, t);
}

static void number(Lexer *lex, Token *t) {
  char c;
  while (is_digit(c = peek(lex))) {
    advance(lex);
  }
  if (c == '.' && is_digit(peek_next(lex))) {
    advance(lex); // dot
    advance(lex); // 1st digit after dot
    while (is_digit(peek(lex))) {
      advance(lex);
    }
    token(lex, TOKEN_FLOAT, t);
    return;
  }
  token(lex, TOKEN_INT, t);
}

static TokenType check_keyword(const Lexer *lex, short start, short len,
                               const char *rest, TokenType type) {
  ptrdiff_t kw_len = lex->current - lex->start;
  assert(kw_len >= 0);
  short expected_len = start + len;
  if (kw_len == expected_len) {
    const char *kw_rest = &lex->start[start];
    if (memcmp(kw_rest, rest, len) == 0) {
      return type;
    }
  }
  return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(const Lexer *lex) {
  assert(lex->current >= lex->start);
  switch (*lex->start) {
  case 'a':
    return check_keyword(lex, 1, 2, "nd", TOKEN_AND);
  case 'c':
    return check_keyword(lex, 1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return check_keyword(lex, 1, 3, "lse", TOKEN_ELSE);
  case 'i':
    return check_keyword(lex, 1, 1, "f", TOKEN_IF);
  case 'n':
    return check_keyword(lex, 1, 2, "il", TOKEN_NIL);
  case 'o':
    return check_keyword(lex, 1, 1, "r", TOKEN_OR);
  case 'p':
    return check_keyword(lex, 1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return check_keyword(lex, 1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return check_keyword(lex, 1, 4, "uper", TOKEN_SUPER);
  case 'v':
    return check_keyword(lex, 1, 2, "ar", TOKEN_VAR);
  case 'w':
    return check_keyword(lex, 1, 4, "hile", TOKEN_WHILE);
  case 'f':
    if (lex->current - lex->start > 1) {
      switch (lex->start[1]) {
      case 'a':
        return check_keyword(lex, 2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return check_keyword(lex, 2, 1, "r", TOKEN_FOR);
      case 'u':
        return check_keyword(lex, 2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 't':
    if (lex->current - lex->start > 1) {
      switch (lex->start[1]) {
      case 'h':
        return check_keyword(lex, 2, 2, "is", TOKEN_THIS);
      case 'r':
        return check_keyword(lex, 2, 2, "ue", TOKEN_TRUE);
      }
    }
    break;
  }
  return TOKEN_IDENTIFIER;
}

static void identifier(Lexer *lex, Token *t) {
  while (is_alpha(peek(lex)) || is_digit(peek(lex))) {
    advance(lex);
  }
  token(lex, identifier_type(lex), t);
}

void lex_consume(Lexer *lex, Token *t) {
  skip_whitespace(lex);
  lex->start = lex->current;
  if (is_at_eof(lex)) {
    token(lex, TOKEN_EOF, t);
    return;
  }
  char c = advance(lex);
  if (is_digit(c)) {
    number(lex, t);
    return;
  }
  if (is_alpha(c)) {
    identifier(lex, t);
    return;
  }
  switch (c) {
  case '\'':
    string(lex, t, '\'');
    return;
  case '\"':
    string(lex, t, '\"');
    return;
  case '(':
    token(lex, TOKEN_LEFT_PAREN, t);
    return;
  case ')':
    token(lex, TOKEN_RIGHT_PAREN, t);
    return;
  case '{':
    token(lex, TOKEN_LEFT_BRACE, t);
    return;
  case '}':
    token(lex, TOKEN_RIGHT_BRACE, t);
    return;
  case ';':
    token(lex, TOKEN_SEMICOLON, t);
    return;
  case ',':
    token(lex, TOKEN_COMMA, t);
    return;
  case '.':
    token(lex, TOKEN_DOT, t);
    return;
  case '-':
    token(lex, TOKEN_MINUS, t);
    return;
  case '+':
    token(lex, TOKEN_PLUS, t);
    return;
  case '/':
    token(lex, TOKEN_SLASH, t);
    return;
  case '*':
    token(lex, TOKEN_STAR, t);
    return;
  case '!':
    token(lex, match(lex, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG, t);
    return;
  case '<':
    token(lex, match(lex, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS, t);
    return;
  case '>':
    token(lex, match(lex, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER, t);
    return;
  case '=':
    token(lex, match(lex, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL, t);
    return;
  }
  error(lex, "unexpected character", t);
}

#define TOKEN(STRING) #STRING,
static char const *TOKEN_TO_STRING[] = {
#include "lex_tokens.inc"
};
#undef TOKEN

void lex_print(const Lexer *lex) {
  Lexer copy = *lex;
  size_t line = SIZE_MAX;
  Token token;
  for (;;) {
    lex_consume(&copy, &token);
    if (token.line != line) {
      printf("%4zu ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    if ((token.end - token.start) < INT_MAX) {
      printf("%17s %2d %.*s\n", TOKEN_TO_STRING[token.type], token.type,
             (int)(token.end - token.start), token.start);
    } else {
      printf("%17s %2d %.*s...\n", TOKEN_TO_STRING[token.type], token.type,
             INT_MAX, token.start);
    }
    if (token.type == TOKEN_EOF) {
      break;
    }
  }
}
