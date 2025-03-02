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

#define EXEC_OK     0
#define EXEC_WAIT   1
#define EXEC_END    2

typedef uint8_t  u8;
typedef uint16_t u16;

typedef struct chip8_vm {
    u8 ram[RAM_SZ];
    u16 *_stack;
    u8 *screen;
    u16 _size;     // free use memory size

    u8 v[16], dt, st, sp;
    u16 i, pc;

    u8 keyState[16]; 
    u8 waitKey, waitReg;
} chip8_vm;

chip8_vm *Chip8_New(const u8 *prog, const long size);
int Chip8_Exec(chip8_vm *vm);
int Chip8_GetPixel(chip8_vm *vm, u8 x, u8 y);

#define ERR(msg) do { \
        fprintf(stderr, "error: %s at %d in %s\n", msg, __LINE__, __FILE__); \
        exit(1); \
    } while (0)

#endif  /* CHIP8_H */
