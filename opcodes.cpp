#include <iostream>

#include "gameboy.h"

u8 op_0x21_LD_HL_u16(Gameboy& gb)
{
    // set HL to the 16-bit value following the opcode
    gb.HL = gb.read16(gb.PC + 1);

    gb.PC += 3;
    return 12;
}

u8 op_0x31_LD_SP_u16(Gameboy& gb)
{
    // set SP to the 16-bit value following the opcode
    gb.SP = gb.read16(gb.PC + 1);

    gb.PC += 3;
    return 12;
}

u8 op_0x32_LD_HLm_A(Gameboy& gb)
{
    // store A into memory at address HL, then decrement HL
    gb.write8(gb.HL, gb.AF_bytes.A);
    gb.HL--;

    gb.PC += 1;
    return 8;
}

u8 op_0xAF_XOR_A_A(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.AF_bytes.A; // result is always 0
    gb.AF_bytes.F = FLAG_Z; // Z flag set (bit 7), others cleared

    gb.PC += 1;
    return 4;
}

u8 op_0xCB_prefixed(Gameboy& gb)
{
    // get next byte to determine specific CB opcode
    u8 cb = gb.read8(gb.PC + 1);
    return gb.cb_opcodes[cb](gb);
}

u8 op_0xCB_0x7C_BIT_7_H(Gameboy& gb)
{
    // don't modify C flag, set H flag, clear N flag, set Z flag if bit 7 of H is 0, else clear
    gb.AF_bytes.F = (gb.AF_bytes.F & FLAG_C) | FLAG_H | ((gb.HL_bytes.H & 0x80) == 0 ? FLAG_Z : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0x20_JR_NZ_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1)); // read signed 8-bit offset

    // move to next instruction first, because offset is relative from there
    gb.PC += 2;

    if ((gb.AF_bytes.F & FLAG_Z) == 0) // Z flag not set
    {
        gb.PC += offset; // now apply relative jump
        return 12;
    }

    return 8;
}

u8 op_0x0E_LD_C_u8(Gameboy& gb)
{
    gb.BC_bytes.C = gb.read8(gb.PC + 1); // load 8-bit value into C register

    gb.PC += 2;
    return 8;
}

u8 op_0x3E_LD_A_u8(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(gb.PC + 1); // load 8-bit value into A register

    gb.PC += 2;
    return 8;
}

u8 op_0xE2_LD_C_A(Gameboy& gb)
{
    gb.write8(0xFF00 + gb.BC_bytes.C, gb.AF_bytes.A); // write A to address (0xFF00 + C)

    gb.PC += 1;
    return 8;
}

u8 op_0x0C_INC_C(Gameboy& gb)
{
    gb.BC_bytes.C += 1; // increment C

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.BC_bytes.C == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.BC_bytes.C & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0x77_LD_HL_A(Gameboy& gb)
{
    gb.write8(gb.HL, gb.AF_bytes.A); // write A to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0xE0_LD_u8_A(Gameboy& gb)
{
    // write A to address (0xFF00 + u8)
    gb.write8(0xFF00 + gb.read8(gb.PC + 1), gb.AF_bytes.A);

    gb.PC += 2;
    return 12;
}

u8 op_0x11_LD_DE_u16(Gameboy& gb)
{
    gb.DE = gb.read16(gb.PC + 1); // set DE to the 16-bit value following the opcode

    gb.PC += 3;
    return 12;
}

u8 op_0x1A_LD_A_DE(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(gb.DE); // load A from memory at address DE

    gb.PC += 1;
    return 8;
}

u8 op_0xCD_CALL_u16(Gameboy& gb)
{
    uint16_t addr = gb.read16(gb.PC + 1); // get 16-bit address to call

    // push address of next instruction (after CALL) onto stack
    // current CALL instruction has length 3 bytes (opcode + 16-bit address)
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 3);

    gb.PC = addr; // jump to called address
    return 24;
}

u8 op_0x4F_LD_C_A(Gameboy& gb)
{
    gb.BC_bytes.C = gb.AF_bytes.A; // copy A into C

    gb.PC += 1;
    return 4;
}

u8 op_0x06_LD_B_u8(Gameboy& gb)
{
    gb.BC_bytes.B = gb.read8(gb.PC + 1); // load 8-bit value into B register

    gb.PC += 2;
    return 8;
}

u8 op_0xC5_PUSH_BC(Gameboy& gb)
{
    gb.SP -= 2; // decrement stack pointer by 2
    gb.write16(gb.SP, gb.BC); // write BC to memory at address SP

    gb.PC += 1;
    return 16;
}

u8 op_0xCB_0x11_RL_C(Gameboy& gb)
{
    u8 old_c = gb.BC_bytes.C;

    // rotate left through carry
    gb.BC_bytes.C = (old_c << 1) | ((gb.AF_bytes.F & FLAG_C) ? 1 : 0);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((gb.BC_bytes.C == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= ((old_c & 0x80) >> 7) * FLAG_C; // C flag if old bit 7 was set

    gb.PC += 2;
    return 8;
}

u8 op_0x17_RLA(Gameboy& gb)
{
    u8 old_a = gb.AF_bytes.A;
    u8 carry_in = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;

    // rotate left through carry
    gb.AF_bytes.A = (old_a << 1) | carry_in;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((old_a & 0x80) >> 7) * FLAG_C; // C flag if old bit 7 was set

    gb.PC += 1;
    return 4;
}

u8 op_0xC1_POP_BC(Gameboy& gb)
{
    gb.BC = gb.read16(gb.SP); // read 16-bit value from memory at address SP into BC
    gb.SP += 2; // increment stack pointer by 2

    gb.PC += 1;
    return 12;
}

u8 op_0x05_DEC_B(Gameboy& gb)
{
    gb.BC_bytes.B -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag (bit 6)
    gb.AF_bytes.F |= ((gb.BC_bytes.B == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.BC_bytes.B & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x22_LD_HLp_A(Gameboy& gb)
{
    gb.write8(gb.HL++, gb.AF_bytes.A); // store A into memory at address HL, then increment HL

    gb.PC += 1;
    return 8;
}

u8 op_0x23_INC_HL(Gameboy& gb)
{
    gb.HL += 1; // increment HL

    gb.PC += 1;
    return 8;
}

u8 op_0xC9_RET(Gameboy& gb)
{
    gb.PC = gb.read16(gb.SP); // pop return address from stack into PC
    gb.SP += 2; // increment stack pointer by 2

    return 16;
}

u8 op_0x13_INC_DE(Gameboy& gb)
{
    gb.DE += 1; // increment DE

    gb.PC += 1;
    return 8;
}

u8 op_0x7B_LD_A_E(Gameboy& gb)
{
    gb.AF_bytes.A = gb.DE_bytes.E; // copy E into A

    gb.PC += 1;
    return 4;
}

u8 op_0xFE_CP_A_u8(Gameboy& gb)
{
    u8 value = gb.read8(gb.PC + 1);
    u8 result = gb.AF_bytes.A - value;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H); // H flag if borrow from bit 4
    gb.AF_bytes.F |= ((gb.AF_bytes.A < value) * FLAG_C); // C flag if borrow (A < value)

    gb.PC += 2;
    return 8;
}

u8 op_0xEA_LD_u16_A(Gameboy& gb)
{
    uint16_t addr = gb.read16(gb.PC + 1);
    gb.write8(addr, gb.AF_bytes.A); // write A to address

    gb.PC += 3;
    return 16;
}

u8 op_0x3D_DEC_A(Gameboy& gb)
{
    gb.AF_bytes.A -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x28_JR_Z_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1)); // read signed 8-bit offset

    gb.PC += 2; // move to next instruction first, offset is relative from there

    if (gb.AF_bytes.F & FLAG_Z) // if Z flag set
    {
        gb.PC += offset;
        return 12;
    }

    return 8;
}

u8 op_0x0D_DEC_C(Gameboy& gb)
{
    gb.BC_bytes.C -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag
    gb.AF_bytes.F |= ((gb.BC_bytes.C == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.BC_bytes.C & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x2E_LD_L_u8(Gameboy& gb)
{
    gb.HL_bytes.L = gb.read8(gb.PC + 1); // load 8-bit value into L register

    gb.PC += 2;
    return 8;
}

u8 op_0x18_JR_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1)); // read signed 8-bit offset

    gb.PC += 2; // move to next instruction first, offset is relative from there
    gb.PC += offset;

    return 12;
}

u8 op_0x67_LD_H_A(Gameboy& gb)
{
    gb.HL_bytes.H = gb.AF_bytes.A; // copy A into H

    gb.PC += 1;
    return 4;
}

u8 op_0x57_LD_D_A(Gameboy& gb)
{
    gb.DE_bytes.D = gb.AF_bytes.A; // copy A into D

    gb.PC += 1;
    return 4;
}

u8 op_0x04_INC_B(Gameboy& gb)
{
    gb.BC_bytes.B += 1; // increment B

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.BC_bytes.B == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.BC_bytes.B & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0x1E_LD_E_u8(Gameboy& gb)
{
    gb.DE_bytes.E = gb.read8(gb.PC + 1); // load 8-bit value into E register

    gb.PC += 2;
    return 8;
}

u8 op_0xF0_LD_A_FF00_u8(Gameboy& gb)
{
    // load A from address (0xFF00 + u8)
    gb.AF_bytes.A = gb.read8(0xFF00 + gb.read8(gb.PC + 1));

    gb.PC += 2;
    return 12;
}

u8 op_0x1D_DEC_E(Gameboy& gb)
{
    gb.DE_bytes.E -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag (bit 6)
    gb.AF_bytes.F |= ((gb.DE_bytes.E == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.DE_bytes.E & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x24_INC_H(Gameboy& gb)
{
    gb.HL_bytes.H += 1; // increment H

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.HL_bytes.H == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.HL_bytes.H & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0x7C_LD_A_H(Gameboy& gb)
{
    gb.AF_bytes.A = gb.HL_bytes.H; // copy H into A

    gb.PC += 1;
    return 4;
}

u8 op_0x90_SUB_A_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 result = gb.AF_bytes.A - value;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H); // H flag if borrow from bit 4
    gb.AF_bytes.F |= ((gb.AF_bytes.A < value) * FLAG_C); // C flag if borrow (A < value)

    gb.AF_bytes.A = result; // store result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x15_DEC_D(Gameboy& gb)
{
    gb.DE_bytes.D -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag (bit 6)
    gb.AF_bytes.F |= ((gb.DE_bytes.D == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.DE_bytes.D & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x16_LD_D_u8(Gameboy& gb)
{
    gb.DE_bytes.D = gb.read8(gb.PC + 1); // load 8-bit value into D register

    gb.PC += 2;
    return 8;
}

u8 op_0xBE_CP_A_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 result = gb.AF_bytes.A - value;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H); // H flag if borrow from bit 4
    gb.AF_bytes.F |= ((gb.AF_bytes.A < value) * FLAG_C); // C flag if borrow (A < value)

    gb.PC += 1;
    return 8;
}

u8 op_0x7D_LD_A_L(Gameboy& gb)
{
    gb.AF_bytes.A = gb.HL_bytes.L; // copy L into A

    gb.PC += 1;
    return 4;
}

u8 op_0x78_LD_A_B(Gameboy& gb)
{
    gb.AF_bytes.A = gb.BC_bytes.B; // copy B into A

    gb.PC += 1;
    return 4;
}

u8 op_0x86_ADD_A_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) + static_cast<uint16_t>(value);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) + (value & 0x0F)) > 0x0F) * FLAG_H; // H flag if carry from bit 4
    gb.AF_bytes.F |= ((result > 0xFF) * FLAG_C); // C flag if carry (result > 255)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 8;
}

u8 op_0x00_NOP(Gameboy& gb)
{
    gb.PC += 1; // simply advance PC by 1
    return 4;
}

u8 op_0xC3_JP_u16(Gameboy& gb)
{
    uint16_t addr = gb.read16(gb.PC + 1); // get 16-bit address to jump to
    gb.PC = addr; // jump to address
    return 16;
}

u8 op_0xF3_DI(Gameboy& gb)
{
    gb.ime = false; // disable interrupts
    gb.ime_scheduled = false;

    gb.PC += 1;
    return 4;
}

u8 op_0x36_LD_HL_u8(Gameboy& gb)
{
    // write 8-bit value into memory at address HL
    gb.write8(gb.HL, gb.read8(gb.PC + 1));

    gb.PC += 2;
    return 12;
}

u8 op_0x2A_LD_A_HLp(Gameboy& gb)
{
    // load A from memory at address HL, then increment HL
    gb.AF_bytes.A = gb.read8(gb.HL++);

    gb.PC += 1;
    return 8;
}

u8 op_0x01_LD_BC_u16(Gameboy& gb)
{
    // set BC to the 16-bit value following the opcode
    gb.BC = gb.read16(gb.PC + 1);

    gb.PC += 3;
    return 12;
}

u8 op_0x0B_DEC_BC(Gameboy& gb)
{
    gb.BC -= 1; // decrement BC

    gb.PC += 1;
    return 8;
}

u8 op_0xB1_OR_A_C(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.BC_bytes.C; // do the OR
    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xFB_EI(Gameboy& gb)
{
    gb.ime_scheduled = true; // enable interrupts after next instruction

    gb.PC += 1;
    return 4;
}

u8 op_0x2F_CPL(Gameboy& gb)
{
    gb.AF_bytes.A = ~gb.AF_bytes.A; // bitwise NOT on A
    gb.AF_bytes.F |= FLAG_N | FLAG_H; // set N and H flags, preserve others

    gb.PC += 1;
    return 4;
}

u8 op_0xE6_AND_A_u8(Gameboy& gb)
{
    u8 value = gb.read8(gb.PC + 1);

    gb.AF_bytes.A &= value; // do the AND
    gb.AF_bytes.F = FLAG_H; // set H flag, clear others
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x37_SWAP_A(Gameboy& gb)
{
    u8 old_a = gb.AF_bytes.A;

    gb.AF_bytes.A = (old_a << 4) | (old_a >> 4); // swap upper and lower nibbles
    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0x47_LD_B_A(Gameboy& gb)
{
    gb.BC_bytes.B = gb.AF_bytes.A; // copy A into B

    gb.PC += 1;
    return 4;
}

u8 op_0xB0_OR_A_B(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.BC_bytes.B; // do the OR
    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xA9_XOR_A_C(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.BC_bytes.C; // do the XOR
    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xA1_AND_A_C(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.BC_bytes.C; // do the AND
    gb.AF_bytes.F = FLAG_H; // set H flag, clear others
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0x79_LD_A_C(Gameboy& gb)
{
    gb.AF_bytes.A = gb.BC_bytes.C; // copy C into A

    gb.PC += 1;
    return 4;
}

u8 op_0xEF_RST_28h(Gameboy& gb)
{
    // push address of next instruction (PC + 1, after RST) onto stack
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1);

    gb.PC = 0x28; // jump to address 0x28
    return 16;
}

u8 op_0x87_ADD_A_A(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u16 result = a << 1;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | ((a & 0x0F) > 0x07) * FLAG_H | (a > 0x7F) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0xE1_POP_HL(Gameboy& gb)
{
    gb.HL = gb.read16(gb.SP); // read 16-bit value from memory at address SP into HL
    gb.SP += 2; // increment stack pointer by 2

    gb.PC += 1;
    return 12;
}

u8 op_0x5F_LD_E_A(Gameboy& gb)
{
    gb.DE_bytes.E = gb.AF_bytes.A; // copy A into E

    gb.PC += 1;
    return 4;
}

u8 op_0x19_ADD_HL_DE(Gameboy& gb)
{
    uint32_t result = static_cast<uint32_t>(gb.HL) + static_cast<uint32_t>(gb.DE);

    gb.AF_bytes.F &= FLAG_Z; // preserve Z flag, clear others
    gb.AF_bytes.F |= (((gb.HL & 0x0FFF) + (gb.DE & 0x0FFF)) > 0x0FFF) * FLAG_H; // H flag if carry from bit 12
    gb.AF_bytes.F |= ((result > 0xFFFF) * FLAG_C); // C flag if carry (result > 65535)

    gb.HL = static_cast<uint16_t>(result & 0xFFFF); // store low 16 bits of result in HL

    gb.PC += 1;
    return 8;
}

u8 op_0x5E_LD_E_HL(Gameboy& gb)
{
    gb.DE_bytes.E = gb.read8(gb.HL); // load E from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x56_LD_D_HL(Gameboy& gb)
{
    gb.DE_bytes.D = gb.read8(gb.HL); // load D from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0xD5_PUSH_DE(Gameboy& gb)
{
    gb.SP -= 2; // decrement stack pointer by 2
    gb.write16(gb.SP, gb.DE); // write DE to memory at address SP

    gb.PC += 1;
    return 16;
}

u8 op_0xE9_JP_HL(Gameboy& gb)
{
    gb.PC = gb.HL; // jump to address in HL
    return 4;
}

u8 op_0xCB_0x87_RES_0_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 0); // reset bit 0 of A

    gb.PC += 2;
    return 8;
}

u8 op_0x12_LD_DE_A(Gameboy& gb)
{
    gb.write8(gb.DE, gb.AF_bytes.A); // write A to address DE

    gb.PC += 1;
    return 8;
}

u8 op_0xE5_PUSH_HL(Gameboy& gb)
{
    gb.SP -= 2; // decrement stack pointer by 2
    gb.write16(gb.SP, gb.HL); // write HL to memory at address SP

    gb.PC += 1;
    return 16;
}

u8 op_0xD1_POP_DE(Gameboy& gb)
{
    gb.DE = gb.read16(gb.SP); // read 16-bit value from memory at address SP into DE
    gb.SP += 2; // increment stack pointer by 2

    gb.PC += 1;
    return 12;
}

u8 op_0xF5_PUSH_AF(Gameboy& gb)
{
    gb.SP -= 2; // decrement stack pointer by 2
    gb.write16(gb.SP, gb.AF); // write AF to memory at address SP

    gb.PC += 1;
    return 16;
}

u8 op_0xFA_LD_A_u16(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(gb.read16(gb.PC + 1)); // load A from address

    gb.PC += 3;
    return 16;
}

u8 op_0xA7_AND_A_A(Gameboy& gb)
{
    // this technically would do A &= A, but that's a no-op, so just set flags
    gb.AF_bytes.F = FLAG_H; // set H flag, clear others
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0x1C_INC_E(Gameboy& gb)
{
    gb.DE_bytes.E += 1; // increment E

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.DE_bytes.E == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.DE_bytes.E & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0xCA_JP_Z_u16(Gameboy& gb)
{
    uint16_t addr = gb.read16(gb.PC + 1); // get 16-bit address to jump to

    if (gb.AF_bytes.F & FLAG_Z) // if Z flag set
    {
        gb.PC = addr; // jump to address
        return 16;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xC8_RET_Z(Gameboy& gb)
{
    if (gb.AF_bytes.F & FLAG_Z) // if Z flag set
    {
        gb.PC = gb.read16(gb.SP); // pop return address from stack into PC
        gb.SP += 2; // increment stack pointer by 2
        return 20;
    }

    gb.PC += 1;
    return 8;
}

u8 op_0x7E_LD_A_HL(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(gb.HL); // load A from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0xF1_POP_AF(Gameboy& gb)
{
    gb.AF = gb.read16(gb.SP); // read 16-bit value from memory at address SP into AF
    gb.SP += 2; // increment stack pointer by 2
    gb.AF_bytes.F &= 0xF0; // ensure lower nibble of F is always zero

    gb.PC += 1;
    return 12;
}

u8 op_0xC0_RET_NZ(Gameboy& gb)
{
    if (!(gb.AF_bytes.F & FLAG_Z)) // if Z flag not set
    {
        gb.PC = gb.read16(gb.SP); // pop return address from stack into PC
        gb.SP += 2; // increment stack pointer by 2
        return 20;
    }

    gb.PC += 1;
    return 8;
}

u8 op_0x34_INC_HL(Gameboy& gb)
{
    u8 old_value = gb.read8(gb.HL);
    u8 result = old_value + 1;
    gb.write8(gb.HL, result);

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((result == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((old_value & 0x0F) + 1) > 0x0F) * FLAG_H; // H flag if carry from bit 4

    gb.PC += 1;
    return 12;
}

u8 op_0x3C_INC_A(Gameboy& gb)
{
    gb.AF_bytes.A += 1; // increment A

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0xD9_RETI(Gameboy& gb)
{
    gb.PC = gb.read16(gb.SP); // pop return address from stack into PC
    gb.SP += 2; // increment stack pointer by 2
    gb.ime = true; // enable interrupts

    return 16;
}

u8 op_0x02_LD_BC_A(Gameboy& gb)
{
    gb.write8(gb.BC, gb.AF_bytes.A); // write A to address BC

    gb.PC += 1;
    return 8;
}

u8 op_0x03_INC_BC(Gameboy& gb)
{
    gb.BC += 1; // increment BC

    gb.PC += 1;
    return 8;
}

u8 op_0x07_RLCA(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 carry = a >> 7; // Extract bit 7 once

    gb.AF_bytes.A = (a << 1) | carry;
    gb.AF_bytes.F = carry * FLAG_C;

    gb.PC += 1;
    return 4;
}

u8 op_0x08_LD_u16_SP(Gameboy& gb)
{
    gb.write16(gb.read16(gb.PC + 1), gb.SP); // write SP to address

    gb.PC += 3;
    return 20;
}

u8 op_0x09_ADD_HL_BC(Gameboy& gb)
{
    uint32_t result = static_cast<uint32_t>(gb.HL) + static_cast<uint32_t>(gb.BC);

    gb.AF_bytes.F &= FLAG_Z; // preserve Z flag, clear others
    gb.AF_bytes.F |= (((gb.HL & 0x0FFF) + (gb.BC & 0x0FFF)) > 0x0FFF) * FLAG_H; // H flag if carry from bit 12
    gb.AF_bytes.F |= ((result > 0xFFFF) * FLAG_C); // C flag if carry (result > 65535)

    gb.HL = static_cast<uint16_t>(result & 0xFFFF); // store low 16 bits of result in HL

    gb.PC += 1;
    return 8;
}

u8 op_0x0A_LD_A_BC(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(gb.BC); // load A from memory at address BC

    gb.PC += 1;
    return 8;
}

u8 op_0x0F_RRCA(Gameboy& gb)
{
    u8 old_a = gb.AF_bytes.A;
    gb.AF_bytes.A = (old_a >> 1) | ((old_a & 0x01) << 7); // rotate right, old bit 0 to bit 7

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (old_a & 0x01) * FLAG_C; // C flag if old bit 0 was set

    gb.PC += 1;
    return 4;
}

u8 op_0x14_INC_D(Gameboy& gb)
{
    gb.DE_bytes.D += 1; // increment D

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.DE_bytes.D == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.DE_bytes.D & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0x1B_DEC_DE(Gameboy& gb)
{
    gb.DE -= 1; // decrement DE

    gb.PC += 1;
    return 8;
}

u8 op_0x1F_RRA(Gameboy& gb)
{
    u8 old_a = gb.AF_bytes.A;
    u8 carry_in = (gb.AF_bytes.F & FLAG_C) ? 0x80 : 0;

    gb.AF_bytes.A = (old_a >> 1) | carry_in; // rotate right through carry

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (old_a & 0x01) * FLAG_C; // C flag if old bit 0 was set

    gb.PC += 1;
    return 4;
}
u8 op_0x25_DEC_H(Gameboy& gb)
{
    gb.HL_bytes.H -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag
    gb.AF_bytes.F |= ((gb.HL_bytes.H == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.HL_bytes.H & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x26_LD_H_u8(Gameboy& gb)
{
    gb.HL_bytes.H = gb.read8(gb.PC + 1); // load 8-bit value into H register

    gb.PC += 2;
    return 8;
}

u8 op_0x27_DAA(Gameboy& gb)
{
    // after an addition, adjust if (half-)carry occurred or if result is out of bounds
    if (!(gb.AF_bytes.F & FLAG_N)) {
        if ((gb.AF_bytes.F & FLAG_C) || (gb.AF_bytes.A > 0x99)) {
            gb.AF_bytes.A += 0x60;
            gb.AF_bytes.F |= FLAG_C;
        }
        if ((gb.AF_bytes.F & FLAG_H) || ((gb.AF_bytes.A & 0x0F) > 0x09)) {
            gb.AF_bytes.A += 0x06;
        }
    } else // after a subtraction, only adjust if (half-)carry occurred
    {
        if (gb.AF_bytes.F & FLAG_C) {
            gb.AF_bytes.A -= 0x60;
        }
        if (gb.AF_bytes.F & FLAG_H) {
            gb.AF_bytes.A -= 0x06;
        }
    }

    // these flags are always updated
    gb.AF_bytes.F &= ~FLAG_Z; // clear Z flag
    gb.AF_bytes.F |= ((gb.AF_bytes.A == 0) * FLAG_Z); // set Z flag if result is 0
    gb.AF_bytes.F &= ~FLAG_H; // clear H flag

    gb.PC += 1;
    return 4;
}

u8 op_0x29_ADD_HL_HL(Gameboy& gb)
{
    uint32_t result = static_cast<uint32_t>(gb.HL) + static_cast<uint32_t>(gb.HL);

    gb.AF_bytes.F &= FLAG_Z; // preserve Z flag, clear others
    gb.AF_bytes.F |= (((gb.HL & 0x0FFF) * 2) > 0x0FFF) * FLAG_H; // H flag if carry from bit 12
    gb.AF_bytes.F |= ((result > 0xFFFF) * FLAG_C); // C flag if carry (result > 65535)

    gb.HL = static_cast<uint16_t>(result & 0xFFFF); // store low 16 bits of result in HL

    gb.PC += 1;
    return 8;
}

u8 op_0x2B_DEC_HL(Gameboy& gb)
{
    gb.HL -= 1; // decrement HL

    gb.PC += 1;
    return 8;
}

u8 op_0x2C_INC_L(Gameboy& gb)
{
    gb.HL_bytes.L += 1; // increment L

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= ((gb.HL_bytes.L == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.HL_bytes.L & 0x0F) == 0) * FLAG_H); // H flag if low nibble overflowed

    gb.PC += 1;
    return 4;
}

u8 op_0x2D_DEC_L(Gameboy& gb)
{
    gb.HL_bytes.L -= 1;

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag (bit 6)
    gb.AF_bytes.F |= ((gb.HL_bytes.L == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.HL_bytes.L & 0x0F) == 0x0F) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 4;
}

u8 op_0x33_INC_SP(Gameboy& gb)
{
    gb.SP += 1; // increment SP

    gb.PC += 1;
    return 8;
}

u8 op_0x35_DEC_HL(Gameboy& gb)
{
    u8 old_value = gb.read8(gb.HL);
    u8 result = old_value - 1;
    gb.write8(gb.HL, result);

    gb.AF_bytes.F &= FLAG_C; // preserve C flag, clear others
    gb.AF_bytes.F |= FLAG_N; // set N flag
    gb.AF_bytes.F |= ((result == 0) * FLAG_Z); // Z flag if result is 0
    gb.AF_bytes.F |= (((old_value & 0x0F) == 0x00) * FLAG_H); // H flag if borrow from bit 4

    gb.PC += 1;
    return 12;
}

u8 op_0x30_JR_NC_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1)); // get signed 8-bit offset

    if (!(gb.AF_bytes.F & FLAG_C)) // if C flag not set
    {
        gb.PC += 2 + offset; // jump to PC + 2 + offset
        return 12;
    }

    gb.PC += 2;
    return 8;
}

u8 op_0x37_SCF(Gameboy& gb)
{
    gb.AF_bytes.F &= ~(FLAG_N | FLAG_H); // clear N and H flags
    gb.AF_bytes.F |= FLAG_C; // set C flag

    gb.PC += 1;
    return 4;
}

u8 op_0x38_JR_C_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1)); // get signed 8-bit offset

    if (gb.AF_bytes.F & FLAG_C) // if C flag set
    {
        gb.PC += 2 + offset; // jump to PC + 2 + offset
        return 12;
    }

    gb.PC += 2;
    return 8;
}

u8 op_0x39_ADD_HL_SP(Gameboy& gb)
{
    uint32_t result = static_cast<uint32_t>(gb.HL) + static_cast<uint32_t>(gb.SP);

    gb.AF_bytes.F &= FLAG_Z; // preserve Z flag, clear others
    gb.AF_bytes.F |= (((gb.HL & 0x0FFF) + (gb.SP & 0x0FFF)) > 0x0FFF) * FLAG_H; // H flag if carry from bit 12
    gb.AF_bytes.F |= ((result > 0xFFFF) * FLAG_C); // C flag if carry (result > 65535)

    gb.HL = static_cast<uint16_t>(result & 0xFFFF); // store low 16 bits of result in HL

    gb.PC += 1;
    return 8;
}

u8 op_0x3A_LD_A_HLm(Gameboy& gb)
{
    // load A from memory at address HL, then decrement HL
    gb.AF_bytes.A = gb.read8(gb.HL--);

    gb.PC += 1;
    return 8;
}

u8 op_0x3B_DEC_SP(Gameboy& gb)
{
    gb.SP -= 1; // decrement SP

    gb.PC += 1;
    return 8;
}

u8 op_0x3F_CCF(Gameboy& gb)
{
    gb.AF_bytes.F &= ~(FLAG_N | FLAG_H); // clear N and H flags
    gb.AF_bytes.F ^= FLAG_C; // toggle C flag

    gb.PC += 1;
    return 4;
}

u8 op_0x40_LD_B_B(Gameboy& gb)
{
    // no-op, just copying B to B
    gb.PC += 1;
    return 4;
}

u8 op_0x41_LD_B_C(Gameboy& gb)
{
    gb.BC_bytes.B = gb.BC_bytes.C; // copy C into B

    gb.PC += 1;
    return 4;
}

u8 op_0x42_LD_B_D(Gameboy& gb)
{
    gb.BC_bytes.B = gb.DE_bytes.D; // copy D into B

    gb.PC += 1;
    return 4;
}

u8 op_0x43_LD_B_E(Gameboy& gb)
{
    gb.BC_bytes.B = gb.DE_bytes.E; // copy E into B

    gb.PC += 1;
    return 4;
}

u8 op_0x44_LD_B_H(Gameboy& gb)
{
    gb.BC_bytes.B = gb.HL_bytes.H; // copy H into B

    gb.PC += 1;
    return 4;
}

u8 op_0x45_LD_B_L(Gameboy& gb)
{
    gb.BC_bytes.B = gb.HL_bytes.L; // copy L into B

    gb.PC += 1;
    return 4;
}

u8 op_0x46_LD_B_HL(Gameboy& gb)
{
    gb.BC_bytes.B = gb.read8(gb.HL); // load B from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x48_LD_C_B(Gameboy& gb)
{
    gb.BC_bytes.C = gb.BC_bytes.B; // copy B into C

    gb.PC += 1;
    return 4;
}

u8 op_0x49_LD_C_C(Gameboy& gb)
{
    // no-op, just copying C to C
    gb.PC += 1;
    return 4;
}

u8 op_0x4A_LD_C_D(Gameboy& gb)
{
    gb.BC_bytes.C = gb.DE_bytes.D; // copy D into C

    gb.PC += 1;
    return 4;
}

u8 op_0x4B_LD_C_E(Gameboy& gb)
{
    gb.BC_bytes.C = gb.DE_bytes.E; // copy E into C

    gb.PC += 1;
    return 4;
}

u8 op_0x4C_LD_C_H(Gameboy& gb)
{
    gb.BC_bytes.C = gb.HL_bytes.H; // copy H into C

    gb.PC += 1;
    return 4;
}

u8 op_0x4D_LD_C_L(Gameboy& gb)
{
    gb.BC_bytes.C = gb.HL_bytes.L; // copy L into C

    gb.PC += 1;
    return 4;
}

u8 op_0x4E_LD_C_HL(Gameboy& gb)
{
    gb.BC_bytes.C = gb.read8(gb.HL); // load C from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x50_LD_D_B(Gameboy& gb)
{
    gb.DE_bytes.D = gb.BC_bytes.B; // copy B into D

    gb.PC += 1;
    return 4;
}

u8 op_0x51_LD_D_C(Gameboy& gb)
{
    gb.DE_bytes.D = gb.BC_bytes.C; // copy C into D

    gb.PC += 1;
    return 4;
}

u8 op_0x52_LD_D_D(Gameboy& gb)
{
    // no-op, just copying D to D
    gb.PC += 1;
    return 4;
}

u8 op_0x53_LD_D_E(Gameboy& gb)
{
    gb.DE_bytes.D = gb.DE_bytes.E; // copy E into D

    gb.PC += 1;
    return 4;
}

u8 op_0x54_LD_D_H(Gameboy& gb)
{
    gb.DE_bytes.D = gb.HL_bytes.H; // copy H into D

    gb.PC += 1;
    return 4;
}

u8 op_0x55_LD_D_L(Gameboy& gb)
{
    gb.DE_bytes.D = gb.HL_bytes.L; // copy L into D

    gb.PC += 1;
    return 4;
}

u8 op_0x58_LD_E_B(Gameboy& gb)
{
    gb.DE_bytes.E = gb.BC_bytes.B; // copy B into E

    gb.PC += 1;
    return 4;
}

u8 op_0x59_LD_E_C(Gameboy& gb)
{
    gb.DE_bytes.E = gb.BC_bytes.C; // copy C into E

    gb.PC += 1;
    return 4;
}

u8 op_0x5A_LD_E_D(Gameboy& gb)
{
    gb.DE_bytes.E = gb.DE_bytes.D; // copy D into E

    gb.PC += 1;
    return 4;
}

u8 op_0x5B_LD_E_E(Gameboy& gb)
{
    // no-op, just copying E to E
    gb.PC += 1;
    return 4;
}

u8 op_0x5C_LD_E_H(Gameboy& gb)
{
    gb.DE_bytes.E = gb.HL_bytes.H; // copy H into E

    gb.PC += 1;
    return 4;
}

u8 op_0x5D_LD_E_L(Gameboy& gb)
{
    gb.DE_bytes.E = gb.HL_bytes.L; // copy L into E

    gb.PC += 1;
    return 4;
}

u8 op_0x60_LD_H_B(Gameboy& gb)
{
    gb.HL_bytes.H = gb.BC_bytes.B; // copy B into H

    gb.PC += 1;
    return 4;
}

u8 op_0x61_LD_H_C(Gameboy& gb)
{
    gb.HL_bytes.H = gb.BC_bytes.C; // copy C into H

    gb.PC += 1;
    return 4;
}

u8 op_0x62_LD_H_D(Gameboy& gb)
{
    gb.HL_bytes.H = gb.DE_bytes.D; // copy D into H

    gb.PC += 1;
    return 4;
}

u8 op_0x63_LD_H_E(Gameboy& gb)
{
    gb.HL_bytes.H = gb.DE_bytes.E; // copy E into H

    gb.PC += 1;
    return 4;
}

u8 op_0x64_LD_H_H(Gameboy& gb)
{
    // no-op, just copying H to H
    gb.PC += 1;
    return 4;
}

u8 op_0x65_LD_H_L(Gameboy& gb)
{
    gb.HL_bytes.H = gb.HL_bytes.L; // copy L into H

    gb.PC += 1;
    return 4;
}

u8 op_0x66_LD_H_HL(Gameboy& gb)
{
    gb.HL_bytes.H = gb.read8(gb.HL); // load H from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x68_LD_L_B(Gameboy& gb)
{
    gb.HL_bytes.L = gb.BC_bytes.B; // copy B into L

    gb.PC += 1;
    return 4;
}

u8 op_0x69_LD_L_C(Gameboy& gb)
{
    gb.HL_bytes.L = gb.BC_bytes.C; // copy C into L

    gb.PC += 1;
    return 4;
}

u8 op_0x6A_LD_L_D(Gameboy& gb)
{
    gb.HL_bytes.L = gb.DE_bytes.D; // copy D into L

    gb.PC += 1;
    return 4;
}

u8 op_0x6B_LD_L_E(Gameboy& gb)
{
    gb.HL_bytes.L = gb.DE_bytes.E; // copy E into L

    gb.PC += 1;
    return 4;
}

u8 op_0x6C_LD_L_H(Gameboy& gb)
{
    gb.HL_bytes.L = gb.HL_bytes.H; // copy H into L

    gb.PC += 1;
    return 4;
}

u8 op_0x6D_LD_L_L(Gameboy& gb)
{
    // no-op, just copying L to L
    gb.PC += 1;
    return 4;
}

u8 op_0x6E_LD_L_HL(Gameboy& gb)
{
    gb.HL_bytes.L = gb.read8(gb.HL); // load L from memory at address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x6F_LD_L_A(Gameboy& gb)
{
    gb.HL_bytes.L = gb.AF_bytes.A; // copy A into L

    gb.PC += 1;
    return 4;
}

u8 op_0x70_LD_HL_B(Gameboy& gb)
{
    gb.write8(gb.HL, gb.BC_bytes.B); // write B to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x71_LD_HL_C(Gameboy& gb)
{
    gb.write8(gb.HL, gb.BC_bytes.C); // write C to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x72_LD_HL_D(Gameboy& gb)
{
    gb.write8(gb.HL, gb.DE_bytes.D); // write D to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x73_LD_HL_E(Gameboy& gb)
{
    gb.write8(gb.HL, gb.DE_bytes.E); // write E to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x74_LD_HL_H(Gameboy& gb)
{
    gb.write8(gb.HL, gb.HL_bytes.H); // write H to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x75_LD_HL_L(Gameboy& gb)
{
    gb.write8(gb.HL, gb.HL_bytes.L); // write L to address HL

    gb.PC += 1;
    return 8;
}

u8 op_0x76_HALT(Gameboy& gb)
{
    u8 pending = gb.read8(0xFFFF) & gb.read8(0xFF0F) & 0x1F;

    // HALT bug: if IME is disabled and an interrupt is already pending
    // PC fails to increment, causing the next byte to be read twice
    if (!gb.ime && pending) {
        gb.halt_bug = true;
    } else {
        // Normal case: enter halt state
        // CPU will wake when any interrupt becomes pending (IE & IF)
        gb.halted = true;
        gb.PC += 1;
    }

    return 4;
}

u8 op_0x7A_LD_A_D(Gameboy& gb)
{
    gb.AF_bytes.A = gb.DE_bytes.D; // copy D into A

    gb.PC += 1;
    return 4;
}

u8 op_0x7F_LD_A_A(Gameboy& gb)
{
    // no-op, just copying A to A
    gb.PC += 1;
    return 4;
}

u8 op_0x80_ADD_A_B(Gameboy& gb)
{
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) + static_cast<uint16_t>(gb.BC_bytes.B);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) + (gb.BC_bytes.B & 0x0F)) > 0x0F) * FLAG_H; // H flag if carry from bit 4
    gb.AF_bytes.F |= (result > 0xFF) * FLAG_C; // C flag if carry (result > 255)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x81_ADD_A_C(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 c = gb.BC_bytes.C;
    u16 result = a + c;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (c & 0x0F)) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x82_ADD_A_D(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 d = gb.DE_bytes.D;
    u16 result = a + d;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (d & 0x0F)) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x83_ADD_A_E(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 e = gb.DE_bytes.E;
    u16 result = a + e;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (e & 0x0F)) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x84_ADD_A_H(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 h = gb.HL_bytes.H;
    u16 result = a + h;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (h & 0x0F)) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x85_ADD_A_L(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 l = gb.HL_bytes.L;
    u16 result = a + l;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (l & 0x0F)) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x88_ADC_A_B(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 b = gb.BC_bytes.B;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + b + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (b & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x89_ADC_A_C(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 c = gb.BC_bytes.C;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + c + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (c & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x8A_ADC_A_D(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 d = gb.DE_bytes.D;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + d + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (d & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x8B_ADC_A_E(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 e = gb.DE_bytes.E;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + e + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (e & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x8C_ADC_A_H(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 h = gb.HL_bytes.H;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + h + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (h & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x8D_ADC_A_L(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 l = gb.HL_bytes.L;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + l + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (l & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0x8E_ADC_A_HL(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 value = gb.read8(gb.HL);
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a + value + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) + (value & 0x0F) + carry) > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 8;
}

u8 op_0x8F_ADC_A_A(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = (a << 1) + carry;

    gb.AF_bytes.F = ((result & 0xFF) == 0) * FLAG_Z | (((a & 0x0F) << 1) + carry > 0x0F) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 1;
    return 4;
}

u8 op_0xC4_CALL_NZ_u16(Gameboy& gb)
{
    uint16_t address = gb.read16(gb.PC + 1); // get 16-bit address from next two bytes

    if (!(gb.AF_bytes.F & FLAG_Z)) // if Z flag not set
    {
        gb.SP -= 2;
        gb.write16(gb.SP, gb.PC + 3); // push address of next instruction onto stack
        gb.PC = address; // jump to address
        return 24;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xC6_ADD_A_u8(Gameboy& gb)
{
    u8 value = gb.read8(gb.PC + 1);
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) + static_cast<uint16_t>(value);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) + (value & 0x0F)) > 0x0F) * FLAG_H; // H flag if carry from bit 4
    gb.AF_bytes.F |= (result > 0xFF) * FLAG_C; // C flag if carry (result > 255)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 2;
    return 8;
}

u8 op_0xD6_SUB_A_u8(Gameboy& gb)
{
    u8 value = gb.read8(gb.PC + 1);
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 2;
    return 8;
}

u8 op_0xB7_OR_A_A(Gameboy& gb)
{
    // A = A | A no-op
    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xAE_XOR_A_HL(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.read8(gb.HL); // A = A ^ value at address HL

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 8;
}

u8 op_0xCB_0x38_SRL_B(Gameboy& gb)
{
    u8 original = gb.BC_bytes.B; // save original value
    gb.BC_bytes.B >>= 1; // logical right shift, bit 7 = 0

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.BC_bytes.B == 0) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= (original & 0x01) ? FLAG_C : 0; // C flag from original bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x19_RR_C(Gameboy& gb)
{
    u8 original = gb.BC_bytes.C; // save original value
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 0x80 : 0x00;
    gb.BC_bytes.C = (gb.BC_bytes.C >> 1) | carry; // rotate right through carry

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.BC_bytes.C == 0) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= (original & 0x01) ? FLAG_C : 0; // C flag from original bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x1A_RR_D(Gameboy& gb)
{
    u8 original = gb.DE_bytes.D; // save original value
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 0x80 : 0x00;
    gb.DE_bytes.D = (gb.DE_bytes.D >> 1) | carry; // rotate right through carry

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.DE_bytes.D == 0) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= (original & 0x01) ? FLAG_C : 0; // C flag from original bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xEE_XOR_A_u8(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.read8(gb.PC + 1); // A = A ^ value at next byte

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCE_ADC_A_u8(Gameboy& gb)
{
    u8 value = gb.read8(gb.PC + 1);
    uint16_t carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) + static_cast<uint16_t>(value) + carry;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (((gb.AF_bytes.A & 0x0F) + (value & 0x0F) + carry) > 0x0F) * FLAG_H; // H flag if carry from bit 4
    gb.AF_bytes.F |= (result > 0xFF) * FLAG_C; // C flag if carry (result > 255)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 2;
    return 8;
}

u8 op_0xD0_RET_NC(Gameboy& gb)
{
    if (!(gb.AF_bytes.F & FLAG_C)) // if C flag not set
    {
        gb.PC = gb.read16(gb.SP); // pop address from stack into PC
        gb.SP += 2;
        return 20;
    }

    gb.PC += 1;
    return 8;
}

u8 op_0xB6_OR_A_HL(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.read8(gb.HL); // A = A | value at address HL

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 8;
}

u8 op_0xCB_0x1B_RR_E(Gameboy& gb)
{
    u8 original = gb.DE_bytes.E; // save original value
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 0x80 : 0x00;
    gb.DE_bytes.E = (gb.DE_bytes.E >> 1) | carry; // rotate right through carry

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.DE_bytes.E == 0) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= (original & 0x01) ? FLAG_C : 0; // C flag from original bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xAD_XOR_A_L(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.HL_bytes.L; // A = A ^ L

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xD8_RET_C(Gameboy& gb)
{
    if (gb.AF_bytes.F & FLAG_C) // if C flag is set
    {
        gb.PC = gb.read16(gb.SP); // pop address from stack into PC
        gb.SP += 2;
        return 20;
    }

    gb.PC += 1;
    return 8;
}

u8 op_0xC2_JP_NZ_u16(Gameboy& gb)
{
    if (!(gb.AF_bytes.F & FLAG_Z)) // if Z flag not set
    {
        gb.PC = gb.read16(gb.PC + 1); // jump to address
        return 16;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xBB_CP_A_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.PC += 1;
    return 4;
}

u8 op_0xF8_LD_HL_SP_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1));
    gb.HL = gb.SP + offset;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (((gb.SP & 0x0F) + (offset & 0x0F)) > 0x0F) * FLAG_H; // H flag if carry from bit 4
    gb.AF_bytes.F |= (((gb.SP & 0xFF) + (offset & 0xFF)) > 0xFF) * FLAG_C; // C flag if carry from bit 8

    gb.PC += 2;
    return 12;
}

u8 op_0xF9_LD_SP_HL(Gameboy& gb)
{
    gb.SP = gb.HL; // copy HL into SP

    gb.PC += 1;
    return 8;
}

u8 op_0xE8_ADD_SP_i8(Gameboy& gb)
{
    int8_t offset = static_cast<int8_t>(gb.read8(gb.PC + 1));
    uint16_t result = gb.SP + offset;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (((gb.SP & 0x0F) + (offset & 0x0F)) > 0x0F) * FLAG_H; // H flag if carry from bit 4
    gb.AF_bytes.F |= (((gb.SP & 0xFF) + (offset & 0xFF)) > 0xFF) * FLAG_C; // C flag if carry from bit 8

    gb.SP = result;

    gb.PC += 2;
    return 16;
}

u8 op_0xF6_OR_A_u8(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.read8(gb.PC + 1); // A = A | value at next byte

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xDE_SBC_A_u8(Gameboy& gb)
{
    u8 a = gb.AF_bytes.A;
    u8 value = gb.read8(gb.PC + 1);
    u8 carry = (gb.AF_bytes.F & FLAG_C) >> 4;
    u16 result = a - value - carry;

    gb.AF_bytes.F = FLAG_N | ((result & 0xFF) == 0) * FLAG_Z | ((a & 0x0F) < ((value & 0x0F) + carry)) * FLAG_H | (result > 0xFF) * FLAG_C;
    gb.AF_bytes.A = static_cast<u8>(result);

    gb.PC += 2;
    return 8;
}

u8 op_0xD2_JP_NC_u16(Gameboy& gb)
{
    if (!(gb.AF_bytes.F & FLAG_C)) // if C flag not set
    {
        gb.PC = gb.read16(gb.PC + 1); // jump to address
        return 16;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xDA_JP_C_u16(Gameboy& gb)
{
    if (gb.AF_bytes.F & FLAG_C) // if C flag is set
    {
        gb.PC = gb.read16(gb.PC + 1); // jump to address
        return 16;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xCC_CALL_Z_u16(Gameboy& gb)
{
    if (gb.AF_bytes.F & FLAG_Z) // if Z flag is set
    {
        gb.SP -= 2;
        gb.write16(gb.SP, gb.PC + 3); // push address of next instruction onto stack
        gb.PC = gb.read16(gb.PC + 1); // jump to address
        return 24;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xD4_CALL_NC_u16(Gameboy& gb)
{
    if (!(gb.AF_bytes.F & FLAG_C)) // if C flag not set
    {
        gb.SP -= 2;
        gb.write16(gb.SP, gb.PC + 3); // push address of next instruction onto stack
        gb.PC = gb.read16(gb.PC + 1); // jump to address
        return 24;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xDC_CALL_C_u16(Gameboy& gb)
{
    if (gb.AF_bytes.F & FLAG_C) // if C flag is set
    {
        gb.SP -= 2;
        gb.write16(gb.SP, gb.PC + 3); // push address of next instruction onto stack
        gb.PC = gb.read16(gb.PC + 1); // jump to address
        return 24;
    }

    gb.PC += 3;
    return 12;
}

u8 op_0xC7_RST_00h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x00; // jump to address 0x00
    return 16;
}

u8 op_0xCF_RST_08h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x08; // jump to address 0x08
    return 16;
}

u8 op_0xD7_RST_10h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x10; // jump to address 0x10
    return 16;
}

u8 op_0xDF_RST_18h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x18; // jump to address 0x18
    return 16;
}

u8 op_0xE7_RST_20h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x20; // jump to address 0x20
    return 16;
}

u8 op_0xF7_RST_30h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x30; // jump to address 0x30
    return 16;
}

u8 op_0xFF_RST_38h(Gameboy& gb)
{
    gb.SP -= 2;
    gb.write16(gb.SP, gb.PC + 1); // push address of next instruction onto stack
    gb.PC = 0x38; // jump to address 0x38
    return 16;
}

u8 op_0xF2_LD_A_FF00_C(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(0xFF00 + gb.BC_bytes.C); // load A from address 0xFF00 + C

    gb.PC += 1;
    return 8;
}

u8 op_0xB2_OR_A_D(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.DE_bytes.D; // A = A | D

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xB3_OR_A_E(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.DE_bytes.E; // A = A | E

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xB4_OR_A_H(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.HL_bytes.H; // A = A | H

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xB5_OR_A_L(Gameboy& gb)
{
    gb.AF_bytes.A |= gb.HL_bytes.L; // A = A | L

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xB8_CP_A_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 A = gb.AF_bytes.A;

    gb.AF_bytes.F = FLAG_N; // N flag set
    gb.AF_bytes.F |= (A == value) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= ((A & 0x0F) < (value & 0x0F)) ? FLAG_H : 0; // H flag
    gb.AF_bytes.F |= (A < value) ? FLAG_C : 0; // C flag

    gb.PC += 1;
    return 4;
}

u8 op_0xB9_CP_A_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    u8 A = gb.AF_bytes.A;

    gb.AF_bytes.F = FLAG_N; // N flag set
    gb.AF_bytes.F |= (A == value) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= ((A & 0x0F) < (value & 0x0F)) ? FLAG_H : 0; // H flag
    gb.AF_bytes.F |= (A < value) ? FLAG_C : 0; // C flag

    gb.PC += 1;
    return 4;
}

u8 op_0xBA_CP_A_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    u8 A = gb.AF_bytes.A;

    gb.AF_bytes.F = FLAG_N; // N flag set
    gb.AF_bytes.F |= (A == value) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= ((A & 0x0F) < (value & 0x0F)) ? FLAG_H : 0; // H flag
    gb.AF_bytes.F |= (A < value) ? FLAG_C : 0; // C flag

    gb.PC += 1;
    return 4;
}

u8 op_0xBC_CP_A_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 A = gb.AF_bytes.A;

    gb.AF_bytes.F = FLAG_N; // N flag set
    gb.AF_bytes.F |= (A == value) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= ((A & 0x0F) < (value & 0x0F)) ? FLAG_H : 0; // H flag
    gb.AF_bytes.F |= (A < value) ? FLAG_C : 0; // C flag

    gb.PC += 1;
    return 4;
}

u8 op_0xBD_CP_A_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 A = gb.AF_bytes.A;

    gb.AF_bytes.F = FLAG_N; // N flag set
    gb.AF_bytes.F |= (A == value) ? FLAG_Z : 0; // Z flag
    gb.AF_bytes.F |= ((A & 0x0F) < (value & 0x0F)) ? FLAG_H : 0; // H flag
    gb.AF_bytes.F |= (A < value) ? FLAG_C : 0; // C flag

    gb.PC += 1;
    return 4;
}

u8 op_0xBF_CP_A_A(Gameboy& gb)
{
    gb.AF_bytes.F = FLAG_N | FLAG_Z; // Only N and Z are set

    gb.PC += 1;
    return 4;
}

u8 op_0x91_SUB_A_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x92_SUB_A_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x93_SUB_A_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x94_SUB_A_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x95_SUB_A_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 4;
}

u8 op_0x96_SUB_A_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value);

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < (value & 0x0F)) * FLAG_H; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (static_cast<int16_t>(result) < 0) * FLAG_C; // C flag if borrow (result < 0)

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF); // store low 8 bits of result in A

    gb.PC += 1;
    return 8;
}

u8 op_0x97_SUB_A_A(Gameboy& gb)
{
    gb.AF_bytes.A = 0; // A - A is always 0
    gb.AF_bytes.F = FLAG_N | FLAG_Z; // N and Z set, H and C cleared
    gb.PC += 1;
    return 4;
}

u8 op_0x98_SBC_A_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0x99_SBC_A_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0x9A_SBC_A_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0x9B_SBC_A_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0x9C_SBC_A_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0x9D_SBC_A_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0x9E_SBC_A_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(value) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((value & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 8;
}

u8 op_0x9F_SBC_A_A(Gameboy& gb)
{
    u8 carry = (gb.AF_bytes.F & FLAG_C) ? 1 : 0;
    uint16_t result = static_cast<uint16_t>(gb.AF_bytes.A) - static_cast<uint16_t>(gb.AF_bytes.A) - carry;

    gb.AF_bytes.F = FLAG_N; // set N flag, clear others
    gb.AF_bytes.F |= ((result & 0xFF) == 0) ? FLAG_Z : 0; // Z flag if result is 0
    gb.AF_bytes.F |= ((gb.AF_bytes.A & 0x0F) < ((gb.AF_bytes.A & 0x0F) + carry)) ? FLAG_H : 0; // H flag if borrow from bit 4
    gb.AF_bytes.F |= (result > 0xFF) ? FLAG_C : 0; // C flag if borrow

    gb.AF_bytes.A = static_cast<u8>(result & 0xFF);

    gb.PC += 1;
    return 4;
}

u8 op_0xA0_AND_A_B(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.BC_bytes.B; // A = A & B

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= FLAG_H; // H flag set

    gb.PC += 1;
    return 4;
}

u8 op_0xA2_AND_A_D(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.DE_bytes.D; // A = A & D

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= FLAG_H; // H flag set

    gb.PC += 1;
    return 4;
}

u8 op_0xA3_AND_A_E(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.DE_bytes.E; // A = A & E

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= FLAG_H; // H flag set

    gb.PC += 1;
    return 4;
}

u8 op_0xA4_AND_A_H(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.HL_bytes.H; // A = A & H

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= FLAG_H; // H flag set

    gb.PC += 1;
    return 4;
}

u8 op_0xA5_AND_A_L(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.HL_bytes.L; // A = A & L

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= FLAG_H; // H flag set

    gb.PC += 1;
    return 4;
}

u8 op_0xA6_AND_A_HL(Gameboy& gb)
{
    gb.AF_bytes.A &= gb.read8(gb.HL); // A = A & value at address HL

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= FLAG_H; // H flag set

    gb.PC += 1;
    return 8;
}

u8 op_0xA8_XOR_A_B(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.BC_bytes.B; // A = A ^ B

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xAA_XOR_A_D(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.DE_bytes.D; // A = A ^ D

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xAB_XOR_A_E(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.DE_bytes.E; // A = A ^ E

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xAC_XOR_A_H(Gameboy& gb)
{
    gb.AF_bytes.A ^= gb.HL_bytes.H; // A = A ^ H

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.AF_bytes.A == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 1;
    return 4;
}

u8 op_0xCB_0x00_RLC_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    gb.BC_bytes.B = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if B is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.BC_bytes.B == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x01_RLC_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    gb.BC_bytes.C = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if C is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.BC_bytes.C == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x02_RLC_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    gb.DE_bytes.D = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if D is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.DE_bytes.D == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x03_RLC_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    gb.DE_bytes.E = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if E is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.DE_bytes.E == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x04_RLC_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    gb.HL_bytes.H = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if H is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.HL_bytes.H == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x05_RLC_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    gb.HL_bytes.L = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if L is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.HL_bytes.L == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x06_RLC_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 result = (value << 1) | (value >> 7); // Rotate left
    gb.write8(gb.HL, result);

    // clear all flags, then set Z if HL is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((result == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x07_RLC_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    gb.AF_bytes.A = (value << 1) | (value >> 7); // Rotate left

    // clear all flags, then set Z if A is 0, and C if bit 7 was set
    gb.AF_bytes.F = ((gb.AF_bytes.A == 0) ? FLAG_Z : 0) | ((value >> 7) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x08_RRC_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    gb.BC_bytes.B = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if B is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.BC_bytes.B == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x09_RRC_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    gb.BC_bytes.C = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if C is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.BC_bytes.C == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x0A_RRC_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    gb.DE_bytes.D = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if D is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.DE_bytes.D == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x0B_RRC_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    gb.DE_bytes.E = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if E is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.DE_bytes.E == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x0C_RRC_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    gb.HL_bytes.H = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if H is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.HL_bytes.H == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x0D_RRC_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    gb.HL_bytes.L = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if L is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.HL_bytes.L == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x0E_RRC_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 result = (value >> 1) | (value << 7); // Rotate right
    gb.write8(gb.HL, result);

    // clear all flags, then set Z if HL is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((result == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x0F_RRC_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    gb.AF_bytes.A = (value >> 1) | (value << 7); // Rotate right

    // clear all flags, then set Z if A is 0, and C if bit 0 was set
    gb.AF_bytes.F = ((gb.AF_bytes.A == 0) ? FLAG_Z : 0) | ((value & 0x01) ? FLAG_C : 0);

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x27_SLA_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    u8 result = value << 1; // Shift left
    gb.AF_bytes.A = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x7F_BIT_7_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x86_RES_0_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 0); // Reset bit 0
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x50_BIT_2_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x60_BIT_4_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x68_BIT_5_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x58_BIT_3_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x7E_BIT_7_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x3F_SRL_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    u8 result = value >> 1; // Shift right
    gb.AF_bytes.A = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x40_BIT_0_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x5F_BIT_3_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x33_SWAP_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    u8 result = (value << 4) | (value >> 4); // Swap nibbles
    gb.DE_bytes.E = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x77_BIT_6_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x6F_BIT_5_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xFE_SET_7_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 7); // Set bit 7
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xBE_RES_7_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 7); // Reset bit 7
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x48_BIT_1_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x61_BIT_4_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x69_BIT_5_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x47_BIT_0_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x41_BIT_0_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x42_BIT_0_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x43_BIT_0_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x44_BIT_0_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x45_BIT_0_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x46_BIT_0_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 0)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 0 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x49_BIT_1_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x4A_BIT_1_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x4B_BIT_1_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x4C_BIT_1_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x4D_BIT_1_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x4E_BIT_1_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x4F_BIT_1_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 1)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 1 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x10_RL_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.BC_bytes.B = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x12_RL_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.DE_bytes.D = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x13_RL_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.DE_bytes.E = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x14_RL_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.HL_bytes.H = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x15_RL_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.HL_bytes.L = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x16_RL_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.write8(gb.HL, result);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 16;
}
u8 op_0xCB_0x17_RL_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value << 1) | carry; // Rotate left through carry
    gb.AF_bytes.A = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x18_RR_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value >> 1) | (carry << 7); // Rotate right through carry
    gb.BC_bytes.B = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x1C_RR_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value >> 1) | (carry << 7); // Rotate right through carry
    gb.HL_bytes.H = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x1D_RR_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value >> 1) | (carry << 7); // Rotate right through carry
    gb.HL_bytes.L = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x1E_RR_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value >> 1) | (carry << 7); // Rotate right through carry
    gb.write8(gb.HL, result);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x1F_RR_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    u8 carry = (gb.AF_bytes.F & FLAG_C) != 0;
    u8 result = (value >> 1) | (carry << 7); // Rotate right through carry
    gb.AF_bytes.A = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x70_BIT_6_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x71_BIT_6_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x72_BIT_6_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x73_BIT_6_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x74_BIT_6_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x75_BIT_6_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x76_BIT_6_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 6)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 6 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x78_BIT_7_B(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.B & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x79_BIT_7_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x7A_BIT_7_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x7B_BIT_7_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x7D_BIT_7_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 7)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 7 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x51_BIT_2_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x52_BIT_2_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x53_BIT_2_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x54_BIT_2_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x55_BIT_2_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x56_BIT_2_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x57_BIT_2_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 2)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 2 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x59_BIT_3_C(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.BC_bytes.C & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x5A_BIT_3_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x5B_BIT_3_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x5C_BIT_3_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x5D_BIT_3_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x5E_BIT_3_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 3)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 3 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0xF0_SET_6_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF1_SET_6_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF2_SET_6_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF3_SET_6_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF4_SET_6_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF5_SET_6_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF6_SET_6_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 6); // Set bit 6
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xF7_SET_6_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 6); // Set bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF8_SET_7_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xF9_SET_7_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xFA_SET_7_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xFB_SET_7_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xFC_SET_7_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xFD_SET_7_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xFF_SET_7_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 7); // Set bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD0_SET_2_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD1_SET_2_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD2_SET_2_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD3_SET_2_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD4_SET_2_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD5_SET_2_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD6_SET_2_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 2); // Set bit 2
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xD7_SET_2_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 2); // Set bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD8_SET_3_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xD9_SET_3_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xDA_SET_3_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xDB_SET_3_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xDC_SET_3_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xDD_SET_3_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xDE_SET_3_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 3); // Set bit 3
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xDF_SET_3_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 3); // Set bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x20_SLA_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 result = value << 1; // Shift left
    gb.BC_bytes.B = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x21_SLA_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    u8 result = value << 1; // Shift left
    gb.BC_bytes.C = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x22_SLA_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    u8 result = value << 1; // Shift left
    gb.DE_bytes.D = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x23_SLA_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    u8 result = value << 1; // Shift left
    gb.DE_bytes.E = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x24_SLA_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 result = value << 1; // Shift left
    gb.HL_bytes.H = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x25_SLA_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 result = value << 1; // Shift left
    gb.HL_bytes.L = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x26_SLA_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 result = value << 1; // Shift left
    gb.write8(gb.HL, result);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value >> 7) * FLAG_C; // C flag if bit 7 of original value was set

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x28_SRA_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.BC_bytes.B = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x29_SRA_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.BC_bytes.C = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x2A_SRA_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.DE_bytes.D = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x2B_SRA_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.DE_bytes.E = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x2C_SRA_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.HL_bytes.H = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x2D_SRA_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.HL_bytes.L = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x2E_SRA_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.write8(gb.HL, result);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x2F_SRA_A(Gameboy& gb)
{
    u8 value = gb.AF_bytes.A;
    u8 msb = value & 0x80; // Preserve the most significant bit
    u8 result = (value >> 1) | msb; // Shift right and preserve MSB
    gb.AF_bytes.A = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x30_SWAP_B(Gameboy& gb)
{
    u8 value = gb.BC_bytes.B;
    gb.BC_bytes.B = (value << 4) | (value >> 4); // Swap nibbles

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.BC_bytes.B == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x31_SWAP_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    gb.BC_bytes.C = (value << 4) | (value >> 4); // Swap nibbles

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.BC_bytes.C == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x32_SWAP_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    gb.DE_bytes.D = (value << 4) | (value >> 4); // Swap nibbles

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.DE_bytes.D == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x34_SWAP_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    gb.HL_bytes.H = (value << 4) | (value >> 4); // Swap nibbles

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.HL_bytes.H == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x35_SWAP_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    gb.HL_bytes.L = (value << 4) | (value >> 4); // Swap nibbles

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (gb.HL_bytes.L == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x36_SWAP_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 result = (value << 4) | (value >> 4); // Swap nibbles
    gb.write8(gb.HL, result);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x39_SRL_C(Gameboy& gb)
{
    u8 value = gb.BC_bytes.C;
    u8 result = value >> 1; // Shift right
    gb.BC_bytes.C = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x3A_SRL_D(Gameboy& gb)
{
    u8 value = gb.DE_bytes.D;
    u8 result = value >> 1; // Shift right
    gb.DE_bytes.D = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x3B_SRL_E(Gameboy& gb)
{
    u8 value = gb.DE_bytes.E;
    u8 result = value >> 1; // Shift right
    gb.DE_bytes.E = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x3C_SRL_H(Gameboy& gb)
{
    u8 value = gb.HL_bytes.H;
    u8 result = value >> 1; // Shift right
    gb.HL_bytes.H = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x3D_SRL_L(Gameboy& gb)
{
    u8 value = gb.HL_bytes.L;
    u8 result = value >> 1; // Shift right
    gb.HL_bytes.L = result;

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x3E_SRL_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    u8 result = value >> 1; // Shift right
    gb.write8(gb.HL, result);

    gb.AF_bytes.F = 0; // clear all flags
    gb.AF_bytes.F |= (result == 0) * FLAG_Z; // Z flag if result is 0
    gb.AF_bytes.F |= (value & 0x01) * FLAG_C; // C flag if bit 0 of original value was set

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x62_BIT_4_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x63_BIT_4_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x64_BIT_4_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x65_BIT_4_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x66_BIT_4_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x67_BIT_4_A(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.AF_bytes.A & (1 << 4)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 4 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x6A_BIT_5_D(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.D & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x6B_BIT_5_E(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.DE_bytes.E & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x6C_BIT_5_H(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.H & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x6D_BIT_5_L(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.HL_bytes.L & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x6E_BIT_5_HL(Gameboy& gb)
{
    gb.AF_bytes.F &= ~FLAG_Z; // Clear Z flag
    gb.AF_bytes.F |= FLAG_H; // Set H flag
    gb.AF_bytes.F &= ~FLAG_N; // Clear N flag

    if ((gb.read8(gb.HL) & (1 << 5)) == 0) {
        gb.AF_bytes.F |= FLAG_Z; // Set Z flag if bit 5 is 0
    }

    gb.PC += 2;
    return 12;
}

u8 op_0xCB_0x80_RES_0_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 0); // Reset bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x81_RES_0_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 0); // Reset bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x82_RES_0_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 0); // Reset bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x83_RES_0_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 0); // Reset bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x84_RES_0_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 0); // Reset bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x85_RES_0_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 0); // Reset bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x88_RES_1_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x89_RES_1_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x8A_RES_1_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x8B_RES_1_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x8C_RES_1_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x8D_RES_1_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x8E_RES_1_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 1); // Reset bit 1
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x8F_RES_1_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 1); // Reset bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x90_RES_2_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x91_RES_2_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x92_RES_2_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x93_RES_2_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x94_RES_2_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x95_RES_2_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x96_RES_2_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 2); // Reset bit 2
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x97_RES_2_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 2); // Reset bit 2

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x98_RES_3_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x99_RES_3_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x9A_RES_3_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x9B_RES_3_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x9C_RES_3_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x9D_RES_3_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0x9E_RES_3_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 3); // Reset bit 3
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0x9F_RES_3_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 3); // Reset bit 3

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA0_RES_4_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA1_RES_4_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA2_RES_4_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA3_RES_4_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA4_RES_4_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA5_RES_4_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA6_RES_4_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 4); // Reset bit 4
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xA7_RES_4_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 4); // Reset bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA8_RES_5_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xA9_RES_5_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xAA_RES_5_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xAB_RES_5_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xAC_RES_5_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xAD_RES_5_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xAE_RES_5_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 5); // Reset bit 5
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xAF_RES_5_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 5); // Reset bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB0_RES_6_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB1_RES_6_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB2_RES_6_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB3_RES_6_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB4_RES_6_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB5_RES_6_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB6_RES_6_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value &= ~(1 << 6); // Reset bit 6
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xB7_RES_6_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 6); // Reset bit 6

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB8_RES_7_B(Gameboy& gb)
{
    gb.BC_bytes.B &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xB9_RES_7_C(Gameboy& gb)
{
    gb.BC_bytes.C &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xBA_RES_7_D(Gameboy& gb)
{
    gb.DE_bytes.D &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xBB_RES_7_E(Gameboy& gb)
{
    gb.DE_bytes.E &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xBC_RES_7_H(Gameboy& gb)
{
    gb.HL_bytes.H &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xBD_RES_7_L(Gameboy& gb)
{
    gb.HL_bytes.L &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xBF_RES_7_A(Gameboy& gb)
{
    gb.AF_bytes.A &= ~(1 << 7); // Reset bit 7

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC0_SET_0_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC1_SET_0_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC2_SET_0_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC3_SET_0_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC4_SET_0_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC5_SET_0_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC6_SET_0_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 0); // Set bit 0
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xC7_SET_0_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 0); // Set bit 0

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC8_SET_1_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xC9_SET_1_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xCA_SET_1_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xCB_SET_1_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xCC_SET_1_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xCD_SET_1_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xCE_SET_1_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 1); // Set bit 1
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xCF_SET_1_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 1); // Set bit 1

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE0_SET_4_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE1_SET_4_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE2_SET_4_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE3_SET_4_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE4_SET_4_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE5_SET_4_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE6_SET_4_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 4); // Set bit 4
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xE7_SET_4_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 4); // Set bit 4

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE8_SET_5_B(Gameboy& gb)
{
    gb.BC_bytes.B |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xE9_SET_5_C(Gameboy& gb)
{
    gb.BC_bytes.C |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xEA_SET_5_D(Gameboy& gb)
{
    gb.DE_bytes.D |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xEB_SET_5_E(Gameboy& gb)
{
    gb.DE_bytes.E |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xEC_SET_5_H(Gameboy& gb)
{
    gb.HL_bytes.H |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xED_SET_5_L(Gameboy& gb)
{
    gb.HL_bytes.L |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0xCB_0xEE_SET_5_HL(Gameboy& gb)
{
    u8 value = gb.read8(gb.HL);
    value |= (1 << 5); // Set bit 5
    gb.write8(gb.HL, value);

    gb.PC += 2;
    return 16;
}

u8 op_0xCB_0xEF_SET_5_A(Gameboy& gb)
{
    gb.AF_bytes.A |= (1 << 5); // Set bit 5

    gb.PC += 2;
    return 8;
}

u8 op_0x10_STOP(Gameboy& gb)
{
    // not a real implementation, but apparently no licensed games use this
    gb.write8(0xFF04, 0); // reset DIV register
    gb.PC += 1;
    return 4;
}

u8 op_unimplemented(Gameboy& gb)
{
    std::cerr << "Unimplemented opcode: 0x" << std::hex
              << (int)gb.read8(gb.PC) << " at PC: " << gb.PC << std::dec
              << std::endl;

    std::cerr << "Next byte in memory: 0x" << std::hex
              << (int)gb.read8(gb.PC + 1) << std::dec << std::endl;
    exit(1);

    return 0;
}
