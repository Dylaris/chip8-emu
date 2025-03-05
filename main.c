#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "chip8.h"

#define INVALID_KEY -1

typedef struct {
    SDL_Window *win;
    SDL_Renderer *renderer;
} sdl_handle;

typedef struct {
    u8 *buf;
    long size;
} FileContent;

sdl_handle g_sdl;

static void init(int scale)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) ERR(SDL_GetError());

    g_sdl.win = SDL_CreateWindow("Chip-8 Emulator",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH * scale, HEIGHT * scale, SDL_WINDOW_SHOWN);
    if (!g_sdl.win) ERR(SDL_GetError());

    g_sdl.renderer = SDL_CreateRenderer(g_sdl.win, -1, SDL_RENDERER_ACCELERATED);
    if (!g_sdl.renderer) ERR(SDL_GetError());
}

static void draw(Chip8 *vm, int scale)
{
    SDL_SetRenderDrawColor(g_sdl.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_sdl.renderer);

    for (u8 y = 0; y < HEIGHT; y++) {
        for (u8 x = 0; x < WIDTH; x++) {
            if (get_pixel(vm, x, y)) {
                SDL_Rect rect = {x * scale, y * scale, scale, scale};
                SDL_SetRenderDrawColor(g_sdl.renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(g_sdl.renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(g_sdl.renderer);
}

static void destroy()
{
    SDL_DestroyRenderer(g_sdl.renderer);
    SDL_DestroyWindow(g_sdl.win);
    SDL_Quit();
}

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

static void loop(Chip8 *vm, int scale)
{
    SDL_Event evt;

    while (1) {
        exec(vm);

        while (SDL_PollEvent(&evt) != 0) {
            int key;
            switch (evt.type) {
            case SDL_QUIT:
                destroy();
                return;
            case SDL_KEYDOWN:
                if (vm->wait_key) {
                    key = key_map(evt.key.keysym.sym);
                    if (key == INVALID_KEY) break;
                    vm->key_state[key] = 1;
                    vm->v[vm->wait_reg] = key;
                    vm->wait_key = 0;
                }
                break;
            case SDL_KEYUP:
                key = key_map(evt.key.keysym.sym);
                if (key == INVALID_KEY) break;
                vm->key_state[key] = 0;
                break;
            }
        }
        if (vm->dt > 0) vm->dt--;
        if (vm->st > 0) vm->st--;

        draw(vm, scale);

        SDL_Delay(16);
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

    const int SCALE = 15;
    init(SCALE);
    loop(vm, SCALE);

    free(vm);
    return 0;
}
