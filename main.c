#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "chip8.h"

#define INVALID_KEY -1

typedef struct {
    u8 *buf;
    long size;
} FileContent;

static int key_map(int sdl_key)
{
    switch (sdl_key) {
    case SDLK_1: return 0x1;
    case SDLK_2: return 0x2;
    case SDLK_3: return 0x3;
    case SDLK_4: return 0xC;
    case SDLK_q: return 0x4;
    case SDLK_w: return 0x5;
    case SDLK_e: return 0x6;
    case SDLK_r: return 0xD;
    case SDLK_a: return 0x7;
    case SDLK_s: return 0x8;
    case SDLK_d: return 0x9;
    case SDLK_f: return 0xE;
    case SDLK_z: return 0xA;
    case SDLK_x: return 0x0;
    case SDLK_c: return 0xB;
    case SDLK_v: return 0xF;
    default:     return INVALID_KEY;
    }
}

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

    srand(time(0));

    FileContent content = read_file(argv[1]);
    Chip8 *vm = new(content.buf, content.size);
    while (exec(vm) != EXEC_END);

    free(vm);
    return 0;
}
