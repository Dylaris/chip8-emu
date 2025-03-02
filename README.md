## Goal
- Make an assembler for Chip8 and use the assembler to translate test file into machine code.
- Make a Chip8 interpreter (emulator) to execute the machine code generated before.

## Spec
### Memory
- 4KB RAM (0x000 - 0xFFF)
- The beginning 512 bytes (0x200) is reserved for interpreter.

```
Memory Map:
+---------------+= 0xFFF (4095) End of Chip-8 RAM
|               |
| 0xF00 to 0xFFF|
|Display refresh|
|     region    |
|               |
+---------------+= 0xF00 Start of Chip8 Display refresh region
|               |
| 0xEA0 to 0xEFF|
|    Stack      |
|               |
+---------------+= 0xEA0 Start of Chip8 Stack
|               |
|               |
|               |
| 0x200 to 0xE9F|
|   Free for    |
|  program use  |
|               |
|               |
|               |
+---------------+= 0x200 (512) Start of most Chip-8 programs
| 0x000 to 0x1FF|
| Reserved for  |
|  interpreter  |
+---------------+= 0x000 (0) Start of Chip-8 RAM
```

### Register
- 16 8-bit registers V0-VF      -> general purpose
    - VF is also used as a flag (carry, borrow, collison)
- 1 16-bit address register I   -> store memory address (only the lowest 12 bits used)
- 2 8-bit registers dt(delay timer), st(sound timer)    -> decrement automatically at a rate of 60Hz when they are non-zero
- 16-bit PC     -> program counter (store next executing instruction)
- 8-bit SP      -> stack pointer (point to the topmost level of the stack)

### Stack
16 16-bit values, store the return address when finished a subroutine

### Input
chip8 uses 16-key hexadecimal keyboard:
|1|2|3|C|
|---|---|---|---|
|4|5|6|D|
|7|8|9|E|
|A|0|B|F|

### Display
chip8 uses 64x32-pixel monochrome display.

chip8 draws graphics on screen through the use of sprites (a group of bytes, 5 bytes long, 8x5).

```
font set:

0: 0xF0, 0x90, 0x90, 0x90, 0xF0
1: 0x20, 0x60, 0x20, 0x20, 0x70
2: 0xF0, 0x10, 0xF0, 0x80, 0xF0
3: 0xF0, 0x10, 0xF0, 0x10, 0xF0
4: 0x90, 0x90, 0xF0, 0x10, 0x10
5: 0xF0, 0x80, 0xF0, 0x10, 0xF0
6: 0xF0, 0x80, 0xF0, 0x90, 0xF0
7: 0xF0, 0x10, 0x20, 0x40, 0x40
8: 0xF0, 0x90, 0xF0, 0x90, 0xF0
9: 0xF0, 0x90, 0xF0, 0x10, 0xF0
A: 0xF0, 0x90, 0xF0, 0x90, 0x90
B: 0xE0, 0x90, 0xE0, 0x90, 0xE0
C: 0xF0, 0x80, 0x80, 0x80, 0xF0
D: 0xE0, 0x90, 0x90, 0x90, 0xE0
E: 0xF0, 0x80, 0xF0, 0x80, 0xF0
F: 0xF0, 0x80, 0xF0, 0x80, 0x80

```

### Instruction
35 instructions and every one has 2-byte long

In Chip8 instruction, we use some specifial bit-field to represent some object:

- nnn or addr - A 12-bit value, the lowest 12 bits of the instruction
- n or nibble - A 4-bit value, the lowest 4 bits of the instruction
- x - A 4-bit value, the lower 4 bits of the high byte of the instruction
- y - A 4-bit value, the upper 4 bits of the low byte of the instruction
- kk or byte - An 8-bit value, the lowest 8 bits of the instruction

| Instruction | Pseudo-Code |
| --- | --- |
| 0NNN | |
| 00E0 | clear screen -> set all pixel to 0 |
| 00EE | PC = Stack[SP--] |
| 1NNN | PC = NNN |
| 2NNN | Stack[SP++] = PC; PC = NNN |
| 3XNN | if VX == KK then PC += 2 |
| 4XNN | if VX != KK then PC += 2 |
| 5XY0 | if VX == VY then PC += 2 |
| 6XNN | VX = KK |
| 7XNN | VX += KK |
| 8XY0 | VX = VY |
| 8XY1 | VX \|= VY |
| 8XY2 | VX &= VY |
| 8XY3 | VX ^= VY |
| 8XY4 | VX += VY; <br>if carry then VF = 1 else VF = 0 |
| 8XY5 | VX -= VY; <br>if borrow then VF = 0 else VF = 1 |
| 8XY6 | VF = VY & 0x01; VY >>= 1; VX = VY |
| 8XY7 | VX = VY - VX; <br>if borrow then VF = 0 else VF = 1 |
| 8XYE | VF = VY & 0x80; VY <<= 1; VX = VY |
| 9XY0 | if VX != VY then PC += 2 |
| ANNN | I = NNN |
| BNNN | PC = NNN + V0 |
| CXNN | VX = (rand() % 0xFF) & KK |
| DXYN | if collison then VF = 1 else VF = 0 |
| EX9E | Key = VX; if Press(Key) then PC += 2 |
| EXA1 | Key = VX; if !Press(Key) then PC += 2 |
| FX07 | VX = DT |
| FX0A | while !AnyKeyPress; VX = Key |
| FX15 | DT = VX |
| FX18 | ST = VX |
| FX1E | I += VX |
| FX29 | I = FONT_BASE + VX * 5 |
| FX33 | [I] = VX >> 8; <br>[I + 1] = VX >> 4; <br>[I + 2] = VX >> 0 |
| FX55 | [I] = V0; <br>[I + 1] = V1; <br>... <br>[I + X] = VX; <br>I = I + X + 1 |
| FX65 | V0 = [I]; <br>V1 = [I + 1]; <br>... <br>VX = [I + X]; <br>I = I + X + 1 |