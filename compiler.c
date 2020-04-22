#include "compiler.h"

#include "lex.h"
#include "mem.h"
#include "types.h"
#include "val.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  Token current;
  Token prev;
  Lexer lex;
  Chunk *chunk;
  bool had_error;
  bool panic_mode;
  List scopes;
  List uninitialized;
} Compiler;

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

typedef void (*CompileFn)(Compiler *, bool can_assign);

typedef struct {
  CompileFn prefix;
  CompileFn infix;
  Precedence precedence;
} CompileRule;

static CompileRule rules[TOKEN__COUNT];

static void compiler_free(Compiler *c) {
  list_free(&c->scopes);
  list_free(&c->uninitialized);
  *c = (Compiler){0};
}

static void error_print(const Token *t, const char *msg) {
  fprintf(stderr, "[line %zu] Error ", t->line);
  switch (t->type) {
  case TOKEN_EOF:
    fprintf(stderr, "at end of file: ");
    break;
  case TOKEN_ERROR:
    fprintf(stderr, ": ");
    break;
  default:
    // TODO: avoid int cast
    fprintf(stderr, "at '%.*s': ", (int)(t->stop - t->start), t->start);
    break;
  }
  fprintf(stderr, "%s\n", (t->type == TOKEN_ERROR) ? t->start : msg);
}

static void err_at_token(Compiler *c, const Token *t, const char *msg) {
  if (c->panic_mode) {
    return;
  }
  c->panic_mode = true;
  c->had_error = true;
  error_print(t, msg);
}

static void err_at_current(Compiler *c, const char *msg) {
  err_at_token(c, &c->current, msg);
}

static void err_at_prev(Compiler *c, const char *msg) {
  err_at_token(c, &c->prev, msg);
}

static void advance(Compiler *c) {
  c->prev = c->current;
  for (;;) {
    lex_consume(&c->lex, &c->current);
    if (c->current.type != TOKEN_ERROR) {
      break;
    }
    err_at_current(c, 0);
  }
}

static void synchronize(Compiler *c) {
  if (!c->panic_mode) {
    return;
  }
  c->panic_mode = false;
  while (c->current.type != TOKEN_EOF) {
    if (c->prev.type == TOKEN_SEMICOLON) {
      return;
    }
    switch (c->current.type) {
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
    advance(c);
  }
}

static void consume(Compiler *c, TokenType type, const char *msg) {
  if (c->current.type == type) {
    advance(c);
    return;
  }
  err_at_current(c, msg);
}

static bool match(Compiler *c, TokenType type) {
  if (c->current.type == type) {
    advance(c);
    return true;
  }
  return false;
}

static void compile_int(Compiler *c, bool _) {
  long long i = strtoll(c->prev.start, /*str_end*/ 0, /*base*/ 0);
  if (errno == ERANGE) {
    err_at_prev(c, "integer constant out of range");
    return;
  }
  assert(i >= 0); // only positive values integers
  Val v;
  if (i <= INT32_MAX) {
    v = val_data4pair(0, i);
    goto emit;
  }
  if (i <= INT64_MAX) {
    int64_t *ip = mem_allocate(sizeof(uint64_t));
    *ip = i;
    v = val_ptr4int64(ip);
    goto emit;
  }
  err_at_prev(c, "integer constant out of range");
  return;
emit : {
  size_t idx = chunk_record_const(c->chunk, v);
  chunk_write_operand(c->chunk, idx, c->prev.line);
}
}

static void compile_float(Compiler *c, bool _) {
  double d = strtod(c->prev.start, 0);
  if (errno == ERANGE) {
    err_at_prev(c, "float constant out of range");
    return;
  }
  assert(d >= 0); // only positive values
  size_t idx = chunk_record_const(c->chunk, val_data4double(d));
  chunk_write_operand(c->chunk, idx, c->prev.line);
}

static void compile_string(Compiler *c, bool _) {
  Slice *s = mem_allocate(sizeof(Slice));
  *s = slice(c->prev.start + 1 /*skip 1st quote */,
             c->prev.stop - c->prev.start - 2 /* skip both quotes */);
  size_t idx = chunk_record_const(c->chunk, val_ptr4slice(s));
  chunk_write_operand(c->chunk, idx, c->prev.line);
}

static void compile_literal(Compiler *c, bool _) {
  TokenType lit = c->prev.type;
  switch (lit) {
  case TOKEN_FALSE:
    chunk_write_operation(c->chunk, c->prev.line, OP_FALSE);
    break;
  case TOKEN_NIL:
    chunk_write_operation(c->chunk, c->prev.line, OP_NIL);
    break;
  case TOKEN_TRUE:
    chunk_write_operation(c->chunk, c->prev.line, OP_TRUE);
  default:
    assert(0);
  }
}

static void enter_scope(Compiler *c) {}
static void exit_scope(Compiler *c) {}

static void compile_print_statement(Compiler *c) {}
static void compile_if_statement(Compiler *c) {}
static void compile_while_statement(Compiler *c) {}
static void compile_for_statement(Compiler *c) {}
static void compile_expression_statement(Compiler *c) {}
static void compile_block(Compiler *c) {}
static void compile_var_declaration(Compiler *c) {}

static void compile_statement(Compiler *c) {
  if (match(c, TOKEN_PRINT)) {
    compile_print_statement(c);
  } else if (match(c, TOKEN_IF)) {
    compile_if_statement(c);
  } else if (match(c, TOKEN_WHILE)) {
    compile_while_statement(c);
  } else if (match(c, TOKEN_FOR)) {
    compile_for_statement(c);
  } else if (match(c, TOKEN_LEFT_BRACE)) {
    enter_scope(c);
    compile_block(c);
    exit_scope(c);
  } else {
    compile_expression_statement(c);
  }
}

static void compile_declaration(Compiler *c) {
  if (match(c, TOKEN_VAR)) {
    compile_var_declaration(c);
  } else {
    compile_statement(c);
  }
  synchronize(c);
}

bool compile(const char *src, Chunk *chunk) { return true; }

static CompileRule rules[] = {
    {NULL, NULL, PREC_NONE},            // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},            // TOKEN_RIGHT_PAREN
    {NULL, NULL, PREC_NONE},            // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},            // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},            // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},            // TOKEN_DOT
    {NULL, NULL, PREC_TERM},            // TOKEN_MINUS
    {NULL, NULL, PREC_TERM},            // TOKEN_PLUS
    {NULL, NULL, PREC_NONE},            // TOKEN_SEMICOLON
    {NULL, NULL, PREC_FACTOR},          // TOKEN_SLASH
    {NULL, NULL, PREC_FACTOR},          // TOKEN_STAR
    {NULL, NULL, PREC_NONE},            // TOKEN_BANG
    {NULL, NULL, PREC_EQUALITY},        // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},            // TOKEN_EQUAL
    {NULL, NULL, PREC_EQUALITY},        // TOKEN_EQUAL_EQUAL
    {NULL, NULL, PREC_COMPARISON},      // TOKEN_GREATER
    {NULL, NULL, PREC_COMPARISON},      // TOKEN_GREATER_EQUAL
    {NULL, NULL, PREC_COMPARISON},      // TOKEN_LESS
    {NULL, NULL, PREC_COMPARISON},      // TOKEN_LESS_EQUAL
    {NULL, NULL, PREC_NONE},            // TOKEN_IDENTIFIER
    {compile_string, NULL, PREC_NONE},  // TOKEN_STRING
    {compile_int, NULL, PREC_NONE},     // TOKEN_INT
    {compile_float, NULL, PREC_NONE},   // TOKEN_FLOAT
    {NULL, NULL, PREC_AND},             // TOKEN_AND
    {NULL, NULL, PREC_NONE},            // TOKEN_CLASS
    {NULL, NULL, PREC_NONE},            // TOKEN_ELSE
    {compile_literal, NULL, PREC_NONE}, // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},            // TOKEN_FOR
    {NULL, NULL, PREC_NONE},            // TOKEN_FUN
    {NULL, NULL, PREC_NONE},            // TOKEN_IF
    {compile_literal, NULL, PREC_NONE}, // TOKEN_NIL
    {NULL, NULL, PREC_OR},              // TOKEN_OR
    {NULL, NULL, PREC_NONE},            // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},            // TOKEN_RETURN
    {NULL, NULL, PREC_NONE},            // TOKEN_SUPER
    {NULL, NULL, PREC_NONE},            // TOKEN_THIS
    {compile_literal, NULL, PREC_NONE}, // TOKEN_TRUE
    {NULL, NULL, PREC_NONE},            // TOKEN_VAR
    {NULL, NULL, PREC_NONE},            // TOKEN_WHILE
    {NULL, NULL, PREC_NONE},            // TOKEN_ERROR
    {NULL, NULL, PREC_NONE},            // TOKEN_EOF
};
