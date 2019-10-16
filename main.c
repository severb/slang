#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "intern.h"
#include "str.h"
#include "table.h"
#include "vm.h"

static void repl() {
  Intern intern;
  intern_init(&intern);
  Table globals;
  table_init(&globals);
  VM vm;
  vm_init(&vm, &globals, &intern);
  char line[16384];
  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    Val interned_line = intern_str(&intern, str_new(line, strlen(line) + 1));
    if (interpret(&vm, interned_line.slice.c) != INTERPRET_OK)
      vm_destroy(&vm);
  }
  vm_destroy(&vm);
  table_destroy(&globals);
  intern_destroy(&intern);

  extern size_t mem_allocated;
  extern size_t mem_freed;
  printf("mem stats: %zu(alloc) %zu(freed)", mem_allocated, mem_freed);
}

static char *read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  char *buffer = malloc(file_size + 1);
  if (!buffer) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
  if (bytes_read < file_size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytes_read] = '\0';
  fclose(file);
  return buffer;
}

static void run_file(const char *path) {
  char *source = read_file(path);
  Intern intern;
  intern_init(&intern);
  Table globals;
  table_init(&globals);
  VM vm;
  vm_init(&vm, &globals, &intern);
  InterpretResult result = interpret(&vm, source);
  vm_destroy(&vm);
  table_destroy(&globals);
  intern_destroy(&intern);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    run_file(argv[1]);
  } else {
    fprintf(stderr, "Usage: %s [path]\n", argv[0]);
  }
  return EXIT_SUCCESS;
}
