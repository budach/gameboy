#pragma once

#include "gameboy.h"

void op_unimplemented(Gameboy& gb);
void op_nop([[maybe_unused]] Gameboy& gb);
void op_read_byte_low(Gameboy& gb);
void op_read_byte_high(Gameboy& gb);
void op_jump_u16(Gameboy& gb);
void op_set_B(Gameboy& gb);
void op_set_C(Gameboy& gb);
void op_set_E(Gameboy& gb);
void op_set_D(Gameboy& gb);
void op_set_L(Gameboy& gb);
void op_set_H(Gameboy& gb);
void op_set_SP(Gameboy& gb);
void op_write_SP(Gameboy& gb);
void op_pop_C(Gameboy& gb);
void op_pop_B(Gameboy& gb);
void op_pop_E(Gameboy& gb);
void op_pop_D(Gameboy& gb);
void op_pop_L(Gameboy& gb);
void op_pop_H(Gameboy& gb);
void op_pop_A(Gameboy& gb);
void op_pop_F(Gameboy& gb);
void op_push_B(Gameboy& gb);
void op_push_C(Gameboy& gb);
void op_push_D(Gameboy& gb);
void op_push_E(Gameboy& gb);
void op_push_H(Gameboy& gb);
void op_push_L(Gameboy& gb);
void op_push_A(Gameboy& gb);
void op_push_F(Gameboy& gb);
void op_write_SP_HL(Gameboy& gb);
