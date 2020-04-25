#include "bytecode.h" // Chunk, chunk_dissasamble
#include "compiler.h" // compile
#include "mem.h"      // mem_allocation_summary

#include <stdio.h>  // stderr, fopen, fprintf, fseek, ftell, SEEK_END, SEEK_SET
                    // fread, fclose
#include <stdlib.h> // EXIT_SUCCESS, exit, size_t, malloc

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

int main(int argc, char *argv[]) {
  if (argc == 1) {
    // repl();
  } else if (argc == 2) {
    char *src = read_file(argv[1]);
    Chunk c = {0};
    if (compile(src, &c)) {
      chunk_disassamble_src(src, &c);
    } else {
      fprintf(stderr, "there were errors during compilation\n");
    }
  } else {
    fprintf(stderr, "usage: %s [path]\n", argv[0]);
  }

#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
