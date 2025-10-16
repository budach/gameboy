#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using u8 = uint8_t;
using i8 = int8_t;
using u16 = uint16_t;
using i16 = int16_t;
using u32 = uint32_t;
using u64 = uint64_t;

constexpr u8 FLAG_Z = 1 << 7; // zero flag
constexpr u8 FLAG_N = 1 << 6; // subtract flag
constexpr u8 FLAG_H = 1 << 5; // half carry flag
constexpr u8 FLAG_C = 1 << 4; // carry flag
constexpr u16 DIV = 0xFF04; // address of divider register
constexpr u16 TIMA = 0xFF05; // address of timer counter
constexpr u16 TMA = 0xFF06; // address of timer modulator
constexpr u16 TMC = 0xFF07; // address of timer frequency and on/off
constexpr int CLOCKSPEED = 4194304; // 4.194304 MHz
constexpr int SCREEN_WIDTH = 160;
constexpr int SCREEN_HEIGHT = 144;
constexpr int SCREEN_SCALE = 5;

struct PPU_Color {
    u8 r, g, b, a;
};

constexpr PPU_Color DMG_PALETTE[4] = {
    { 0xE0, 0xF8, 0xD0, 0xFF }, // White
    { 0x88, 0xC0, 0x70, 0xFF }, // Light gray
    { 0x34, 0x68, 0x56, 0xFF }, // Dark gray
    { 0x08, 0x18, 0x20, 0xFF }, // Black
};

struct Gameboy {

    /* ----------------- */
    /* ---  members  --- */
    /* ----------------- */

    u8 (*opcodes[256])(Gameboy&); // opcode table
    u8 (*cb_opcodes[256])(Gameboy&); // CB-prefixed opcode table

    u64 interrupt_counts[5]; // interrupt request counts for debugging

    std::vector<u8> memory; // 64KB addressable memory
    std::vector<u8> cartridge; // full cartridge content
    std::vector<u8> ram_banks; // external RAM banks (if any)
    std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT * 4> framebuffer_back; // 160x144 pixels, RGBA format (back buffer)
    std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT * 4> framebuffer_front; // display buffer (front buffer)
    std::string header_title; // game title from ROM header

    void* texture; // raylib texture for rendering

    int timer_counter; // counts CPU cycles for timer
    int divider_counter; // counts CPU cycles for divider register
    int scanline_counter; // counts CPU cycles for PPU scanlines

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
    u8 mbc_type; // memory bank controller type
    u8 current_rom_bank; // currently loaded ROM bank number
    u8 current_ram_bank; // currently loaded RAM bank number
    bool ime; // interrupt master enable
    bool ime_scheduled; // whether to enable IME after next instruction
    bool halted; // whether the CPU is halted
    bool halt_bug; // whether the CPU is in halt bug state
    bool ram_enabled; // whether external RAM is enabled
    bool rom_banking; // whether in ROM banking mode

    /* ----------------- */
    /* ---  methods  --- */
    /* ----------------- */

    Gameboy(const std::string& path_rom);
    ~Gameboy();

    u8 read8(u16 addr) const;
    u16 read16(u16 addr) const;
    void write8(u16 addr, u8 value);
    void write16(u16 addr, u16 value);

    u8 run_opcode();
    void run_one_frame();
    void render_screen();
    void update_inputs();
    void request_interrupt(u8 bit);
    void update_timers(u8 cycles);
    void ppu_step(u8 cycles);
    u8 check_interrupts();
    void init_graphics();
    void cleanup_graphics();
    void handle_banking(u16 addr, u8 value);
    PPU_Color get_color(u16 palette_register, u8 color_id);
};
