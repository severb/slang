#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "table.h"
#include "vm.h"

static void repl() {
  // REPL lines are interned as a memory management hack.
  // We need to preserve the lines because static strings refer to them.
  Table lines;
  tableInit(&lines);

  VM vm;
  vmInit(&vm);

  char line[1024];

  for (;;) {
    printf("> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    int len = strlen(line);
    line[len - 1] = '\0'; // overwrite \n
    ObjStringBase *l = internObjString(&lines, line, len);
    interpret(&vm, OBJ_AS_OBJSTRING(l)->chars);
  }

  vmFree(&vm);

  tableFreeKeys(&lines);
  tableFree(&lines);
}

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  fseek(file, 0L, SEEK_SET);

  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char *path) {
  char *source = readFile(path);

  VM vm;
  vmInit(&vm);
  InterpretResult result = interpret(&vm, source);
  vmFree(&vm);

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
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: %s [path]\n", argv[0]);
  }

  return 0;
}
