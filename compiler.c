#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "table.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  Scanner *scanner;
  Table *interned;
  Chunk *chunk;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . () []
  PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(Parser *);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static ParseRule rules[TOKEN_EOF + 1];

void static initParser(Parser *parser, Scanner *scanner, Chunk *chunk,
                       Table *interned) {
  Token emptyToken;
  emptyToken.type = TOKEN_ERROR;
  emptyToken.start = "Unitialized token.";
  emptyToken.length = (int)strlen(emptyToken.start);
  emptyToken.line = -1;
  parser->current = emptyToken;
  parser->previous = emptyToken;
  parser->scanner = scanner;
  parser->interned = interned;
  parser->chunk = chunk;
  parser->hadError = false;
  parser->panicMode = false;
}

// ERROR ----------------------------------------------------------------------

static void errorPrint(Token token, const char *message) {
  fprintf(stderr, "[line %d] Error", token.line);
  switch (token.type) {
  case TOKEN_EOF:
    fprintf(stderr, " at end");
  case TOKEN_ERROR:
    break;
  default:
    fprintf(stderr, " at '%.*s'", token.length, token.start);
    break;
  }
  fprintf(stderr, ": %s\n",
          (token.type == TOKEN_ERROR) ? token.start : message);
}

static bool errorPanic(Parser *parser) {
  bool alreadyPanicked = parser->panicMode;
  parser->panicMode = true;
  return alreadyPanicked;
}

static void errorUnpanic(Parser *parser) { parser->panicMode = false; }

static void errorAtCurrent(Parser *parser, const char *message) {
  if (errorPanic(parser))
    return;
  parser->hadError = true;
  errorPrint(parser->current, message);
}

static void errorAtPrev(Parser *parser, const char *message) {
  if (errorPanic(parser))
    return;
  parser->hadError = true;
  parser->panicMode = true;
  errorPrint(parser->previous, message);
}

// SCAN -----------------------------------------------------------------------

static void scanAdvance(Parser *parser) {
  parser->previous = parser->current;
  for (;;) {
    parser->current = scannerConsumeToken(parser->scanner);
    if (parser->current.type != TOKEN_ERROR)
      break;
    errorAtCurrent(parser, NULL);
  }
}

static void scanConsume(Parser *parser, TokenType type, const char *message) {
  if ((parser->current.type) == type) {
    scanAdvance(parser);
    return;
  }
  errorAtCurrent(parser, message);
}

// EMIT -----------------------------------------------------------------------

static void emitByte(Parser *parser, uint8_t byte) {
  chunkWrite(parser->chunk, byte, parser->previous.line);
}

static void emitBytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
  emitByte(parser, byte1);
  emitByte(parser, byte2);
}

static void emitConstant(Parser *parser, Value value) {
  int constant = chunkAddConst(parser->chunk, value);
  if (constant > UINT8_MAX) {
    errorAtPrev(parser, "Too many constants in one chunk.");
    return;
  }
  emitBytes(parser, OP_CONSTANT, (uint8_t)constant);
}

// PARSE ----------------------------------------------------------------------

static void parsePrecedence(Parser *parser, Precedence precedence) {
  scanAdvance(parser);
  ParseFn prefixRule = rules[parser->previous.type].prefix;
  if (prefixRule == NULL) {
    errorAtPrev(parser, "Expect expression.");
    return;
  }
  prefixRule(parser);

  while (precedence <= rules[parser->current.type].precedence) {
    scanAdvance(parser);
    ParseFn infixRule = rules[parser->previous.type].infix;
    infixRule(parser);
  }
}

static void parseNumber(Parser *parser) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, VAL_LIT_NUMBER(value));
}

static void parseString(Parser *parser) {
  const char *start = parser->previous.start + 1; // skip starting "
  int length = parser->previous.length - 2;       // skip starting and ending "
  Obj *obj = internObjStringStatic(parser->interned, start, length);
  emitConstant(parser, VAL_LIT_OBJ(obj));
}

static void parseLiteral(Parser *parser) {
  TokenType literalType = parser->previous.type;
  switch (literalType) {
  case TOKEN_FALSE:
    emitByte(parser, OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(parser, OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(parser, OP_TRUE);
    break;
  default:
    return; // unreachable
  }
}

static void parseExpression(Parser *parser) {
  parsePrecedence(parser, PREC_ASSIGNMENT);
}

static void parseGrouping(Parser *parser) {
  parseExpression(parser);
  scanConsume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void parseUnary(Parser *parser) {
  TokenType operatorType = parser->previous.type;

  parsePrecedence(parser, PREC_UNARY);

  switch (operatorType) {
  case TOKEN_MINUS:
    emitByte(parser, OP_NEGATE);
    break;
  case TOKEN_BANG:
    emitByte(parser, OP_NOT);
    break;
  default:
    return; // unreachable
  }
}

static void parseBinary(Parser *parser) {
  TokenType operatorType = parser->previous.type;
  parsePrecedence(parser, rules[operatorType].precedence + 1);

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitBytes(parser, OP_EQUAL, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(parser, OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(parser, OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(parser, OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(parser, OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(parser, OP_GREATER, OP_NOT);
    break;
  case TOKEN_MINUS:
    emitBytes(parser, OP_NEGATE, OP_ADD);
    // emitByte(parser, OP_SUBTRACT);
    break;
  case TOKEN_PLUS:
    emitByte(parser, OP_ADD);
    break;
  case TOKEN_SLASH:
    emitByte(parser, OP_DIVIDE);
    break;
  case TOKEN_STAR:
    emitByte(parser, OP_MULTIPLY);
    break;
  default:
    return; // unreachable
  }
}

// COMPILE --------------------------------------------------------------------

static void compileConclude(Parser *parser) {
  emitByte(parser, OP_RETURN);
#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    chunkDisassemble(parser->chunk, "code");
  }
#endif
}

bool compile(const char *source, Chunk *chunk, Table *interned) {
  Scanner scanner;
  scannerInit(&scanner, source);

  Parser parser;
  initParser(&parser, &scanner, chunk, interned);

  scanAdvance(&parser);
  parseExpression(&parser);
  scanConsume(&parser, TOKEN_EOF, "Expect end of expression.");
  compileConclude(&parser);

  return !parser.hadError;
}

// RULES ----------------------------------------------------------------------

static ParseRule rules[] = {
    {parseGrouping, NULL, PREC_NONE},     // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},              // TOKEN_RIGHT_PAREN
    {NULL, NULL, PREC_NONE},              // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},              // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},              // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},              // TOKEN_DOT
    {parseUnary, parseBinary, PREC_TERM}, // TOKEN_MINUS
    {NULL, parseBinary, PREC_TERM},       // TOKEN_PLUS
    {NULL, NULL, PREC_NONE},              // TOKEN_SEMICOLON
    {NULL, parseBinary, PREC_FACTOR},     // TOKEN_SLASH
    {NULL, parseBinary, PREC_FACTOR},     // TOKEN_STAR
    {parseUnary, NULL, PREC_NONE},        // TOKEN_BANG
    {NULL, parseBinary, PREC_EQUALITY},   // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},              // TOKEN_EQUAL
    {NULL, parseBinary, PREC_EQUALITY},   // TOKEN_EQUAL_EQUAL
    {NULL, parseBinary, PREC_COMPARISON}, // TOKEN_GREATER
    {NULL, parseBinary, PREC_COMPARISON}, // TOKEN_GREATER_EQUAL
    {NULL, parseBinary, PREC_COMPARISON}, // TOKEN_LESS
    {NULL, parseBinary, PREC_COMPARISON}, // TOKEN_LESS_EQUAL
    {NULL, NULL, PREC_NONE},              // TOKEN_IDENTIFIER
    {parseString, NULL, PREC_NONE},       // TOKEN_STRING
    {parseNumber, NULL, PREC_NONE},       // TOKEN_NUMBER
    {NULL, NULL, PREC_NONE},              // TOKEN_AND
    {NULL, NULL, PREC_NONE},              // TOKEN_CLASS
    {NULL, NULL, PREC_NONE},              // TOKEN_ELSE
    {parseLiteral, NULL, PREC_NONE},      // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},              // TOKEN_FOR
    {NULL, NULL, PREC_NONE},              // TOKEN_FUN
    {NULL, NULL, PREC_NONE},              // TOKEN_IF
    {parseLiteral, NULL, PREC_NONE},      // TOKEN_NIL
    {NULL, NULL, PREC_NONE},              // TOKEN_OR
    {NULL, NULL, PREC_NONE},              // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},              // TOKEN_RETURN
    {NULL, NULL, PREC_NONE},              // TOKEN_SUPER
    {NULL, NULL, PREC_NONE},              // TOKEN_THIS
    {parseLiteral, NULL, PREC_NONE},      // TOKEN_TRUE
    {NULL, NULL, PREC_NONE},              // TOKEN_VAR
    {NULL, NULL, PREC_NONE},              // TOKEN_WHILE
    {NULL, NULL, PREC_NONE},              // TOKEN_ERROR
    {NULL, NULL, PREC_NONE},              // TOKEN_EOF
};
