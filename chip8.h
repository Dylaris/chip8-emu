#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#define STACK_OFF   0xEA0
#define SCREEN_OFF  0xF00 

#define RAM_SZ      0x1000
#define STACK_SZ    (SCREEN_OFF - STACK_OFF)
#define SCREEN_SZ   (RAM_SZ - SCREEN_OFF)

#define WIDTH   64
#define HEIGHT  32

typedef uint8_t  u8;
typedef uint16_t u16;

typedef struct chip8_vm {
    u8 ram[RAM_SZ];
    u16 *_stack;
    u8 *screen;
    u16 _resSz;     // reserved size for internal use

    u8 v[16], dt, st, sp;
    u16 i, pc;
} chip8_vm;

chip8_vm *Chip8_New(const u8 *prog, const long size);

#endif  /* CHIP8_H */
