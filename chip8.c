#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) ((a < b) ? a : b)

static u8 font_set[] = {
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

Chip8 *new(const u8 *prog, const long size)
{
    if (size > MAX_FREE_SZ) ERR("out of memory");

    Chip8 *vm = malloc(sizeof(Chip8));
    if (vm == NULL) ERR("vm malloc");

    memset(vm->ram, 0, RAM_SZ);
    memcpy(vm->ram, font_set, sizeof(font_set));
    memcpy(vm->ram + 0x200, prog, size);
    vm->_stack = (u16 *) &vm->ram[STACK_OFF];
    vm->screen = &vm->ram[SCREEN_OFF];
    vm->_size = (u16) size;
    vm->pc = 0x200;
    vm->sp = 0;
    vm->wait_key = 0;
    for (int i = 0; i < 16; i++) vm->key_state[i] = 0;

    return vm;
}

int get_pixel(Chip8 *vm, u8 x, u8 y)
{
    u8 byte_idx = (y * WIDTH + x) / 8;
    u8 bit_off  = (y * WIDTH + x) % 8;
    return (vm->screen[byte_idx] >> (7 - bit_off)) & 0x1;
}

static void set_pixel(Chip8 *vm, u8 x, u8 y, u8 on)
{
    u8 byte_idx = (y * WIDTH + x) / 8;
    u8 bit_off  = (y * WIDTH + x) % 8;
    vm->screen[byte_idx] = on ? (vm->screen[byte_idx] |  (0x80 >> bit_off))
                             : (vm->screen[byte_idx] & ~(0x80 >> bit_off));
}

static void op_0(Chip8 *vm, u16 opcode)
{
    switch (opcode) {
    case 0x00E0: {
        for (int i = 0; i < WIDTH*HEIGHT/8; i++) 
            vm->screen[i] = 0; 
    } break;
    case 0x00EE: vm->pc = vm->_stack[--vm->sp]; break;
    default:     vm->pc = opcode & 0xFFF;       break;
    }
}

static void op_1(Chip8 *vm, u16 nnn) { vm->pc = nnn; }
static void op_2(Chip8 *vm, u16 nnn) { vm->_stack[vm->sp++] = vm->pc; vm->pc = nnn; }
static void op_3(Chip8 *vm, u8 *vx, u8 kk) { if (*vx == kk) vm->pc += 2; }
static void op_4(Chip8 *vm, u8 *vx, u8 kk) { if (*vx != kk) vm->pc += 2; }
static void op_5(Chip8 *vm, u8 *vx, u8 *vy) { if (*vx == *vy) vm->pc += 2; }
static void op_6(u8 *vx, u8 kk) { *vx = kk; }
static void op_7(u8 *vx, u8 kk) { *vx += kk; }
static void op_8(u8 *vx, u8 *vy, u8 *vf, u8 suffix)
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
        *vx = *vy >> 1;
    } break;
    case 0x7: {
        *vf = (*vy < *vx) ? 0 : 1;  // borrow or not
        *vx = *vy - *vx;
    } break;
    case 0xE: {
        *vf = *vy & 0x80;
        *vx = *vy << 1;
    } break;
    default:
    }
}
static void op_9(Chip8 *vm, u8 *vx, u8 *vy) { if (*vx != *vy) vm->pc += 2; }
static void op_A(Chip8 *vm, u16 nnn) { vm->i = nnn; }
static void op_B(Chip8 *vm, u16 nnn) { vm->pc = nnn + vm->v[0]; }
static void op_C(u8 *vx, u8 kk) { *vx = (rand() % 0xFf) & kk; }
static void op_D(Chip8 *vm, u8 start_x, u8 start_y, u8 n)
{
    start_x %= WIDTH;
    start_y %= HEIGHT;
    int end_x = MIN(start_x + 8, WIDTH);
    int end_y = MIN(start_y + n, HEIGHT);

    vm->v[0xF] = 0;

    for (int y = start_y; y < end_y; y++) {
        u8 sprite_byte = vm->ram[vm->i];
        for (int x = start_x; x < end_x; x++) {
            u8 off = (x - start_x) % 8;
            u8 sprite_pixel = (sprite_byte >> (7 - off)) & 0x1;
            u8 screen_pixel = get_pixel(vm, x, y);

            if (sprite_pixel) {
                if (screen_pixel) vm->v[0xF] = 1;
                set_pixel(vm, x, y, screen_pixel == 0 ? 1 : 0);
            }
        }
    }
}
static void op_E(Chip8 *vm, u8 *vx, u8 suffix)
{
    u8 key_press = vm->key_state[*vx];
    switch (suffix) {
    case 0x1: vm->pc = (key_press == 1) ? vm->pc + 2 : vm->pc; break;
    case 0xE: vm->pc = (key_press == 0) ? vm->pc + 2 : vm->pc; break;
    default:
    }
    if (key_press == 1) vm->key_state[*vx] = 0;
}
static void op_F(Chip8 *vm, u8 *vx, u8 suffix)
{
    switch (suffix) {
    case 0x07: *vx = vm->dt; break;
    case 0x0A: vm->wait_key = 1; vm->wait_reg = vx - vm->v; break;
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

int exec(Chip8 *vm)
{
    if (vm->wait_key == 1) return EXEC_WAIT;
    if (vm->pc >= 0x200 + vm->_size) return EXEC_END;

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
    case 0x0: op_0(vm, opcode); break;
    case 0x1: op_1(vm, nnn);    break;
    case 0x2: op_2(vm, nnn);    break;
    case 0x3: op_3(vm, vx, kk); break;
    case 0x4: op_4(vm, vx, kk); break;
    case 0x5: op_5(vm, vx, vy); break;
    case 0x6: op_6(vx, kk);     break;
    case 0x7: op_7(vx, kk);     break;
    case 0x8: op_8(vx, vy, vf, opcode >> 12); break;
    case 0x9: op_9(vm, vx, vy); break;
    case 0xA: op_A(vm, nnn);    break;
    case 0xB: op_B(vm, nnn);    break;
    case 0xC: op_C(vx, kk);     break;
    case 0xD: op_D(vm, x, y, n); break;
    case 0xE: op_E(vm, vx, opcode >> 12); break;
    case 0xF: op_F(vm, vx, opcode >> 8);  break;
    default:
    }

    return EXEC_OK;
}
