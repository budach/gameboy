#pragma once

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// the current register approach will not work on big-endian machines
static_assert(std::endian::native == std::endian::little,
    "This build assumes a little-endian host (std::endian::little)");

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

constexpr u8 FLAG_Z = 1 << 7; // zero flag
constexpr u8 FLAG_N = 1 << 6; // subtract flag
constexpr u8 FLAG_H = 1 << 5; // half carry flag
constexpr u8 FLAG_C = 1 << 4; // carry flag

struct Gameboy {

    /* ----------------- */
    /* ---  members  --- */
    /* ----------------- */

    u8 (*opcodes[256])(Gameboy&); // opcode table
    u8 (*cb_opcodes[256])(Gameboy&); // CB-prefixed opcode table

    std::vector<u8> memory; // 64KB addressable memory
    std::vector<u8> cartridge; // full cartridge content

    union { // registers
        u16 AF;
        struct {
            u8 F;
            u8 A;
        } AF_bytes;
    };

    union { // registers
        u16 BC;
        struct {
            u8 C;
            u8 B;
        } BC_bytes;
    };

    union { // registers
        u16 DE;
        struct {
            u8 E;
            u8 D;
        } DE_bytes;
    };

    union { // registers
        u16 HL;
        struct {
            u8 L;
            u8 H;
        } HL_bytes;
    };

    u16 SP; // stack pointer
    u16 PC; // program counter
    u8 joypad_state; // current button states
    bool ime; // interrupt master enable
    bool ime_scheduled; // whether to enable IME after next instruction
    bool halted; // whether the CPU is halted
    bool halt_bug; // whether the CPU is in halt bug state

    /* ----------------- */
    /* ---  methods  --- */
    /* ----------------- */

    Gameboy(const std::string& path_rom);

    u8 read8(u16 addr) const;
    u16 read16(u16 addr) const;
    void write8(u16 addr, u8 value);
    void write16(u16 addr, u16 value);

    u8 run_opcode();
    void run_one_frame();
    void render_screen();
    void update_inputs();
    void request_interrupt(u8 bit);
    u8 check_interrupts();
};
