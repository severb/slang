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

typedef void (*CompileFn)(Compiler *, bool can_assign);

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

bool compile(const char *src, Chunk *chunk) { return true; }

typedef struct {
  CompileFn prefix;
  CompileFn infix;
  Precedence precedence;
} CompileRule;

static CompileRule rules[TOKEN_EOF + 1];

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
  error_at_token(c, &c->current, msg);
}

static void err_at_prev(Compiler *c, const char *msg) {
  error_at_token(c, &c->prev, msg);
}

static void advance(Compiler *c) {
  c->prev = c->current;
  for (;;) {
    lex_consume(&c->lex, &c->current);
    if (c->current.type != TOKEN_ERROR) {
      break;
    }
    error_at_current(c, 0);
  }
}

static void synchronize(Compiler *c) {
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
  error_at_current(c, msg);
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
