#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "intern.h"
#include "scanner.h"
#include "str.h"
#include "val.h"

typedef struct {
  Token current;
  Token prev;
  Scanner scanner;
  Intern *intern;
  Chunk *chunk;
  bool had_error;
  bool panic_mode;
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

typedef void (*ParseFn)(Parser *, bool);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static ParseRule rules[TOKEN_EOF + 1];

static void scan_advance(Parser *);
static void parse_expr(Parser *);

Parser *parser_init(Parser *p, const char *src, Chunk *chunk, Intern *intern) {
  if (p == 0)
    return 0;
  const char *uninitialized_msg = "Uninitialized token.";
  Token uninitialized = {
      .type = TOKEN_ERROR,
      .start = uninitialized_msg,
      .len = strlen(uninitialized_msg),
  };
  *p = (Parser){
      .current = uninitialized,
      .prev = uninitialized,
      .scanner = {0},
      .intern = intern,
      .chunk = chunk,
      .had_error = false,
      .panic_mode = false,
  };
  scanner_init(&p->scanner, src);
  return p;
}

static void error_print(const Token *token, const char *msg) {
  fprintf(stderr, "[line %zu] Error", token->line);
  switch (token->type) {
  case TOKEN_EOF:
    fprintf(stderr, " at end");
  case TOKEN_ERROR:
    break;
  default:
    fprintf(stderr, " at '%.*s'", (int)token->len, token->start);
    break;
  }
  fprintf(stderr, ": %s\n", (token->type == TOKEN_ERROR) ? token->start : msg);
}

static void error_syncronize(Parser *p) {
  p->panic_mode = false;
  while (p->current.type != TOKEN_EOF) {
    if (p->prev.type == TOKEN_SEMICOLON)
      return;
    switch (p->current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:;
    }
    scan_advance(p);
  }
}

static void error_at_token(Parser *p, const Token *token, const char *msg) {
  if (p->panic_mode)
    return;
  p->panic_mode = true;
  p->had_error = true;
  error_print(token, msg);
}

static void error_at_current(Parser *p, const char *msg) {
  error_at_token(p, &p->current, msg);
}

static void error_at_prev(Parser *p, const char *msg) {
  error_at_token(p, &p->prev, msg);
}

static void scan_advance(Parser *p) {
  p->prev = p->current;
  for (;;) {
    p->current = scanner_consume(&p->scanner);
    if (p->current.type != TOKEN_ERROR)
      break;
    error_at_current(p, NULL);
  }
}

static void scan_consume(Parser *p, TokenType type, const char *msg) {
  if ((p->current.type) == type) {
    scan_advance(p);
    return;
  }
  error_at_current(p, msg);
}

static bool scan_match(Parser *p, TokenType type) {
  bool res = p->current.type == type;
  if (res)
    scan_advance(p);
  return res;
}

static void emit_const(Parser *p, OpCode op, Val *val, const Token *token) {
  uint16_t pos = chunk_emit_const(p->chunk, op, val, token->line);
  if (pos == UINT16_MAX)
    error_at_token(p, token, "Too many constants in this scope.");
}

static void emit_byte(const Parser *p, uint8_t byte) {
  chunk_write(p->chunk, byte, p->prev.line);
}

static void emit_bytes(const Parser *p, uint8_t bytes[2]) {
  chunk_write2(p->chunk, bytes, p->prev.line);
}

static void parse_precedence(Parser *p, Precedence prec) {
  scan_advance(p);
  ParseFn prefix_rule = rules[p->prev.type].prefix;
  if (prefix_rule == NULL) {
    error_at_prev(p, "Expect expression.");
    return;
  }
  bool can_assign = prec <= PREC_ASSIGNMENT;
  prefix_rule(p, can_assign);
  while (prec <= rules[p->current.type].precedence) {
    scan_advance(p);
    ParseFn infix_rule = rules[p->prev.type].infix;
    infix_rule(p, can_assign);
  }
  if (can_assign && scan_match(p, TOKEN_EQUAL))
    error_at_current(p, "Invalid target assignment.");
}

static void parse_number(Parser *p, bool _) {
  double value = strtod(p->prev.start, NULL);
  emit_const(p, OP_CONSTANT, &VAL_LIT_NUMBER(value), &p->prev);
}

static void parse_string(Parser *p, bool _) {
  const char *start = p->prev.start + 1; // skip starting quote
  int len = p->prev.len - 2;             // skip starting and ending quotes
  Slice slice;
  slice_init(&slice, start, len);
  Val str = intern_slice(p->intern, slice);
  emit_const(p, OP_CONSTANT, &str, &p->prev);
}

static void parse_literal(Parser *p, bool _) {
  TokenType literal_type = p->prev.type;
  switch (literal_type) {
  case TOKEN_FALSE:
    emit_byte(p, OP_FALSE);
    break;
  case TOKEN_NIL:
    emit_byte(p, OP_NIL);
    break;
  case TOKEN_TRUE:
    emit_byte(p, OP_TRUE);
    break;
  default:
    assert(0);
  }
}

static void parse_expr(Parser *p) { parse_precedence(p, PREC_ASSIGNMENT); }

static void parse_variable(Parser *p, bool can_assign) {
  Token token = p->prev;
  Slice slice;
  slice_init(&slice, p->prev.start, p->prev.len);
  Val var = intern_slice(p->intern, slice);
  if (can_assign && scan_match(p, TOKEN_EQUAL)) {
    parse_expr(p);
    emit_const(p, OP_SET_GLOBAL, &var, &token);
  } else
    emit_const(p, OP_GET_GLOBAL, &var, &token);
}

static void parse_grouping(Parser *p, bool _) {
  parse_expr(p);
  scan_consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void parse_unary(Parser *p, bool _) {
  TokenType operator_type = p->prev.type;
  parse_precedence(p, PREC_UNARY);
  switch (operator_type) {
  case TOKEN_MINUS:
    emit_byte(p, OP_NEGATE);
    break;
  case TOKEN_BANG:
    emit_byte(p, OP_NOT);
    break;
  default:
    assert(0);
  }
}

static void parse_binary(Parser *p, bool _) {
  TokenType operator_type = p->prev.type;
  parse_precedence(p, rules[operator_type].precedence + 1);
  switch (operator_type) {
  case TOKEN_BANG_EQUAL:
    emit_bytes(p, (uint8_t[]){OP_EQUAL, OP_NOT});
    break;
  case TOKEN_EQUAL_EQUAL:
    emit_byte(p, OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emit_byte(p, OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emit_bytes(p, (uint8_t[]){OP_LESS, OP_NOT});
    break;
  case TOKEN_LESS:
    emit_byte(p, OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emit_bytes(p, (uint8_t[]){OP_GREATER, OP_NOT});
    break;
  case TOKEN_MINUS:
    // TODO: optimize to OP_SUBTRACT in another pass
    emit_bytes(p, (uint8_t[]){OP_NEGATE, OP_ADD});
    break;
  case TOKEN_PLUS:
    emit_byte(p, OP_ADD);
    break;
  case TOKEN_SLASH:
    emit_byte(p, OP_DIVIDE);
    break;
  case TOKEN_STAR:
    emit_byte(p, OP_MULTIPLY);
    break;
  default:
    assert(0);
  }
}

static void parse_stmt_expr(Parser *p) {
  parse_expr(p);
  scan_consume(p, TOKEN_SEMICOLON,
               "Expect semicolon after expression statement.");
  emit_byte(p, OP_POP);
}

static void parse_stmt_print(Parser *p) {
  parse_expr(p);
  scan_consume(p, TOKEN_SEMICOLON, "Expect semicolon after print statement.");
  emit_byte(p, OP_PRINT);
}

static void parse_stmt(Parser *p) {
  if (scan_match(p, TOKEN_PRINT))
    parse_stmt_print(p);
  else
    parse_stmt_expr(p);
}

static void parse_decl_var(Parser *p) {
  scan_consume(p, TOKEN_IDENTIFIER, "Expect variable name.");
  Token token = p->prev;
  Slice slice;
  slice_init(&slice, p->prev.start, p->prev.len);
  Val var = intern_slice(p->intern, slice);
  if (scan_match(p, TOKEN_EQUAL))
    parse_expr(p);
  else
    emit_byte(p, OP_NIL);
  emit_const(p, OP_DEF_GLOBAL, &var, &token);
  scan_consume(p, TOKEN_SEMICOLON,
               "Expect semicolon after variable declaration.");
}

static void parse_decl(Parser *p) {
  if (scan_match(p, TOKEN_VAR))
    parse_decl_var(p);
  else
    parse_stmt(p);
  if (p->panic_mode)
    error_syncronize(p);
}

bool compile(const char *src, Chunk *chunk, Intern *intern) {
  Parser parser;
  parser_init(&parser, src, chunk, intern);
  scan_advance(&parser); // prime the parser
  while (!scan_match(&parser, TOKEN_EOF))
    parse_decl(&parser);
  emit_byte(&parser, OP_RETURN);
  return !parser.had_error;
}

static ParseRule rules[] = {
    {parse_grouping, NULL, PREC_NONE},      // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},                // TOKEN_RIGHT_PAREN
    {NULL, NULL, PREC_NONE},                // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},                // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},                // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},                // TOKEN_DOT
    {parse_unary, parse_binary, PREC_TERM}, // TOKEN_MINUS
    {NULL, parse_binary, PREC_TERM},        // TOKEN_PLUS
    {NULL, NULL, PREC_NONE},                // TOKEN_SEMICOLON
    {NULL, parse_binary, PREC_FACTOR},      // TOKEN_SLASH
    {NULL, parse_binary, PREC_FACTOR},      // TOKEN_STAR
    {parse_unary, NULL, PREC_NONE},         // TOKEN_BANG
    {NULL, parse_binary, PREC_EQUALITY},    // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},                // TOKEN_EQUAL
    {NULL, parse_binary, PREC_EQUALITY},    // TOKEN_EQUAL_EQUAL
    {NULL, parse_binary, PREC_COMPARISON},  // TOKEN_GREATER
    {NULL, parse_binary, PREC_COMPARISON},  // TOKEN_GREATER_EQUAL
    {NULL, parse_binary, PREC_COMPARISON},  // TOKEN_LESS
    {NULL, parse_binary, PREC_COMPARISON},  // TOKEN_LESS_EQUAL
    {parse_variable, NULL, PREC_NONE},      // TOKEN_IDENTIFIER
    {parse_string, NULL, PREC_NONE},        // TOKEN_STRING
    {parse_number, NULL, PREC_NONE},        // TOKEN_NUMBER
    {NULL, NULL, PREC_NONE},                // TOKEN_AND
    {NULL, NULL, PREC_NONE},                // TOKEN_CLASS
    {NULL, NULL, PREC_NONE},                // TOKEN_ELSE
    {parse_literal, NULL, PREC_NONE},       // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},                // TOKEN_FOR
    {NULL, NULL, PREC_NONE},                // TOKEN_FUN
    {NULL, NULL, PREC_NONE},                // TOKEN_IF
    {parse_literal, NULL, PREC_NONE},       // TOKEN_NIL
    {NULL, NULL, PREC_NONE},                // TOKEN_OR
    {NULL, NULL, PREC_NONE},                // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},                // TOKEN_RETURN
    {NULL, NULL, PREC_NONE},                // TOKEN_SUPER
    {NULL, NULL, PREC_NONE},                // TOKEN_THIS
    {parse_literal, NULL, PREC_NONE},       // TOKEN_TRUE
    {NULL, NULL, PREC_NONE},                // TOKEN_VAR
    {NULL, NULL, PREC_NONE},                // TOKEN_WHILE
    {NULL, NULL, PREC_NONE},                // TOKEN_ERROR
    {NULL, NULL, PREC_NONE},                // TOKEN_EOF
};
