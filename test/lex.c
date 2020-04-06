#include "lex.h" // Lexer, lex, lex_print

#include <stdio.h>  // stderr, fopen, fprintf, fseek, ftell, SEEK_END, SEEK_SET
                    // fread, fclose
#include <stdlib.h> // exit, EXIT_SUCCESS, size_t, malloc

static char *read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(1);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  char *buffer = malloc(file_size + 1);
  if (!buffer) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(1);
  }
  size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
  if (bytes_read < file_size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(1);
  }
  buffer[bytes_read] = '\0';
  fclose(file);
  return buffer;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <path>\n", argv[0]);
    exit(1);
  }
  char *source = read_file(argv[1]);
  Lexer l = lex(source);
  lex_print(&l);
  return EXIT_SUCCESS;
}
