#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a < b) ? a : b)

static u8 fontSet[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80
};

#define MAX_FREE_SZ (STACK_OFF - 0x200)

chip8_vm *Chip8_New(const u8 *prog, const long size)
{
    if (size > MAX_FREE_SZ) ERR("out of memory");

    chip8_vm *vm = malloc(sizeof(chip8_vm));
    if (vm == NULL) ERR("vm malloc");

    memset(vm->ram, 0, RAM_SZ);
    memcpy(vm->ram, fontSet, sizeof(fontSet));
    memcpy(vm->ram + 0x200, prog, size);
    vm->_stack = (u16 *) &vm->ram[STACK_OFF];
    vm->screen = &vm->ram[SCREEN_OFF];
    vm->_size = (u16) size;
    vm->pc = 0x200;
    vm->sp = 0;
    vm->waitKey = 0;
    for (int i = 0; i < 16; i++) vm->keyState[i] = 0;

    return vm;
}

int Chip8_GetPixel(chip8_vm *vm, u8 x, u8 y)
{
    u8 byteIdx = (y * WIDTH + x) / 8;
    u8 bitOff  = (y * WIDTH + x) % 8;
    return vm->screen[byteIdx] & (0x80 >> bitOff);
}

static void SetPixel(chip8_vm *vm, u8 x, u8 y, u8 on)
{
    u8 byteIdx = (y * WIDTH + x) / 8;
    u8 bitOff  = (y * WIDTH + x) % 8;
    vm->screen[byteIdx] = on ? (vm->screen[byteIdx] |  (0x80 >> bitOff))
                             : (vm->screen[byteIdx] & ~(0x80 >> bitOff));
}

static void Op_0(chip8_vm *vm, u16 opcode)
{
    switch (opcode) {
    case 0x00E0: {
        for (int i = 0; i < WIDTH*HEIGHT/8; i++) 
            vm->screen[i] = 0; 
    } break;
    case 0x00EE: vm->pc = vm->_stack[vm->sp--]; break;
    default:     vm->pc = opcode & 0xFFF;       break;
    }
}

static void Op_1(chip8_vm *vm, u16 nnn) { vm->pc = nnn; }
static void Op_2(chip8_vm *vm, u16 nnn) { vm->_stack[vm->sp++] = vm->pc; vm->pc = nnn; }
static void Op_3(chip8_vm *vm, u8 *vx, u8 kk) { if (*vx == kk) vm->pc += 2; }
static void Op_4(chip8_vm *vm, u8 *vx, u8 kk) { if (*vx != kk) vm->pc += 2; }
static void Op_5(chip8_vm *vm, u8 *vx, u8 *vy) { if (*vx == *vy) vm->pc += 2; }
static void Op_6(u8 *vx, u8 kk) { *vx = kk; }
static void Op_7(u8 *vx, u8 kk) { *vx += kk; }
static void Op_8(u8 *vx, u8 *vy, u8 *vf, u8 suffix)
{
    switch (suffix) {
    case 0x0: *vx  = *vy; break;
    case 0x1: *vx |= *vy; break;
    case 0x2: *vx &= *vy; break;
    case 0x3: *vx ^= *vy; break;
    case 0x4: {
        u16 result = *vx + *vy;
        *vx = result & 0xFF;
        *vf = (result > 0xFF) ? 1 : 0;  // carry or not
    } break;
    case 0x5: {
        *vf = (*vx < *vy) ? 0 : 1;  // borrow or not
        *vx -= *vy;
    } break;
    case 0x6: {
        *vf = *vy & 0x01;
        *vy >>= 1;
        *vx = *vy;
    } break;
    case 0x7: {
        *vf = (*vy < *vx) ? 0 : 1;  // borrow or not
        *vx = *vy - *vx;
    } break;
    case 0xE: {
        *vf = *vy & 0x80;
        *vy <<= 1;
        *vx = *vy;
    } break;
    default:
    }
}
static void Op_9(chip8_vm *vm, u8 *vx, u8 *vy) { if (*vx != *vy) vm->pc += 2; }
static void Op_A(chip8_vm *vm, u16 nnn) { vm->i = nnn; }
static void Op_B(chip8_vm *vm, u16 nnn) { vm->pc = nnn + vm->v[0]; }
static void Op_C(u8 *vx, u8 kk) { *vx = (rand() % 0xFf) & kk; }
static void Op_D(chip8_vm *vm, u8 startX, u8 startY, u8 n)
{
    startX %= WIDTH;
    startY %= HEIGHT;
    int endX = MIN(startX + 8, WIDTH);
    int endY = MIN(startY + n, HEIGHT);

    vm->v[0xF] = 0;

    for (int y = startY; y < endY; y++) {
        u8 spriteByte = vm->ram[vm->i];
        for (int x = startX; x < endX; x++) {
            u8 off = (x - startX) % 8;
            u8 spritePixel = spriteByte & (0x80 >> off);
            u8 screenPixel = Chip8_GetPixel(vm, x, y);

            if (spritePixel) {
                if (screenPixel) vm->v[0xF] = 1;
                SetPixel(vm, x, y, screenPixel == 0 ? 1 : 0);
            }
        }
    }
}
static void Op_E(chip8_vm *vm, u8 *vx, u8 suffix)
{
    u8 keyPress = vm->keyState[*vx];
    switch (suffix) {
    case 0x1: vm->pc = (keyPress == 1) ? vm->pc + 2 : vm->pc; break;
    case 0xE: vm->pc = (keyPress == 0) ? vm->pc + 2 : vm->pc; break;
    default:
    }
    if (keyPress == 1) vm->keyState[*vx] = 0;
}
static void Op_F(chip8_vm *vm, u8 *vx, u8 suffix)
{
    switch (suffix) {
    case 0x07: *vx = vm->dt; break;
    case 0x0A: vm->waitKey = 1; vm->waitReg = vx - vm->v; break;
    case 0x15: vm->dt = *vx; break;
    case 0x18: vm->st = *vx; break;
    case 0x1E: vm->i += *vx; break;
    case 0x29: vm->i = 0x0 + (*vx) * 5; break;
    case 0x33: {
        vm->ram[vm->i + 0] = *vx >> 8;
        vm->ram[vm->i + 1] = *vx >> 4;
        vm->ram[vm->i + 2] = *vx >> 0;
    } break;
    case 0x55: {
        u8 count = vx - vm->v + 1;
        for (int off = 0; off < count; off++)
            vm->ram[vm->i + off] = vm->v[off];
        vm->i += count;
    } break;
    case 0x65: {
        u8 count = vx - vm->v + 1;
        for (int off = 0; off < count; off++)
            vm->v[off] = vm->ram[vm->i + off];
        vm->i += count;
    } break;
    default:
    } 
}

int Chip8_Exec(chip8_vm *vm)
{
    if (vm->waitKey == 1) return EXEC_WAIT;
    if (vm->pc > 0x200 + vm->_size) return EXEC_END;

    u16 opcode = (vm->ram[vm->pc] << 8) | vm->ram[vm->pc + 1];
    vm->pc += 2;

    /* Extra bit-fields from opcode */
    u8 prefix = (opcode >> 12) & 0xF;
    u16 nnn   = (opcode >>  0) & 0xFFF;
    u8 n      = (opcode >>  0) & 0xF;
    u8 x      = (opcode >>  8) & 0xF;
    u8 y      = (opcode >>  4) & 0xF;
    u8 kk     = (opcode >>  0) & 0xFF;

    u8 *vx = &vm->v[x];
    u8 *vy = &vm->v[y];
    u8 *vf = &vm->v[0xF];

    switch (prefix) {
    case 0x0: Op_0(vm, opcode); break;
    case 0x1: Op_1(vm, nnn);    break;
    case 0x2: Op_2(vm, nnn);    break;
    case 0x3: Op_3(vm, vx, kk); break;
    case 0x4: Op_4(vm, vx, kk); break;
    case 0x5: Op_5(vm, vx, vy); break;
    case 0x6: Op_6(vx, kk);     break;
    case 0x7: Op_7(vx, kk);     break;
    case 0x8: Op_8(vx, vy, vf, opcode >> 12); break;
    case 0x9: Op_9(vm, vx, vy); break;
    case 0xA: Op_A(vm, nnn);    break;
    case 0xB: Op_B(vm, nnn);    break;
    case 0xC: Op_C(vx, kk);     break;
    case 0xD: Op_D(vm, x, y, n); break;
    case 0xE: Op_E(vm, vx, opcode >> 12); break;
    case 0xF: Op_F(vm, vx, opcode >> 8);  break;
    default:
    }

    return EXEC_OK;
}
