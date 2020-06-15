#include "compiler.h"

#include "bytecode.h" // Chunk
#include "dynarray.h" // dynarray_*, DynamicArray
#include "lex.h"      // Lexer, lex_consume
#include "list.h"     // List, list_*
#include "mem.h"      // mem_allocate
#include "str.h"      // Slice, slice
#include "tag.h"      // Tag, *_tag, tag_*

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NDEBUG
static_assert(SIZE_MAX <= UINT64_MAX, "cannot safely cast size_t to uint64_t VM operands");
#endif

// Supporting the 'break' statement requires a lot of bookkeeping in a single-pass compiler
typedef struct {
    size_t pop_bookmark;  // bookmark to the OP_POP_N statement
    size_t jump_bookmark; // bookmark to the OP_JUMP statement
    uint64_t locals;      // the number of locals to pop
} Break;

dynarray_declare(Break);
dynarray_define(Break);

typedef struct {
    bool is_loop;
    size_t continue_label;      // if it's a loop
    size_t continue_locals;     // the numbe of locals defined before the continue label
    List locals;                // list of locals
    DynamicArray(Break) breaks; // list of break statements
    Tag uninitialized;          // a var that's currently being initialized (used for var a = a;)
} Block;

dynarray_declare(Block);
dynarray_define(Block);

typedef struct {
    Token current;
    Token prev;
    Lexer lex;
    Chunk *chunk;
    bool had_error;
    bool panic_mode;
    DynamicArray(Block) block_queue;
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

void trace_enter(const char *, const Compiler *);
void trace_exit(void);

#ifdef SLANG_DEBUG
static int indent = 0;
void trace_enter(const char *f, const Compiler *c) {
    for (int i = indent; i > 0; i--) {
        fputs("  ", stderr);
    }
    fputs(f, stderr);
    fputs(": ", stderr);
    fprintf(stderr, "%.*s\n", (int)(c->prev.end - c->prev.start), c->prev.start);
    indent++;
}
void trace_exit(void) { indent--; }
#else
void trace_enter(const char *f, const Compiler *c) {
    (void)f; // unused
    (void)c; // unused
}
void trace_exit(void) {}
#endif

// a few forward declarations
static void compile_statement(Compiler *);
static void compile_declaration(Compiler *);
static void compile_var_declaration(Compiler *);

static void compiler_destroy(Compiler *c) {
    for (size_t i = 0; i < dynarray_len(Block)(&c->block_queue); i++) {
        Block *b = dynarray_get(Block)(&c->block_queue, i);
        dynarray_destroy(Break)(&b->breaks);
        list_destroy(&b->locals);
    }
    dynarray_destroy(Block)(&c->block_queue);
    *c = (Compiler){0};
}

static void error_print(const Token *t, const char *msg) {
    fprintf(stderr, "[line %zu] error", t->line + 1);
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

static void err_at_current(Compiler *c, const char *msg) { err_at_token(c, &c->current, msg); }

static void err_at_prev(Compiler *c, const char *msg) { err_at_token(c, &c->prev, msg); }

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
    (void)_; // unused
    trace_enter("compile_int", c);
    Token token = c->prev;
    char buf[token.end - token.start + 1];
    char *b = buf;
    for (const char *c = token.start; c < token.end; c++) {
        if (*c != '_') {
            *b = *c;
            b++;
        }
    }
    *b = '\0';
    long long i = strtoll(buf, /*str_end*/ 0, /*base*/ 0);
    if (errno == ERANGE) {
        err_at_prev(c, "integer constant out of range");
        trace_exit();
        return;
    }
    assert(i >= 0 && "tokenizer returned negative integer");
    if (i > INT64_MAX) {
        err_at_prev(c, "integer constant out of range");
        trace_exit();
        return;
    }
    Tag t = int_to_tag(i);
    size_t idx = chunk_record_const(c->chunk, t);
    chunk_write_unary(c->chunk, c->prev.line, OP_GET_CONSTANT, idx);
    trace_exit();
    return;
}

static void compile_float(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_float", c);
    Token token = c->prev;
    char buf[token.end - token.start + 1];
    char *b = buf;
    for (const char *c = token.start; c < token.end; c++) {
        if (*c != '_') {
            *b = *c;
            b++;
        }
    }
    *b = '\0';
    double d = strtod(buf, 0);
    if (errno == ERANGE) {
        err_at_prev(c, "float constant out of range");
        trace_exit();
        return;
    }
    assert(d >= 0 && "tokenizer returend negative foalt");
    size_t idx = chunk_record_const(c->chunk, double_to_tag(d));
    chunk_write_unary(c->chunk, c->prev.line, OP_GET_CONSTANT, idx);
    trace_exit();
}

static void compile_string(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_string", c);
    // TODO: use a memory pool
    Slice *s = mem_allocate(sizeof(*s));
    *s = slice(c->prev.start + 1 /*skip 1st quote */, c->prev.end - 1 /* skip last quotes */);
    size_t idx = chunk_record_const(c->chunk, slice_to_tag(s));
    chunk_write_unary(c->chunk, c->prev.line, OP_GET_CONSTANT, idx);
    trace_exit();
}

static void compile_literal(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_literal", c);
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
        assert(0 && "unknown literal");
    }
    trace_exit();
}

static void enter_block(Compiler *c) {
    Block block = (Block){.uninitialized = TAG_NIL};
    dynarray_append(Block)(&c->block_queue, &block);
}

static bool in_block(const Compiler *c) { return dynarray_len(Block)(&c->block_queue) > 0; }

static Block *top_block(Compiler *c) {
    assert(in_block(c) && "not in a block");
    size_t len = dynarray_len(Block)(&c->block_queue);
    return dynarray_get(Block)(&c->block_queue, len - 1);
}

static Block *top_loop_block(Compiler *c) {
    for (size_t i = dynarray_len(Block)(&c->block_queue); i > 0; i--) {
        Block *b = dynarray_get(Block)(&c->block_queue, i - 1);
        if (b->is_loop) {
            return b;
        }
    }
    return 0;
}

static bool in_loop(Compiler *c) { return top_loop_block(c) != 0; }

static void set_continue_label(Compiler *c) {
    Block *top = top_block(c);
    top->is_loop = true;
    top->continue_label = chunk_label(c->chunk);
    top->continue_locals = list_len(&top->locals);
}

static void exit_block(Compiler *c) {
    assert(in_block(c) && "not in a block");
    Block *top = top_block(c);
    size_t brk_len = dynarray_len(Break)(&top->breaks);
    size_t loc_len = list_len(&top->locals);
    for (size_t i = 0; i < brk_len; i++) { // bump break locals count
        Break *brk = dynarray_get(Break)(&top->breaks, i);
        // TODO: use safe math for bumping locals
        brk->locals += loc_len;
    }
    if (!top->is_loop && brk_len) {
        // copy breaks in the parent block
        assert(in_loop(c) && "breaks escaped the loop");
        size_t blk_q_len = dynarray_len(Block)(&c->block_queue);
        Block *toptop = dynarray_get(Block)(&c->block_queue, blk_q_len - 2);
        for (size_t i = 0; i < brk_len; i++) {
            Break *brk = dynarray_get(Break)(&top->breaks, i);
            dynarray_append(Break)(&toptop->breaks, brk);
        }
    }
    if (loc_len > 1) {
        chunk_write_unary(c->chunk, c->prev.line, OP_POP_N, loc_len);
    } else if (loc_len == 1) {
        chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    }
    if (top->is_loop) {
        // patch all breaks
        for (size_t i = 0; i < dynarray_len(Break)(&top->breaks); i++) {
            Break *brk = dynarray_get(Break)(&top->breaks, i);
            chunk_patch_unary_operand(c->chunk, brk->pop_bookmark, OP_POP_N, brk->locals);
            chunk_patch_unary(c->chunk, brk->jump_bookmark, OP_JUMP);
        }
    }
    dynarray_destroy(Break)(&top->breaks);
    list_destroy(&top->locals);
    assert(tag_eq(top->uninitialized, TAG_NIL) && "uninitialized vars at block end");
    size_t len = dynarray_len(Block)(&c->block_queue);
    dynarray_trunc(Block)(&c->block_queue, len - 1);
}

static bool declare_local(Compiler *c, Tag var) {
    Block *top = top_block(c);
    if (list_find_from(&top->locals, var, 0)) {
        tag_free(var);
        err_at_prev(c, "variable already defined");
        return false;
    }
    list_append(&top->locals, var);
    top->uninitialized = var;
    return true;
}

static void initialize_local(Compiler *c) {
    Block *top = top_block(c);
    top->uninitialized = TAG_NIL;
}

static bool resolve_local(Compiler *c, Tag var, size_t *idx) {
    Block *top = top_block(c);
    if (tag_eq(top->uninitialized, var)) {
        err_at_prev(c, "local variable used in its own initializer");
        return false;
    }
    // traverse the blocks in reverse order and look for var
    // if found, sum its index with the number of locals in the remaining blocks
    for (size_t i = dynarray_len(Block)(&c->block_queue); i > 0; i--) {
        size_t ii = i - 1;
        Block *b = dynarray_get(Block)(&c->block_queue, ii);
        *idx = 0;
        if (list_find_from(&b->locals, var, idx)) {
            for (size_t j = ii; j > 0; j--) {
                size_t jj = j - 1;
                Block *parent_block = dynarray_get(Block)(&c->block_queue, jj);
                *idx += list_len(&parent_block->locals);
            }
            return true;
        }
    }
    return false;
}

static void compile_precedence(Compiler *c, Precedence p) {
    trace_enter("compile_precedence", c);
    advance(c);
    CompileFn prefix_rule = rules[c->prev.type].prefix;
    if (prefix_rule == 0) {
        err_at_prev(c, "invalid expression");
        trace_exit();
        return;
    }
    bool can_assign = p <= PREC_ASSIGNMENT;
    prefix_rule(c, can_assign);
    while (p <= rules[c->current.type].precedence) {
        advance(c);
        CompileFn infix_rule = rules[c->prev.type].infix;
        assert(infix_rule && "precedence set but infix function missing");
        infix_rule(c, can_assign);
    }
    // assignment is handled in rules, if we see an assignment here it's an err
    if (can_assign && match(c, TOKEN_EQUAL)) {
        err_at_current(c, "invalid target assignment");
        trace_exit();
        return;
    }
    trace_exit();
}

static void compile_expression(Compiler *c) {
    trace_enter("compile_expression", c);
    compile_precedence(c, PREC_ASSIGNMENT);
    trace_exit();
}

static void compile_print_statement(Compiler *c) {
    trace_enter("compile_print_statement", c);
start:
    compile_expression(c);
    chunk_write_operation(c->chunk, c->prev.line, OP_PRINT);
    if (match(c, TOKEN_COMMA)) {
        goto start;
    }
    consume(c, TOKEN_SEMICOLON, "missing semicolon after print");
    chunk_write_operation(c->chunk, c->prev.line, OP_PRINT_NL);
    trace_exit();
}

static void compile_if_statement(Compiler *c) {
    trace_enter("compile_if_statement", c);
    consume(c, TOKEN_LEFT_PAREN, "missing paren before if condition");
    compile_expression(c);
    consume(c, TOKEN_RIGHT_PAREN, "missing paren after if condition");
    size_t jump_if_false = chunk_reserve_unary(c->chunk, c->prev.line);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    compile_statement(c);
    size_t jump_after_else = chunk_reserve_unary(c->chunk, c->prev.line);
    chunk_patch_unary(c->chunk, jump_if_false, OP_JUMP_IF_FALSE);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    if (match(c, TOKEN_ELSE)) {
        compile_statement(c);
    }
    chunk_patch_unary(c->chunk, jump_after_else, OP_JUMP);
    trace_exit();
}

static void compile_while_statement(Compiler *c) {
    trace_enter("compile_while_statement", c);
    size_t start = chunk_label(c->chunk);
    consume(c, TOKEN_LEFT_PAREN, "missing paren before while condition");
    enter_block(c);
    set_continue_label(c);
    compile_expression(c);
    consume(c, TOKEN_RIGHT_PAREN, "missing paren after while condition");
    size_t jump_if_false = chunk_reserve_unary(c->chunk, c->prev.line);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    compile_statement(c);
    chunk_loop_to_label(c->chunk, c->prev.line, start);
    chunk_patch_unary(c->chunk, jump_if_false, OP_JUMP_IF_FALSE);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    exit_block(c);
    trace_exit();
}

static void compile_expression_statement(Compiler *c) {
    trace_enter("compile_expression_statement", c);
    compile_expression(c);
    consume(c, TOKEN_SEMICOLON, "missing semicolon after expression statement");
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    trace_exit();
}

// this loop:
// if (var a = 10; a < 10; a++) {
//     <body>
//
// is compiled as if it was written like:
// {
//     var a = 10;
// condition:
//     if (!(a < 10))
//         goto end;
//     goto body;
// increment:
//     a = a + 1;
//     goto condition;
// body:
//     <body>
//     goto inc;
// end:
// }

static void compile_for_statement(Compiler *c) {
    trace_enter("compile_for_statement", c);
    consume(c, TOKEN_LEFT_PAREN, "missing paren after for");
    enter_block(c);
    if (match(c, TOKEN_SEMICOLON)) {
        // no initializer
    } else if (match(c, TOKEN_VAR)) {
        compile_var_declaration(c);
    } else {
        compile_expression_statement(c);
    }
    size_t condition = chunk_label(c->chunk);
    if (match(c, TOKEN_SEMICOLON)) {
        // no condition
        chunk_write_operation(c->chunk, c->prev.line, OP_TRUE);
    } else {
        // can't use complie_expression_statement() as it pop the value
        compile_expression(c);
        consume(c, TOKEN_SEMICOLON, "missing semicolon after for condition");
    }
    size_t jump_if_false_to_end = chunk_reserve_unary(c->chunk, c->prev.line);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    size_t jump_to_body = chunk_reserve_unary(c->chunk, c->prev.line);
    size_t increment = chunk_label(c->chunk);
    set_continue_label(c);
    if (match(c, TOKEN_RIGHT_PAREN)) {
        // no increment
    } else {
        compile_expression(c);
        consume(c, TOKEN_RIGHT_PAREN, "missing paren after for");
        chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    }
    chunk_loop_to_label(c->chunk, c->prev.line, condition);
    chunk_patch_unary(c->chunk, jump_to_body, OP_JUMP);
    compile_statement(c); // the body
    chunk_loop_to_label(c->chunk, c->prev.line, increment);
    chunk_patch_unary(c->chunk, jump_if_false_to_end, OP_JUMP_IF_FALSE);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    exit_block(c);
    trace_exit();
}

static void compile_block(Compiler *c) {
    trace_enter("compile_block", c);
    while (!match(c, TOKEN_RIGHT_BRACE)) {
        compile_declaration(c);
        if (match(c, TOKEN_EOF)) {
            err_at_current(c, "missing closing brace missing after block");
            trace_exit();
            return;
        }
    }
    trace_exit();
}

static void compile_continue_statement(Compiler *c) {
    trace_enter("compile_continue_statement", c);
    if (!in_loop(c)) {
        err_at_prev(c, "cannot continue outside of a loop");
        trace_exit();
        return;
    }
    consume(c, TOKEN_SEMICOLON, "missing semicolon after continue");
    size_t locals = 0;
    Block *blk;
    for (size_t i = dynarray_len(Block)(&c->block_queue); i > 0; i--) {
        size_t ii = i - 1;
        blk = dynarray_get(Block)(&c->block_queue, ii);
        locals += list_len(&blk->locals);
        if (blk->is_loop) {
            break;
        }
    }
    assert(locals >= blk->continue_locals && "continue locals invariant");
    locals -= blk->continue_locals;
    if (locals > 1) {
        chunk_write_unary(c->chunk, c->prev.line, OP_POP_N, locals);
    } else if (locals == 1) {
        chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    }
    chunk_loop_to_label(c->chunk, c->prev.line, blk->continue_label);
    trace_exit();
}

static void compile_break_statement(Compiler *c) {
    trace_enter("compile_break_statement", c);
    // TODO: fix local vairable popping
    if (!in_loop(c)) {
        err_at_prev(c, "cannot break outside of a loop");
        trace_exit();
        return;
    }
    consume(c, TOKEN_SEMICOLON, "missing semicolon after break");
    Block *b = top_block(c);
    size_t pop_bookmark = chunk_reserve_unary(c->chunk, c->prev.line);
    size_t jump_bookmark = chunk_reserve_unary(c->chunk, c->prev.line);
    Break brk = (Break){.pop_bookmark = pop_bookmark, .jump_bookmark = jump_bookmark};
    dynarray_append(Break)(&b->breaks, &brk);
    trace_exit();
}

static void compile_statement(Compiler *c) {
    trace_enter("compile_statement", c);
    if (match(c, TOKEN_PRINT)) {
        compile_print_statement(c);
    } else if (match(c, TOKEN_IF)) {
        compile_if_statement(c);
    } else if (match(c, TOKEN_WHILE)) {
        compile_while_statement(c);
    } else if (match(c, TOKEN_FOR)) {
        compile_for_statement(c);
    } else if (match(c, TOKEN_CONTINUE)) {
        compile_continue_statement(c);
    } else if (match(c, TOKEN_BREAK)) {
        compile_break_statement(c);
    } else if (match(c, TOKEN_LEFT_BRACE)) {
        // TODO: consider checking if it's a dictionary literal
        enter_block(c);
        compile_block(c);
        exit_block(c);
    } else {
        compile_expression_statement(c);
    }
    trace_exit();
}

Tag var_from_token(Token t) {
    // TODO: use a static slice in the caller instead of allways allocating
    // TODO: use a memory pool
    Slice *s = mem_allocate(sizeof(*s));
    *s = slice(t.start, t.end);
    return slice_to_tag(s);
}

static void compile_var_declaration(Compiler *c) {
    trace_enter("compile_var_declaration", c);
start:
    consume(c, TOKEN_IDENTIFIER, "missing variable name");
    Tag var = var_from_token(c->prev);
    if (in_block(c)) {
        if (!declare_local(c, var)) {
            trace_exit();
            return;
        };
    }
    if (match(c, TOKEN_EQUAL)) {
        compile_expression(c);
    } else {
        chunk_write_operation(c->chunk, c->prev.line, OP_NIL);
    }
    if (in_block(c)) {
        initialize_local(c);
        size_t idx;
        bool found = resolve_local(c, var, &idx);
        (void)found;
        assert(found && "cannot resolve local variable during declaration");
        chunk_write_unary(c->chunk, c->prev.line, OP_SET_LOCAL, idx);
    } else {
        size_t idx = chunk_record_const(c->chunk, var);
        chunk_write_unary(c->chunk, c->prev.line, OP_DEF_GLOBAL, idx);
    }
    if (match(c, TOKEN_COMMA)) {
        goto start;
    }
    consume(c, TOKEN_SEMICOLON, "missing semicolon after variable declaration");
    trace_exit();
}

static void compile_declaration(Compiler *c) {
    trace_enter("compile_declaration", c);
    if (match(c, TOKEN_VAR)) {
        compile_var_declaration(c);
    } else {
        compile_statement(c);
    }
    synchronize(c);
    trace_exit();
}

static void compile_unary(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_unary", c);
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
        assert(0 && "unknown unary token");
    }
    trace_exit();
}

static void compile_binary(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_binary", c);
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
    case TOKEN_PERCENT:
        chunk_write_operation(c->chunk, t.line, OP_REMAINDER);
        break;
    default:
        assert(0 && "unknown binary token");
    }
    trace_exit();
}

static void get_var(Compiler *c, Tag var) {
    if (in_block(c)) { // in local scope
        size_t idx;
        if (resolve_local(c, var, &idx)) {
            tag_free(var);
            chunk_write_unary(c->chunk, c->prev.line, OP_GET_LOCAL, idx);
            return;
        }
    }
    // in global scope or no local variable found in local and parent scopes
    size_t idx = chunk_record_const(c->chunk, var);
    chunk_write_unary(c->chunk, c->prev.line, OP_GET_GLOBAL, idx);
}

static void set_var(Compiler *c, Tag var) {
    if (in_block(c)) { // in local scope
        size_t idx;
        if (resolve_local(c, var, &idx)) {
            tag_free(var);
            chunk_write_unary(c->chunk, c->prev.line, OP_SET_LOCAL, idx);
            return;
        }
    }
    // in global scope or no local variable found in local and parent scopes
    size_t idx = chunk_record_const(c->chunk, var);
    chunk_write_unary(c->chunk, c->prev.line, OP_SET_GLOBAL, idx);
}

static void compile_variable(Compiler *c, bool can_assign) {
    trace_enter(can_assign ? "compile_variable(T)" : "compile_variable(F)", c);
    Tag var_get = var_from_token(c->prev);
    Tag var_set = var_from_token(c->prev);
    if (can_assign && match(c, TOKEN_EQUAL)) {
        compile_expression(c);
        tag_free(var_get);
        set_var(c, var_set);
    } else if (can_assign && match(c, TOKEN_PLUS_EQUAL)) {
        get_var(c, var_get);
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_ADD);
        set_var(c, var_set);
    } else if (can_assign && match(c, TOKEN_MINUS_EQUAL)) {
        get_var(c, var_get);
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_NEGATE);
        chunk_write_operation(c->chunk, c->prev.line, OP_ADD);
        set_var(c, var_set);
    } else if (can_assign && match(c, TOKEN_STAR_EQUAL)) {
        get_var(c, var_get);
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_MULTIPLY);
        set_var(c, var_set);
    } else if (can_assign && match(c, TOKEN_SLASH_EQUAL)) {
        get_var(c, var_get);
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_DIVIDE);
        set_var(c, var_set);
    } else if (can_assign && match(c, TOKEN_PERCENT_EQUAL)) {
        get_var(c, var_get);
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_REMAINDER);
        set_var(c, var_set);
    } else {
        tag_free(var_set);
        get_var(c, var_get);
    }
    trace_exit();
}

static void compile_and(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_and", c);
    size_t jump_if_false = chunk_reserve_unary(c->chunk, c->prev.line);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    compile_precedence(c, PREC_AND);
    chunk_patch_unary(c->chunk, jump_if_false, OP_JUMP_IF_FALSE);
    trace_exit();
}

static void compile_or(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_or", c);
    size_t jump_if_true = chunk_reserve_unary(c->chunk, c->prev.line);
    chunk_write_operation(c->chunk, c->prev.line, OP_POP);
    compile_precedence(c, PREC_OR);
    chunk_patch_unary(c->chunk, jump_if_true, OP_JUMP_IF_TRUE);
    trace_exit();
}

static void compile_grouping(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_grouping", c);
    compile_expression(c);
    consume(c, TOKEN_RIGHT_PAREN, "missing paren after expression");
    trace_exit();
}

static void compile_dict(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_dict", c);
    chunk_write_operation(c->chunk, c->prev.line, OP_DICT);
    while (!match(c, TOKEN_RIGHT_BRACE)) {
        compile_expression(c);
        consume(c, TOKEN_COLON, "missing colon between key and value");
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_DICT_INIT);
        if (!match(c, TOKEN_COMMA)) {
            consume(c, TOKEN_RIGHT_BRACE, "missing right brace after dictionary literal");
            break;
        }
    }
    trace_exit();
}

static void compile_list(Compiler *c, bool _) {
    (void)_; // unused
    trace_enter("compile_list", c);
    chunk_write_operation(c->chunk, c->prev.line, OP_LIST);
    while (!match(c, TOKEN_RIGHT_BRACKET)) {
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_LIST_INIT);
        if (!match(c, TOKEN_COMMA)) {
            consume(c, TOKEN_RIGHT_BRACKET, "missing right bracket after list literal");
            break;
        }
    }
    trace_exit();
}

static void compile_item(Compiler *c, bool can_assign) {
    trace_enter(can_assign ? "compile_item(T)" : "compile_item(F)", c);
    if (match(c, TOKEN_RIGHT_BRACKET)) {
        if (!can_assign) {
            err_at_prev(c, "unexpected append operator");
            trace_exit();
            return;
        }
        consume(c, TOKEN_EQUAL, "missing assignment in append operand");
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_APPEND);
        trace_exit();
        return;
    }
    compile_expression(c);
    consume(c, TOKEN_RIGHT_BRACKET, "missing right bracket");
    if (can_assign && match(c, TOKEN_EQUAL)) { // an assignment
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_SET);
    } else if (can_assign && match(c, TOKEN_PLUS_EQUAL)) {
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_SHORT_ADD);
    } else if (can_assign && match(c, TOKEN_MINUS_EQUAL)) {
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_NEGATE);
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_SHORT_ADD);
    } else if (can_assign && match(c, TOKEN_STAR_EQUAL)) {
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_SHORT_MULTIPLY);
    } else if (can_assign && match(c, TOKEN_SLASH_EQUAL)) {
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_SHORT_DIVIDE);
    } else if (can_assign && match(c, TOKEN_PERCENT_EQUAL)) {
        compile_expression(c);
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_SHORT_REMAINDER);
    } else {
        chunk_write_operation(c->chunk, c->prev.line, OP_ITEM_GET);
    }
    trace_exit();
}

bool compile(const char *src, Chunk *chunk) {
    Compiler c = {.chunk = chunk, .lex = lex(src)};
    advance(&c);
    while (!match(&c, TOKEN_EOF)) {
        compile_declaration(&c);
    }
    chunk_write_operation(chunk, c.current.line, OP_RETURN);
    bool had_error = c.had_error;
    compiler_destroy(&c);
    return !had_error;
}

// This table matches token's enum definition order
static CompileRule rules[] = {
    {compile_grouping, 0, PREC_NONE},           // TOKEN_LEFT_PAREN
    {0, 0, PREC_NONE},                          // TOKEN_RIGHT_PAREN
    {compile_dict, 0, PREC_NONE},               // TOKEN_LEFT_BRACE
    {0, 0, PREC_NONE},                          // TOKEN_RIGHT_BRACE
    {compile_list, compile_item, PREC_CALL},    // TOKEN_LEFT_BRACKET
    {0, 0, PREC_NONE},                          // TOKEN_RIGHT_BRACKET
    {0, 0, PREC_NONE},                          // TOKEN_COMMA
    {0, 0, PREC_NONE},                          // TOKEN_DOT
    {compile_unary, compile_binary, PREC_TERM}, // TOKEN_MINUS
    {0, 0, PREC_NONE},                          // TOKEN_MINUS_EQUAL
    {0, compile_binary, PREC_TERM},             // TOKEN_PLUS
    {0, 0, PREC_NONE},                          // TOKEN_PLUS_EQUAL
    {0, 0, PREC_NONE},                          // TOKEN_COLON
    {0, 0, PREC_NONE},                          // TOKEN_SEMICOLON
    {0, compile_binary, PREC_FACTOR},           // TOKEN_SLASH
    {0, 0, PREC_NONE},                          // TOKEN_SLASH_EQUAL
    {0, compile_binary, PREC_FACTOR},           // TOKEN_STAR
    {0, 0, PREC_NONE},                          // TOKEN_STAR_EQUAL
    {0, compile_binary, PREC_FACTOR},           // TOKEN_PERCENT
    {0, 0, PREC_NONE},                          // TOKEN_PERCENT_EQUAL
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
    {0, 0, PREC_NONE},                          // TOKEN_BREAK
    {0, 0, PREC_NONE},                          // TOKEN_CONTINUE
    {0, 0, PREC_NONE},                          // TOKEN_ERROR
    {0, 0, PREC_NONE},                          // TOKEN_EOF
};
