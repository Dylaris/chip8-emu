#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>


typedef struct FileContent {
    u8 *buf;
    long size;
} FileContent;

static FileContent read_file(const char *filename)
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

    FileContent result = {};
    result.buf = buf;
    result.size = length;

    return result;
}

int main(int argc, char **argv)
{
    if (argc != 2) 
        ERR("<USE>: ./chip8 program");

    FileContent content = read_file(argv[1]);
    Chip8 *vm = new(content.buf, content.size);
    while (exec(vm) != EXEC_END);

    free(vm);
    return 0;
}
