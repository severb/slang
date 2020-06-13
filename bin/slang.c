#include "bytecode.h" // Chunk, chunk_*
#include "compiler.h" // compile
#include "mem.h"      // mem_stats
#include "vm.h"       // interpret

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "could not open file \"%s\"\n", path);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "not enough memory to read file \"%s\"\n", path);
        exit(1);
    }
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "could not read file \"%s\"\n", path);
        exit(1);
    }
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

// exported for the web
void disassamble(const char *src) {
    Chunk c = {0};
    if (compile(src, &c)) {
        chunk_disassamble_src(&c, src);
    }
    chunk_destroy(&c);
}

bool run(const char *src) {
    bool success = false;
    Chunk c = {0};
    if (compile(src, &c)) {
#ifdef SLANG_DEBUG
        chunk_disassamble_src(&c, src);
#endif
        success = interpret(&c);
    }
    chunk_destroy(&c);
    return success;
}

int main(int argc, char *argv[]) {
    bool success = false;
    if (argc == 1) {
        // repl();
    } else if (argc == 2) {
        char *src = read_file(argv[1]);
        success = run(src);
        free(src);
    } else {
        fprintf(stderr, "usage: %s [path]\n", argv[0]);
    }

#ifdef SLANG_DEBUG
    if (mem_stats.bytes != 0) {
        fprintf(stderr, "unfreed memory: %zu\n", mem_stats.bytes);
    }
#endif
    assert(mem_stats.bytes == 0 && "unfreed memory");

    if (!success) {
        exit(1);
    }

    return EXIT_SUCCESS;
}
