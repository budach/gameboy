#include <iostream>

#include "gameboy.h"

void op_unimplemented(Gameboy& gb)
{
    std::cerr << "!!! Unimplemented opcode at PC=0x" << std::hex << gb.PC - 1 << std::dec << std::endl;
    std::cerr << "!!! opcode: 0x" << std::hex << (int)gb.read8(gb.PC - 1) << std::dec << std::endl;
    exit(1);
}

void op_nop([[maybe_unused]] Gameboy& gb)
{
    // nothing
}

void op_read_byte_low(Gameboy& gb)
{
    gb.low = gb.read8(gb.PC++);
}

void op_read_byte_high(Gameboy& gb)
{
    gb.high = gb.read8(gb.PC++);
}

void op_jump_u16(Gameboy& gb)
{
    gb.PC = (gb.high << 8) | gb.low;
}

void op_set_B(Gameboy& gb)
{
    gb.BC_bytes.B = gb.read8(gb.PC++);
}

void op_set_C(Gameboy& gb)
{
    gb.BC_bytes.C = gb.read8(gb.PC++);
}

void op_set_E(Gameboy& gb)
{
    gb.DE_bytes.E = gb.read8(gb.PC++);
}

void op_set_D(Gameboy& gb)
{
    gb.DE_bytes.D = gb.read8(gb.PC++);
}

void op_set_L(Gameboy& gb)
{
    gb.HL_bytes.L = gb.read8(gb.PC++);
}

void op_set_H(Gameboy& gb)
{
    gb.HL_bytes.H = gb.read8(gb.PC++);
}

void op_set_SP(Gameboy& gb)
{
    gb.SP = gb.read16(gb.PC);
    gb.PC += 2;
}

void op_write_SP(Gameboy& gb)
{
    gb.write16((gb.high << 8) | gb.low, gb.SP);
}

void op_pop_C(Gameboy& gb)
{
    gb.BC_bytes.C = gb.read8(gb.SP++);
}

void op_pop_B(Gameboy& gb)
{
    gb.BC_bytes.B = gb.read8(gb.SP++);
}

void op_pop_E(Gameboy& gb)
{
    gb.DE_bytes.E = gb.read8(gb.SP++);
}

void op_pop_D(Gameboy& gb)
{
    gb.DE_bytes.D = gb.read8(gb.SP++);
}

void op_pop_L(Gameboy& gb)
{
    gb.HL_bytes.L = gb.read8(gb.SP++);
}

void op_pop_H(Gameboy& gb)
{
    gb.HL_bytes.H = gb.read8(gb.SP++);
}

void op_pop_A(Gameboy& gb)
{
    gb.AF_bytes.A = gb.read8(gb.SP++);
}

void op_pop_F(Gameboy& gb)
{
    gb.AF_bytes.F = gb.read8(gb.SP++);
    gb.AF_bytes.F &= 0xF0; // lower 4 bits of F are always 0
}

void op_push_B(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.BC_bytes.B);
}

void op_push_C(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.BC_bytes.C);
}

void op_push_D(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.DE_bytes.D);
}

void op_push_E(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.DE_bytes.E);
}

void op_push_H(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.HL_bytes.H);
}

void op_push_L(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.HL_bytes.L);
}

void op_push_A(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.AF_bytes.A);
}

void op_push_F(Gameboy& gb)
{
    gb.write8(--gb.SP, gb.AF_bytes.F);
}

void op_write_SP_HL(Gameboy& gb)
{
    gb.SP = gb.HL;
}