#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#define GENERATE_STRING(STRING) #STRING,
const char *TOKEN_TO_STRING[] = {FOREACH_TOKEN(GENERATE_STRING)};
#undef GENERATE_STRING

void scannerInit(Scanner *scanner, const char *source) {
  scanner->start = source;
  scanner->current = source;
  scanner->line = 0;
}

static Token makeToken(Scanner *scanner, TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner->start;
  token.length = (int)(scanner->current - scanner->start);
  token.line = scanner->line;
  return token;
}

static Token makeErrorToken(Scanner *scanner, const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner->line;
  return token;
}

static bool isDigit(char c) { return '0' <= c && c <= '9'; }
static bool isAlpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static char peek(Scanner *scanner) { return scanner->current[0]; }
static bool isAtEnd(Scanner *scanner) { return peek(scanner) == '\0'; }
static char peekNext(Scanner *scanner) {
  if (isAtEnd(scanner))
    return '\0';
  return scanner->current[1];
}
static void bumpLine(Scanner *scanner) { scanner->line++; }

static char advance(Scanner *scanner) {
  char result = peek(scanner);
  scanner->current++;
  return result;
}

static bool match(Scanner *scanner, char expected) {
  if (isAtEnd(scanner))
    return false;
  if (peek(scanner) != expected)
    return false;
  advance(scanner);
  return true;
}

static void skipWhitespace(Scanner *scanner) {
  for (;;) {
    char c = peek(scanner);
    switch (c) {

    case '\n':
      bumpLine(scanner);
    case ' ':
    case '\r':
    case '\t':
      advance(scanner);
      break;

    case '/':
      if (peekNext(scanner) != '/')
        return;
      advance(scanner);
      advance(scanner);
      while (peek(scanner) != '\n' && !isAtEnd(scanner))
        advance(scanner);
      break;

    default:
      return;
    }
  }
}

static Token string(Scanner *scanner) {
  char c;
  while ((c = peek(scanner)) != '\'' && !isAtEnd(scanner)) {
    if (c == '\n')
      bumpLine(scanner);
    advance(scanner);
  }

  if (isAtEnd(scanner))
    return makeErrorToken(scanner, "Unterminated string.");

  advance(scanner); // the closing quote.
  return makeToken(scanner, TOKEN_STRING);
}

static Token number(Scanner *scanner) {
  char c;
  while (isDigit(c = peek(scanner)))
    advance(scanner);

  if (c == '.' && isDigit(peekNext(scanner))) {
    advance(scanner);
    advance(scanner);
    while (isDigit(peek(scanner)))
      advance(scanner);
  }

  return makeToken(scanner, TOKEN_NUMBER);
}

static TokenType checkKeyword(Scanner *scanner, int start, int length,
                              const char *rest, TokenType type) {
  int kwLength = scanner->current - scanner->start;
  int expectedLength = start + length;
  const char *kwRest = scanner->start + start;
  if ((kwLength == expectedLength) && memcmp(kwRest, rest, length) == 0)
    return type;
  return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
  switch (*scanner->start) {
  case 'a':
    return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
  case 'c':
    return checkKeyword(scanner, 1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
  case 'i':
    return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
  case 'n':
    return checkKeyword(scanner, 1, 2, "il", TOKEN_NIL);
  case 'o':
    return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
  case 'p':
    return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
  case 'v':
    return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
  case 'w':
    return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);

  case 'f':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'a':
        return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
      case 'u':
        return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
      }
    }
    break;

  case 't':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'h':
        return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
      case 'r':
        return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
      }
    }
    break;
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
  while (isAlpha(peek(scanner)) || isDigit(peek(scanner)))
    advance(scanner);
  return makeToken(scanner, identifierType(scanner));
}

Token scannerConsumeToken(Scanner *scanner) {
  skipWhitespace(scanner);

  scanner->start = scanner->current;
  if (isAtEnd(scanner))
    return makeToken(scanner, TOKEN_EOF);

  char c = advance(scanner);

  if (isDigit(c))
    return number(scanner);
  if (isAlpha(c))
    return identifier(scanner);

  switch (c) {
  case '(':
    return makeToken(scanner, TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(scanner, TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(scanner, TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(scanner, TOKEN_RIGHT_BRACE);
  case ';':
    return makeToken(scanner, TOKEN_SEMICOLON);
  case ',':
    return makeToken(scanner, TOKEN_COMMA);
  case '.':
    return makeToken(scanner, TOKEN_DOT);
  case '-':
    return makeToken(scanner, TOKEN_MINUS);
  case '+':
    return makeToken(scanner, TOKEN_PLUS);
  case '/':
    return makeToken(scanner, TOKEN_SLASH);
  case '*':
    return makeToken(scanner, TOKEN_STAR);
  case '!':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

  case '\'':
    return string(scanner);
  }

  return makeErrorToken(scanner, "Unexpected character.");
}
