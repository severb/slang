#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "chunk.h"
#include "common.h"
#include "intern.h"
#include "mem.h"
#include "scanner.h"
#include "str.h"
#include "table.h"
#include "val.h"

typedef struct {
  Token current;
  Token prev;
  Scanner scanner;
  Intern *intern;
  Chunk *chunk;
  bool had_error;
  bool panic_mode;
  Array scopes;
  Table uninitialized;
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
static void parse_block(Parser *);
static void parse_stmt(Parser *);
static void parse_decl_var(Parser *);

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
  array_init(&p->scopes);
  table_init(&p->uninitialized);
  return p;
}

void parser_destroy(Parser *p) {
  if (p == 0)
    return;
  array_destroy(&p->scopes);
  table_destroy(&p->uninitialized);
  parser_init(p, "", 0, 0);
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
  if (!chunk_emit_const(p->chunk, op, val, token->line))
    error_at_token(p, token, "Too many constants in this scope.");
}

static void emit_byte(const Parser *p, uint8_t byte) {
  chunk_write(p->chunk, byte, p->prev.line);
}

static void emit_bytes(const Parser *p, uint8_t bytes[2]) {
  chunk_write2(p->chunk, bytes, p->prev.line);
}

static void local_declare(Parser *p, Val *var, Token *token) {
  Val scope = p->scopes.vals[p->scopes.len - 1];
  assert(VAL_IS_TABLE(scope));
  size_t idx = scope.val.as.table->len;
  if (idx >= UINT16_MAX) {
    error_at_token(p, token, "Too many local variables declared.");
    return;
  }
  Val copy = *var;
  Val *res = table_setdefault(scope.val.as.table, var, &VAL_LIT_NUMBER(idx));
  assert(VAL_IS_NUMBER(*res));
  if (idx != res->val.as.number)
    error_at_token(p, token, "Variable already declared.");
  table_set(&p->uninitialized, &copy, &VAL_LIT_NIL);
}

static void local_initialize(Parser *p, Val var) {
  table_del(&p->uninitialized, var);
}

static bool local_resolve(Parser *p, const Val *var, uint16_t *idx) {
  if (table_get(&p->uninitialized, *var)) {
    error_at_prev(p, "Cannot read local variable in its own initializer.");
    return false;
  }
  for (size_t i = 1; i <= p->scopes.len; i++) {
    Val scope = p->scopes.vals[p->scopes.len - i];
    assert(VAL_IS_TABLE(scope));
    Val *res = table_get(scope.val.as.table, *var);
    if (res != 0) {
      assert(VAL_IS_NUMBER(*res));
      size_t index = res->val.as.number;
      // sum all parent scope sizes
      for (size_t j = i + 1; j <= p->scopes.len; j++) {
        index += p->scopes.vals[p->scopes.len - j].val.as.table->len;
      }
      *idx = index;
      return true;
    }
  }
  return false;
}

static void scope_begin(Parser *p) {
  Table *t = ALLOCATE(Table);
  table_init(t);
  Val scope = VAL_LIT_TABLE(t);
  array_append(&p->scopes, &scope);
}

static void scope_end(Parser *p) {
  Val *v = array_pop(&p->scopes);
  assert(VAL_IS_TABLE(*v));
  for (size_t i = 0; i < v->val.as.table->len; i++)
    emit_byte(p, OP_POP);
  val_destroy(v);
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
    if (p->scopes.len > 0) {
      uint16_t idx;
      if (local_resolve(p, &var, &idx)) {
        chunk_emit_idx(p->chunk, OP_SET_LOCAL, idx, token.line);
        return;
      }
    }
    emit_const(p, OP_SET_GLOBAL, &var, &token);
  } else {
    if (p->scopes.len > 0) {
      uint16_t idx;
      if (local_resolve(p, &var, &idx)) {
        chunk_emit_idx(p->chunk, OP_GET_LOCAL, idx, token.line);
        return;
      }
    }
    emit_const(p, OP_GET_GLOBAL, &var, &token);
  }
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

static void parse_stmt_if(Parser *p) {
  scan_consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  parse_expr(p);
  scan_consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  chunk_write3(p->chunk, (uint8_t[]){OP_JUMP_IF_FALSE, 0, 0}, p->prev.line);
  size_t else_idx = p->chunk->len;
  emit_byte(p, OP_POP);
  parse_stmt(p);
  chunk_write3(p->chunk, (uint8_t[]){OP_JUMP, 0, 0}, p->prev.line);
  size_t end_idx = p->chunk->len;
  uint16_t location = p->chunk->len - else_idx;
  p->chunk->code[else_idx - 2] = (uint8_t)location >> 8;
  p->chunk->code[else_idx - 1] = (uint8_t)location;
  emit_byte(p, OP_POP);
  if (scan_match(p, TOKEN_ELSE)) {
    parse_stmt(p);
  }
  location = p->chunk->len - end_idx;
  p->chunk->code[end_idx - 2] = (uint8_t)location >> 8;
  p->chunk->code[end_idx - 1] = (uint8_t)location;
}

static void parse_stmt_while(Parser *p) {
  scan_consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  size_t start_idx = p->chunk->len;
  parse_expr(p);
  scan_consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  chunk_write3(p->chunk, (uint8_t[]){OP_JUMP_IF_FALSE, 0, 0}, p->prev.line);
  size_t break_idx = p->chunk->len;
  emit_byte(p, OP_POP);
  parse_stmt(p);
  uint16_t location = (p->chunk->len + 3) - start_idx;
  chunk_write3(p->chunk,
               (uint8_t[]){OP_LOOP, (uint8_t)location >> 8, (uint8_t)location},
               p->prev.line);
  location = p->chunk->len - break_idx;
  p->chunk->code[break_idx - 2] = (uint8_t)location >> 8;
  p->chunk->code[break_idx - 1] = (uint8_t)location;
  emit_byte(p, OP_POP);
}

static void parse_stmt_for(Parser *p) {
  scope_begin(p);
  scan_consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (scan_match(p, TOKEN_SEMICOLON)) {
    // pass
  } else if (scan_match(p, TOKEN_VAR)) {
    parse_decl_var(p);
  } else {
    parse_stmt_expr(p);
  }
  size_t start_idx = p->chunk->len;
  size_t break_idx = -1;
  if (!scan_match(p, TOKEN_SEMICOLON)) {
    parse_expr(p);
    scan_consume(p, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    chunk_write3(p->chunk, (uint8_t[]){OP_JUMP_IF_FALSE, 0, 0}, p->prev.line);
    break_idx = p->chunk->len;
    emit_byte(p, OP_POP);
  }
  if (!scan_match(p, TOKEN_RIGHT_PAREN)) {
    chunk_write3(p->chunk, (uint8_t[]){OP_JUMP, 0, 0}, p->prev.line);
    size_t before_inc_idx = p->chunk->len;
    parse_expr(p);
    emit_byte(p, OP_POP);
    scan_consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    size_t loc = (p->chunk->len + 3) - start_idx;
    chunk_write3(p->chunk,
                 (uint8_t[]){OP_LOOP, (uint8_t)loc >> 8, (uint8_t)loc},
                 p->prev.line);
    loc = p->chunk->len - before_inc_idx;
    p->chunk->code[before_inc_idx - 2] = (uint8_t)loc >> 8;
    p->chunk->code[before_inc_idx - 1] = (uint8_t)loc;
    start_idx = before_inc_idx;
  }
  parse_stmt(p);
  uint16_t location = (p->chunk->len + 3) - start_idx;
  chunk_write3(p->chunk,
               (uint8_t[]){OP_LOOP, (uint8_t)location >> 8, (uint8_t)location},
               p->prev.line);
  if (break_idx != (size_t)-1) {
    size_t loc = p->chunk->len - break_idx;
    p->chunk->code[break_idx - 2] = (uint8_t)loc >> 8;
    p->chunk->code[break_idx - 1] = (uint8_t)loc;
    emit_byte(p, OP_POP); // condition
  }
  scope_end(p);
}

static void parse_stmt(Parser *p) {
  if (scan_match(p, TOKEN_PRINT)) {
    parse_stmt_print(p);
  } else if (scan_match(p, TOKEN_IF)) {
    parse_stmt_if(p);
  } else if (scan_match(p, TOKEN_WHILE)) {
    parse_stmt_while(p);
  } else if (scan_match(p, TOKEN_FOR)) {
    parse_stmt_for(p);
  } else if (scan_match(p, TOKEN_LEFT_BRACE)) {
    scope_begin(p);
    parse_block(p);
    scope_end(p);
  } else
    parse_stmt_expr(p);
}

static void parse_decl_var(Parser *p) {
  scan_consume(p, TOKEN_IDENTIFIER, "Expect variable name.");
  Token token = p->prev;
  Slice slice;
  slice_init(&slice, p->prev.start, p->prev.len);
  Val var = intern_slice(p->intern, slice);
  Val var_copy = var;
  if (p->scopes.len > 0)
    local_declare(p, &var, &token);
  if (scan_match(p, TOKEN_EQUAL))
    parse_expr(p);
  else
    emit_byte(p, OP_NIL);
  if (p->scopes.len == 0)
    emit_const(p, OP_DEF_GLOBAL, &var, &token);
  else
    local_initialize(p, var_copy);
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

static void parse_block(Parser *p) {
  while (p->current.type != TOKEN_RIGHT_BRACE && p->current.type != TOKEN_EOF)
    parse_decl(p);
  scan_consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void parse_and(Parser *p, bool _) {
  chunk_write3(p->chunk, (uint8_t[]){OP_JUMP_IF_FALSE, 0, 0}, p->prev.line);
  size_t idx = p->chunk->len;
  emit_byte(p, OP_POP);
  parse_precedence(p, PREC_AND);
  uint16_t location = p->chunk->len - idx;
  p->chunk->code[idx - 2] = (uint8_t)location >> 8;
  p->chunk->code[idx - 1] = (uint8_t)location;
}

static void parse_or(Parser *p, bool _) {
  chunk_write3(p->chunk, (uint8_t[]){OP_JUMP_IF_TRUE, 0, 0}, p->prev.line);
  size_t idx = p->chunk->len;
  emit_byte(p, OP_POP);
  parse_precedence(p, PREC_AND);
  uint16_t location = p->chunk->len - idx;
  p->chunk->code[idx - 2] = (uint8_t)location >> 8;
  p->chunk->code[idx - 1] = (uint8_t)location;
}

bool compile(const char *src, Chunk *chunk, Intern *intern) {
  Parser parser;
  parser_init(&parser, src, chunk, intern);
  scan_advance(&parser); // prime the parser
  while (!scan_match(&parser, TOKEN_EOF))
    parse_decl(&parser);
  emit_byte(&parser, OP_RETURN);
  bool res = !parser.had_error;
  parser_destroy(&parser);
  return res;
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
    {NULL, parse_and, PREC_AND},            // TOKEN_AND
    {NULL, NULL, PREC_NONE},                // TOKEN_CLASS
    {NULL, NULL, PREC_NONE},                // TOKEN_ELSE
    {parse_literal, NULL, PREC_NONE},       // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},                // TOKEN_FOR
    {NULL, NULL, PREC_NONE},                // TOKEN_FUN
    {NULL, NULL, PREC_NONE},                // TOKEN_IF
    {parse_literal, NULL, PREC_NONE},       // TOKEN_NIL
    {NULL, parse_or, PREC_OR},              // TOKEN_OR
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