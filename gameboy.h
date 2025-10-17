#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
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
constexpr u32 CYCLES_PER_FRAME = 70224;

struct PPU_Color {
    u8 r, g, b;
};

constexpr PPU_Color DMG_PALETTE[4] = {
    { 0xE0, 0xF8, 0xD0 }, // White
    { 0x88, 0xC0, 0x70 }, // Light gray
    { 0x34, 0x68, 0x56 }, // Dark gray
    { 0x08, 0x18, 0x20 }, // Black
};

struct Sprite {
    u8 x;
    u8 y;
    u8 tile;
    u8 attributes;
    u8 oam_index;
};

struct Gameboy {

    /* ----------------- */
    /* ---  members  --- */
    /* ----------------- */

    /* opcode tables */
    u8 (*opcodes[256])(Gameboy&); // opcode table
    u8 (*cb_opcodes[256])(Gameboy&); // CB-prefixed opcode table

    /* CPU registers */
    union {
        u16 AF;
        struct {
            u8 F;
            u8 A;
        } AF_bytes;
    };

    union {
        u16 BC;
        struct {
            u8 C;
            u8 B;
        } BC_bytes;
    };

    union {
        u16 DE;
        struct {
            u8 E;
            u8 D;
        } DE_bytes;
    };

    union {
        u16 HL;
        struct {
            u8 L;
            u8 H;
        } HL_bytes;
    };

    /* core CPU/PPU state */
    u16 SP; // stack pointer
    u16 PC; // program counter
    int timer_counter; // counts CPU cycles for timer
    int divider_counter; // counts CPU cycles for divider register
    int scanline_counter; // counts CPU cycles for PPU scanlines
    int ppu_cycle; // current cycle within scanline
    int scanline_sprite_count; // number of sprites on current scanline
    int target_fps; // target frames per second
    u8 joypad_state; // current button states
    u8 ppu_mode; // current PPU mode (0-3)
    u8 window_line_counter; // how many window lines have been drawn this frame
    bool scanline_rendered; // whether the current scanline has been rendered
    std::array<std::array<PPU_Color, 4>, 3> palette_cache; // cached decoded palette colors

    std::array<Sprite, 10> scanline_sprites; // up to 10 sprites per scanline
    u8 mbc_type; // memory bank controller type
    bool ime; // interrupt master enable
    bool ime_scheduled; // whether to enable IME after next instruction
    bool halted; // whether the CPU is halted
    bool halt_bug; // whether the CPU is in halt bug state
    u16 current_rom_bank; // currently loaded ROM bank number
    u16 rom_bank_count; // total number of 16KB ROM banks
    const u8* current_rom_bank_ptr; // cached pointer to currently selected ROM bank
    u8 current_ram_bank; // currently loaded RAM bank number
    bool ram_enabled; // whether external RAM is enabled
    bool rom_banking; // whether in ROM banking mode
    std::array<u8, 5> rtc_registers; // RTC registers (MBC3)
    std::array<u8, 5> rtc_latched_registers; // Latched RTC snapshot
    u8 rtc_selected_register; // currently selected RTC register (0xFF = none)
    u8 rtc_latch_previous_value; // previous value written to latch register
    bool rtc_latch_active; // whether RTC data is latched
    u8 io_register_masks[256]; // which bits are always read as 1 in I/O registers
    std::vector<u8> memory; // 64KB addressable memory
    std::vector<u8> cartridge; // full cartridge content
    std::vector<u8> ram_banks; // external RAM banks (if any)
    std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT * 3> framebuffer_back; // 160x144 pixels, RGB format (back buffer)
    std::array<u8, SCREEN_WIDTH * SCREEN_HEIGHT * 3> framebuffer_front; // display buffer (front buffer)
    std::string header_title; // game title from ROM header
    std::string window_title; // window title string
    std::filesystem::path rom_path; // path to loaded ROM
    std::filesystem::path save_path; // path to battery-backed save file
    size_t ram_bank_size; // size in bytes of one external RAM bank
    size_t ram_bank_count; // number of external RAM banks
    bool cartridge_has_ram; // whether cartridge exposes external RAM
    bool cartridge_has_battery; // whether cartridge RAM is battery-backed
    bool ram_dirty; // whether RAM content has been modified since last save

    void* texture; // raylib texture for rendering

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
    void set_rom_bank(u16 bank);
    void set_ram_bank(u8 bank);
    void refresh_palette_cache(u8 index, u8 value);
    PPU_Color get_color(u16 palette_register, u8 color_id);
    void set_ppu_mode(u8 mode);
    void update_stat_coincidence_flag();
    void evaluate_sprites(u8 ly);
    void update_window_title(size_t measured_fps);
    bool render_scanline();

private:
    void initialize_memory();
    void initialize_io_masks();
    void load_cartridge(const std::string& path_rom);
    void extract_header_title();
    void initialize_cpu_state();
    void initialize_io_registers();
    void initialize_runtime_state();
    void initialize_opcode_tables();
    void load_save_ram();
    void save_save_ram();
};

inline u8 Gameboy::read8(u16 addr) const
{
    const u8* const mem = memory.data();

    if (addr < 0x4000) {
        return mem[addr];
    }
    if (addr < 0x8000) {
        return current_rom_bank_ptr[addr - 0x4000];
    }
    if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc_type == 3) {
            if (!ram_enabled) {
                return 0xFF;
            }
            if (rtc_selected_register <= 0x04) {
                return rtc_latch_active ? rtc_latched_registers[rtc_selected_register]
                                        : rtc_registers[rtc_selected_register];
            }
        }
        if (ram_banks.empty() || ram_bank_size == 0 || ram_bank_count == 0) {
            return 0xFF;
        }
        const size_t offset = addr - 0xA000;
        const size_t bank_offset = ram_bank_size ? (offset % ram_bank_size) : 0;
        const size_t bank_index = ram_bank_count ? (current_ram_bank % ram_bank_count) : 0;
        const size_t absolute_index = bank_index * ram_bank_size + bank_offset;
        if (absolute_index >= ram_banks.size()) {
            return 0xFF;
        }
        return ram_banks[absolute_index];
    }
    if (addr >= 0xFF00) {
        if (addr == 0xFF00) {
            u8 select = mem[0xFF00] & 0x30;
            u8 result = static_cast<u8>(0xC0 | select | 0x0F);
            if (!(select & 0x10)) {
                result = static_cast<u8>((result & 0xF0) | (joypad_state & 0x0F));
            }
            if (!(select & 0x20)) {
                result = static_cast<u8>((result & 0xF0) | ((joypad_state >> 4) & 0x0F));
            }
            return result;
        }
        return static_cast<u8>(mem[addr] | io_register_masks[addr - 0xFF00]);
    }
    return mem[addr];
}

inline u16 Gameboy::read16(u16 addr) const
{
    const u8 low = read8(addr);
    const u8 high = read8(static_cast<u16>(addr + 1));
    return static_cast<u16>((static_cast<u16>(high) << 8) | low);
}
