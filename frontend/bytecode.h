#ifndef slang_bytecode_h
#define slang_bytecode_h

#include "dynarray.h" // DynamicArray, dynarray_*
#include "list.h"     // List, list_
#include "tag.h"      // Tag

#include <assert.h> // assert
#include <stdint.h> // uint8_t, SIZE_MAX, uint64_t

#define OPCODE(ENUM) ENUM,
typedef enum {
#include "bytecode_ops.incl"
} OpCode;
#undef OPCODE

typedef struct {
    DynamicArray(uint8_t) bytecode;
    DynamicArray(size_t) lines;
    List consts;
} Chunk;

void chunk_write_operation(Chunk *, size_t line, uint8_t op);
void chunk_write_unary(Chunk *, size_t line, uint8_t op, uint64_t operand);

size_t chunk_reserve_unary(Chunk *, size_t line);
void chunk_patch_unary(Chunk *, size_t bookmark, uint8_t op);

inline size_t chunk_label(const Chunk *c) { return dynarray_len(uint8_t)(&c->bytecode); }
inline void chunk_loop_to_label(Chunk *c, size_t line, size_t label) {
    size_t here = chunk_label(c);
    assert(here >= label);
    chunk_write_unary(c, line, OP_LOOP, chunk_label(c) - label);
}

size_t chunk_record_const(Chunk *, Tag);
inline Tag chunk_get_const(const Chunk *c, size_t idx) { return *list_get(&c->consts, idx); }

void chunk_seal(Chunk *);
void chunk_destroy(Chunk *);
void chunk_free(Chunk *);

size_t chunk_lines_delta(const Chunk *, size_t current_line, size_t offset);

void chunk_disassamble(const Chunk *);
void chunk_disassamble_src(const Chunk *, const char *);

inline size_t chunk_len(const Chunk *c) { return dynarray_len(uint8_t)(&c->bytecode); }

inline uint8_t chunk_read_opcode(const Chunk *c, size_t offset) {
    return *dynarray_get(uint8_t)(&c->bytecode, offset);
}

inline uint64_t chunk_read_operator(const Chunk *c, size_t *offset) {
    // TODO: split into a fast path and a call to a slow path to ensure inline
    uint64_t result = *dynarray_get(uint8_t)(&c->bytecode, (*offset)++);
    if (!(result & 0x80)) {
        return result;
    }
    result &= 0x7f;
    for (int i = 1; i < 8; i++) {
        uint64_t byte = *dynarray_get(uint8_t)(&c->bytecode, (*offset)++);
        if (!(byte & 0x80)) {
            return result | (byte << (7 * i));
        }
        result |= (byte & 0x7f) << (7 * i);
    }
    uint64_t byte = *dynarray_get(uint8_t)(&c->bytecode, (*offset)++);
    return result | (byte << 56);
}

#endif
