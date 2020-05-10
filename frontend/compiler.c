#include "compiler.h"

#include "frontend/lex.h" // Lexer, lex_consume
#include "mem.h"          // mem_allocate
#include "types/list.h"   // List, list_*
#include "types/str.h"    // Slice, slice
#include "types/tag.h"    // Tag, *_to_tag, tag_to_*

#include <errno.h>   // errno
#include <stdbool.h> // bool, true, false
#include <stdint.h>  // uint64_t
#include <stdio.h>   // fprintf, stderr
#include <stdlib.h>  // size_t

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

static void compile_statement(Compiler *c);
static void compile_declaration(Compiler *c);

static void compiler_free(Compiler *c) {
  list_free(&c->scopes);
  list_free(&c->uninitialized);
  *c = (Compiler){0};
}

static void error_print(const Token *t, const char *msg) {
  fprintf(stderr, "[line %zu] Error", t->line);
  switch (t->type) {
  case TOKEN_EOF:
    fprintf(stderr, " at end of file: ");
    break;
  case TOKEN_ERROR:
    fprintf(stderr, ": ");
    break;
  default:
    // TODO: avoid int cast
    fprintf(stderr, " at '%.*s': ", (int)(t->end - t->start), t->start);
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
  Tag t;
  if (i <= INT32_MAX) {
    t = upair_to_tag(0, i);
    goto emit;
  }
  if (i <= INT64_MAX) {
    // TODO: use a memory pool
    int64_t *ip = mem_allocate(sizeof(uint64_t));
    *ip = i;
    t = i64_to_tag(ip);
    goto emit;
  }
  err_at_prev(c, "integer constant out of range");
  return;
emit : {
  size_t idx = chunk_record_const(c->chunk, t);
  chunk_write_unary(c->chunk, c->prev.line, OP_CONSTANT, idx);
}
}

static void compile_float(Compiler *c, bool _) {
  double d = strtod(c->prev.start, 0);
  if (errno == ERANGE) {
    err_at_prev(c, "float constant out of range");
    return;
  }
  assert(d >= 0); // only positive values
  size_t idx = chunk_record_const(c->chunk, double_to_tag(d));
  chunk_write_unary(c->chunk, c->prev.line, OP_CONSTANT, idx);
}

static void compile_string(Compiler *c, bool _) {
  // TODO: use a memory pool
  Slice *s = mem_allocate(sizeof(Slice));
  *s = slice(c->prev.start + 1 /*skip 1st quote */,
             c->prev.end - 1 /* skip last quotes */);
  size_t idx = chunk_record_const(c->chunk, slice_to_tag(s));
  chunk_write_unary(c->chunk, c->prev.line, OP_CONSTANT, idx);
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
    break;
  default:
    assert(0);
  }
}

static void enter_scope(Compiler *c) {
  // TODO: use a memory pool
  List *scopes = mem_allocate(sizeof(List));
  *scopes = (List){0};
  list_append(&c->scopes, list_to_tag(scopes));
  List *uninitialized = mem_allocate(sizeof(List));
  *uninitialized = (List){0};
  list_append(&c->uninitialized, list_to_tag(uninitialized));
}

static bool in_scope(const Compiler *c) { return list_len(&c->scopes) > 0; }

static void exit_scope(Compiler *c) {
  assert(in_scope(c) && "not in a scope");
  tag_free(list_pop(&c->scopes));
  tag_free(list_pop(&c->uninitialized));
}

static List *top_scope_list(Compiler *c) {
  assert(in_scope(c) && "not in a scope");
  Tag scope = *list_last(&c->scopes);
  return tag_to_list(scope);
}

static List *top_uninitialized_list(Compiler *c) {
  assert(in_scope(c) && "not in a scope");
  Tag uninitialized = *list_last(&c->uninitialized);
  return tag_to_list(uninitialized);
}

static void declare_local(Compiler *c, Tag var) {
  List *top_scope = top_scope_list(c);
  if (list_find(top_scope, var, 0)) {
    err_at_prev(c, "variable already defined");
    return;
  }
  List *top_uninitialized = top_uninitialized_list(c);
  list_append(top_scope, var);
  list_append(top_uninitialized, tag_to_ref(var)); // just a ref
}

static void initialize_local(Compiler *c, Tag var) {
  List *top_uninitialized = top_uninitialized_list(c);
  size_t idx;
  bool found = list_find(top_uninitialized, var, &idx);
  assert(found && "cannot initialize an undefined variable");
  // replace the initialized var with the last uninitialized
  Tag last = *list_last(top_uninitialized);
  if (idx < list_len(top_uninitialized)) {
    *list_get(top_uninitialized, idx) = last;
  } else {
    tag_free(last);
  }
}

static bool resolve_local(Compiler *c, Tag var, size_t *idx) {
  List *top_uninitialized = top_uninitialized_list(c);
  if (list_find(top_uninitialized, var, 0)) {
    err_at_prev(c, "local variable used in its own initializer");
    return false;
  }
  // traverse the scopes in reverse order and look for var
  // if found, sum its index with the size of all remaining bottom scopes
  for (size_t i = list_len(&c->scopes); i > 0; i--) {
    size_t ii = i - 1;
    Tag scope_val = *list_get(&c->scopes, ii);
    List *scope_list = tag_to_list(scope_val);
    if (list_find(scope_list, var, idx)) {
      for (size_t j = ii; j > 0; j--) {
        size_t jj = j - 1;
        Tag parent_scope_val = *list_get(&c->scopes, jj);
        List *parent_scope_list = tag_to_list(parent_scope_val);
        *idx += list_len(parent_scope_list);
      }
      return true;
    }
  }
  return false;
}

static void compile_precedence(Compiler *c, Precedence p) {
  advance(c);
  CompileFn prefix_rule = rules[c->prev.type].prefix;
  if (prefix_rule == 0) {
    err_at_prev(c, "invalid expression");
    return;
  }
  bool can_assign = p <= PREC_ASSIGNMENT;
  prefix_rule(c, can_assign);
  while (p <= rules[c->current.type].precedence) {
    advance(c);
    CompileFn infix_rule = rules[c->prev.type].infix;
    assert(infix_rule);
    infix_rule(c, can_assign);
  }
  // assignment is handled in rules, if we see an assignment here it's an err
  if (can_assign && match(c, TOKEN_EQUAL)) {
    err_at_current(c, "invalid target assignment");
    return;
  }
}

static void compile_expression(Compiler *c) {
  compile_precedence(c, PREC_ASSIGNMENT);
}

static void compile_print_statement(Compiler *c) {
start:
  compile_expression(c);
  chunk_write_operation(c->chunk, c->prev.line, OP_PRINT);
  if (match(c, TOKEN_COMMA)) {
    goto start;
  }
  consume(c, TOKEN_SEMICOLON, "semicolon missing after print");
}

static void compile_if_statement(Compiler *c) {
  consume(c, TOKEN_LEFT_PAREN, "missing paren before if condition");
  compile_expression(c);
  consume(c, TOKEN_RIGHT_PAREN, "missing paren after if condition");
  size_t jump_if_false = chunk_reserve_unary(c->chunk, c->prev.line);
  size_t then_label = chunk_len(c->chunk);
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
  compile_statement(c);
  size_t jump_after_else = chunk_reserve_unary(c->chunk, c->prev.line);
  size_t else_label = chunk_len(c->chunk);
  assert(chunk_len(c->chunk) >= then_label);
  chunk_patch_unary(c->chunk, jump_if_false, OP_JUMP_IF_FALSE,
                    chunk_len(c->chunk) - then_label);
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
  if (match(c, TOKEN_ELSE)) {
    compile_statement(c);
  }
  assert(chunk_len(c->chunk) >= else_label);
  chunk_patch_unary(c->chunk, jump_after_else, OP_JUMP,
                    chunk_len(c->chunk) - else_label);
}

static void compile_while_statement(Compiler *c) {
  consume(c, TOKEN_LEFT_BRACE, "missing paren before while condition");
  size_t start_label = chunk_len(c->chunk);
  compile_expression(c);
  consume(c, TOKEN_RIGHT_BRACE, "missing paren after while condition");
  size_t jump_if_false = chunk_reserve_unary(c->chunk, c->prev.line);
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
  compile_statement(c);
  assert(chunk_len(c->chunk) >= start_label);
  chunk_patch_unary(c->chunk, jump_if_false, OP_LOOP,
                    chunk_len(c->chunk) - start_label);
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
}

static void compile_for_statement(Compiler *c) {}

static void compile_expression_statement(Compiler *c) {
  compile_expression(c);
  consume(c, TOKEN_SEMICOLON, "semicolon missing after expression statement");
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
}

static void compile_block(Compiler *c) {
  while (!match(c, TOKEN_RIGHT_BRACE)) {
    compile_declaration(c);
    if (match(c, TOKEN_EOF)) {
      err_at_current(c, "closing brace missing after block");
      return;
    }
  }
}

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

Tag sliceval_from_token(Token t) {
  // TODO: use a memory pool
  Slice *s = mem_allocate(sizeof(Slice));
  *s = slice(t.start, t.end);
  return slice_to_tag(s);
}

static void compile_var_declaration(Compiler *c) {
start:
  consume(c, TOKEN_IDENTIFIER, "variable name is missing");
  Tag var = sliceval_from_token(c->prev);
  if (in_scope(c)) {
    declare_local(c, var);
  }
  if (match(c, TOKEN_EQUAL)) {
    compile_expression(c);
  } else {
    chunk_write_operation(c->chunk, c->prev.line, OP_NIL);
  }
  if (in_scope(c)) {
    initialize_local(c, var);
    size_t idx;
    bool found = resolve_local(c, var, &idx);
    assert(found);
    chunk_write_unary(c->chunk, c->prev.line, OP_SET_LOCAL, idx);
  } else {
    size_t idx = chunk_record_const(c->chunk, var);
    chunk_write_unary(c->chunk, c->prev.line, OP_DEF_GLOBAL, idx);
  }
  if (match(c, TOKEN_COMMA)) {
    goto start;
  }
  consume(c, TOKEN_SEMICOLON, "semicolon missing after variable declaration");
}

static void compile_declaration(Compiler *c) {
  if (match(c, TOKEN_VAR)) {
    compile_var_declaration(c);
  } else {
    compile_statement(c);
  }
  synchronize(c);
}

static void compile_unary(Compiler *c, bool _) {
  Token t = c->prev;
  compile_precedence(c, PREC_UNARY);
  switch (t.type) {
  case TOKEN_MINUS:
    chunk_write_operation(c->chunk, t.line, OP_NEGATE);
    break;
  case TOKEN_BANG:
    chunk_write_operation(c->chunk, t.line, OP_NOT);
    break;
  default:
    assert(0);
  }
}

static void compile_binary(Compiler *c, bool _) {
  Token t = c->prev;
  compile_precedence(c, rules[t.type].precedence + 1);
  switch (t.type) {
  case TOKEN_BANG_EQUAL:
    chunk_write_operation(c->chunk, t.line, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    chunk_write_operation(c->chunk, t.line, OP_EQUAL);
    break;
  case TOKEN_GREATER:
    chunk_write_operation(c->chunk, t.line, OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    chunk_write_operation(c->chunk, t.line, OP_LESS);
    chunk_write_operation(c->chunk, t.line, OP_NOT);
    break;
  case TOKEN_LESS:
    chunk_write_operation(c->chunk, t.line, OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    chunk_write_operation(c->chunk, t.line, OP_GREATER);
    chunk_write_operation(c->chunk, t.line, OP_NOT);
    break;
  case TOKEN_MINUS:
    chunk_write_operation(c->chunk, t.line, OP_NEGATE);
    chunk_write_operation(c->chunk, t.line, OP_ADD);
    break;
  case TOKEN_PLUS:
    chunk_write_operation(c->chunk, t.line, OP_ADD);
    break;
  case TOKEN_SLASH:
    chunk_write_operation(c->chunk, t.line, OP_DIVIDE);
    break;
  case TOKEN_STAR:
    chunk_write_operation(c->chunk, t.line, OP_MULTIPLY);
    break;
  default:
    assert(0);
  }
}

static void compile_variable(Compiler *c, bool can_assign) {
  Tag var = sliceval_from_token(c->prev);
  if (can_assign && match(c, TOKEN_EQUAL)) { // an assignment
    compile_expression(c);
    if (in_scope(c)) { // in local scope
      size_t idx;
      if (resolve_local(c, var, &idx)) {
        chunk_write_unary(c->chunk, c->prev.line, OP_SET_LOCAL, idx);
        return;
      }
    }
    // in global scope or no local variable found in local and parent scopes
    size_t idx = chunk_record_const(c->chunk, var);
    chunk_write_unary(c->chunk, c->prev.line, OP_SET_GLOBAL, idx);
  } else {             // not an assignment
    if (in_scope(c)) { // in local scope
      size_t idx;
      if (resolve_local(c, var, &idx)) {
        chunk_write_unary(c->chunk, c->prev.line, OP_GET_LOCAL, idx);
        return;
      }
    }
    // in global scope or no local variable found in local and parent scopes
    size_t idx = chunk_record_const(c->chunk, var);
    chunk_write_unary(c->chunk, c->prev.line, OP_GET_GLOBAL, idx);
  }
}

static void compile_and(Compiler *c, bool _) {
  size_t jump_if_false = chunk_reserve_unary(c->chunk, c->prev.line);
  size_t begin_label = chunk_len(c->chunk);
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
  compile_precedence(c, PREC_AND);
  assert(chunk_len(c->chunk) >= begin_label);
  chunk_patch_unary(c->chunk, jump_if_false, OP_JUMP_IF_FALSE,
                    chunk_len(c->chunk) - begin_label);
}

static void compile_or(Compiler *c, bool _) {
  size_t jump_if_true = chunk_reserve_unary(c->chunk, c->prev.line);
  size_t begin_label = chunk_len(c->chunk);
  chunk_write_operation(c->chunk, c->prev.line, OP_POP);
  compile_precedence(c, PREC_OR);
  assert(chunk_len(c->chunk) >= begin_label);
  chunk_patch_unary(c->chunk, jump_if_true, OP_JUMP_IF_TRUE,
                    chunk_len(c->chunk) - begin_label);
}

static void compile_grouping(Compiler *c, bool _) {
  compile_expression(c);
  consume(c, TOKEN_RIGHT_PAREN, "missing paren after expression");
}

bool compile(const char *src, Chunk *chunk) {
  Compiler c = {0};
  c.chunk = chunk;
  c.lex = lex(src);
  advance(&c);
  while (!match(&c, TOKEN_EOF)) {
    compile_declaration(&c);
  }
  chunk_write_operation(chunk, c.current.line, OP_RETURN);
  bool had_error = c.had_error;
  compiler_free(&c);
  return !had_error;
}

static CompileRule rules[] = {
    {compile_grouping, 0, PREC_NONE},           // TOKEN_LEFT_PAREN
    {0, 0, PREC_NONE},                          // TOKEN_RIGHT_PAREN
    {0, 0, PREC_NONE},                          // TOKEN_LEFT_BRACE
    {0, 0, PREC_NONE},                          // TOKEN_RIGHT_BRACE
    {0, 0, PREC_NONE},                          // TOKEN_COMMA
    {0, 0, PREC_NONE},                          // TOKEN_DOT
    {compile_unary, compile_binary, PREC_TERM}, // TOKEN_MINUS
    {0, compile_binary, PREC_TERM},             // TOKEN_PLUS
    {0, 0, PREC_NONE},                          // TOKEN_SEMICOLON
    {0, compile_binary, PREC_FACTOR},           // TOKEN_SLASH
    {0, compile_binary, PREC_FACTOR},           // TOKEN_STAR
    {compile_unary, 0, PREC_NONE},              // TOKEN_BANG
    {0, compile_binary, PREC_EQUALITY},         // TOKEN_BANG_EQUAL
    {0, 0, PREC_NONE},                          // TOKEN_EQUAL
    {0, compile_binary, PREC_EQUALITY},         // TOKEN_EQUAL_EQUAL
    {0, compile_binary, PREC_COMPARISON},       // TOKEN_GREATER
    {0, compile_binary, PREC_COMPARISON},       // TOKEN_GREATER_EQUAL
    {0, compile_binary, PREC_COMPARISON},       // TOKEN_LESS
    {0, compile_binary, PREC_COMPARISON},       // TOKEN_LESS_EQUAL
    {compile_variable, 0, PREC_NONE},           // TOKEN_IDENTIFIER
    {compile_string, 0, PREC_NONE},             // TOKEN_STRING
    {compile_int, 0, PREC_NONE},                // TOKEN_INT
    {compile_float, 0, PREC_NONE},              // TOKEN_FLOAT
    {0, compile_and, PREC_AND},                 // TOKEN_AND
    {0, 0, PREC_NONE},                          // TOKEN_CLASS
    {0, 0, PREC_NONE},                          // TOKEN_ELSE
    {compile_literal, 0, PREC_NONE},            // TOKEN_FALSE
    {0, 0, PREC_NONE},                          // TOKEN_FOR
    {0, 0, PREC_NONE},                          // TOKEN_FUN
    {0, 0, PREC_NONE},                          // TOKEN_IF
    {compile_literal, 0, PREC_NONE},            // TOKEN_NIL
    {0, compile_or, PREC_OR},                   // TOKEN_OR
    {0, 0, PREC_NONE},                          // TOKEN_PRINT
    {0, 0, PREC_NONE},                          // TOKEN_RETURN
    {0, 0, PREC_NONE},                          // TOKEN_SUPER
    {0, 0, PREC_NONE},                          // TOKEN_THIS
    {compile_literal, 0, PREC_NONE},            // TOKEN_TRUE
    {0, 0, PREC_NONE},                          // TOKEN_VAR
    {0, 0, PREC_NONE},                          // TOKEN_WHILE
    {0, 0, PREC_NONE},                          // TOKEN_ERROR
    {0, 0, PREC_NONE},                          // TOKEN_EOF
};
