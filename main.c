#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>

#define ERR(msg) do { \
        fprintf(stderr, "error: %s\n", msg); \
        exit(1); \
    } while (0)

typedef struct file_content {
    u8 *buf;
    long size;
} file_content;

static file_content ReadFile(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) ERR("open file");

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8 *buf = malloc(length);
    if (buf == NULL) ERR("buf malloc");

    size_t read = fread(buf, 1, length, file);
    if (read != (size_t) length) ERR("read bytes");

    file_content result = {};
    result.buf = buf;
    result.size = length;

    return result;
}

int main(int argc, char **argv)
{
    if (argc != 2) 
        ERR("<USE>: ./chip8 program");

    file_content result = ReadFile(argv[1]);
    chip8_vm *vm = Chip8_New(result.buf, result.size);

    free(vm);
    return 0;
}
