#include <algorithm>
#include <bit>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <limits>
#include <system_error>

#include "gameboy.h"
#include "opcodes.h"

#include "raylib.h"

namespace fs = std::filesystem;

Gameboy::Gameboy(const std::string& path_rom)
    : current_rom_bank_ptr(nullptr)
    , rom_path(path_rom)
    , save_path()
    , ram_bank_size(0)
    , ram_bank_count(0)
    , cartridge_has_ram(false)
    , cartridge_has_battery(false)
    , ram_dirty(false)
    , texture(nullptr)
{
    initialize_memory();
    initialize_io_masks();
    save_path = rom_path;
    if (!save_path.has_extension()) {
        save_path += ".sav";
    } else {
        save_path.replace_extension(".sav");
    }
    load_cartridge(path_rom);
    extract_header_title();
    initialize_cpu_state();
    initialize_io_registers();
    initialize_runtime_state();
    initialize_opcode_tables();
    init_graphics();
}

void Gameboy::initialize_memory()
{
    memory.resize(0x10000, 0);
    ram_banks.clear();
    ram_bank_size = 0;
    ram_bank_count = 0;
    set_ram_bank(0);
    current_rom_bank = 1;
    rom_bank_count = 0;
    ram_enabled = false;
    rom_banking = true;
    mbc_type = 0;
    rtc_registers.fill(0);
    rtc_latched_registers.fill(0);
    rtc_selected_register = 0xFF;
    rtc_latch_previous_value = 0xFF;
    rtc_latch_active = false;
    scanline_counter = 0; // counts cycles within current scanline
    scanline_sprite_count = 0;
    ppu_cycle = 0;
    ppu_mode = 0;
    window_line_counter = 0;
    scanline_rendered = false;
    cartridge_has_ram = false;
    cartridge_has_battery = false;
    ram_dirty = false;
}

void Gameboy::initialize_io_masks()
{
    std::fill(std::begin(io_register_masks), std::end(io_register_masks), 0x00);

    io_register_masks[0x03] = 0xFF; // Unmapped registers read as 0xFF

    for (int i = 0x08; i <= 0x0E; i++) {
        io_register_masks[i] = 0xFF;
    }

    io_register_masks[0x15] = 0xFF;
    io_register_masks[0x1F] = 0xFF;

    for (int i = 0x27; i <= 0x2F; i++) {
        io_register_masks[i] = 0xFF;
    }

    for (int i = 0x4C; i <= 0x50; i++) {
        io_register_masks[i] = 0xFF;
    }

    for (int i = 0x51; i <= 0x7F; i++) {
        io_register_masks[i] = 0xFF;
    }

    io_register_masks[0x02] = 0x7E; // SC: bits 1-6 unused
    io_register_masks[0x07] = 0xF8; // TAC: bits 3-7 unused
    io_register_masks[0x0F] = 0xE0; // IF: bits 5-7 unused
    io_register_masks[0x10] = 0x80; // NR10: bit 7 unused
    io_register_masks[0x1A] = 0x7F; // NR30: bits 0-6 unused
    io_register_masks[0x1C] = 0x9F; // NR32: bits 0-4,7 unused
    io_register_masks[0x20] = 0xC0; // NR41: bits 6-7 unused
    io_register_masks[0x23] = 0x3F; // NR44: bits 0-5 unused
    io_register_masks[0x26] = 0x70; // NR52: bits 4-6 unused
    io_register_masks[0x41] = 0x80; // STAT: bit 7 unused
}

void Gameboy::load_cartridge(const std::string& path_rom)
{
    std::ifstream file(path_rom, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open ROM file: " << path_rom << std::endl;
        exit(1);
    }

    cartridge = std::vector<u8>(std::istreambuf_iterator<char>(file), {});
    file.close();

    if (cartridge.size() < 0x8000) {
        std::cerr << "Invalid ROM file (too small): " << path_rom << std::endl;
        exit(1);
    }

    size_t bank_count = std::max<size_t>(1, (cartridge.size() + 0x3FFF) / 0x4000);
    if (bank_count > std::numeric_limits<u16>::max()) {
        bank_count = std::numeric_limits<u16>::max();
    }
    rom_bank_count = static_cast<u16>(bank_count);
    const size_t padded_size = static_cast<size_t>(rom_bank_count) * 0x4000;
    if (cartridge.size() < padded_size) {
        cartridge.resize(padded_size, 0xFF);
    }
    set_rom_bank(current_rom_bank);

    const u8 cartridge_type = cartridge[0x147];
    const u8 ram_size_code = cartridge[0x149];

    std::cout << "Cartridge type: 0x"
              << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cartridge_type)
              << std::dec << std::endl;

    cartridge_has_ram = false;
    cartridge_has_battery = false;

    switch (cartridge_type) {
    case 0x00:
        mbc_type = 0;
        break;
    case 0x01:
        mbc_type = 1;
        break;
    case 0x02:
        mbc_type = 1;
        cartridge_has_ram = true;
        break;
    case 0x03:
        mbc_type = 1;
        cartridge_has_ram = true;
        cartridge_has_battery = true;
        break;
    case 0x05:
        mbc_type = 2;
        break;
    case 0x06:
        mbc_type = 2;
        cartridge_has_battery = true;
        break;
    case 0x0F:
        mbc_type = 3;
        break;
    case 0x10:
        mbc_type = 3;
        cartridge_has_ram = true;
        cartridge_has_battery = true;
        break;
    case 0x11:
        mbc_type = 3;
        break;
    case 0x12:
        mbc_type = 3;
        cartridge_has_ram = true;
        break;
    case 0x13:
        mbc_type = 3;
        cartridge_has_ram = true;
        cartridge_has_battery = true;
        break;
    default:
        std::cerr << "Unsupported MBC type in ROM header: 0x"
                  << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cartridge_type)
                  << std::dec << std::endl;
        exit(1);
    }

    // MBC2 always has 512x4-bit internal RAM despite header reporting 0
    if (mbc_type == 2) {
        cartridge_has_ram = true;
    }

    if (ram_size_code != 0) {
        cartridge_has_ram = true;
    }

    switch (ram_size_code) {
    case 0x00:
        ram_bank_size = 0;
        ram_bank_count = 0;
        break;
    case 0x01:
        ram_bank_size = 0x800; // 2KB
        ram_bank_count = 1;
        break;
    case 0x02:
        ram_bank_size = 0x2000; // 8KB
        ram_bank_count = 1;
        break;
    case 0x03:
        ram_bank_size = 0x2000; // 4 x 8KB = 32KB
        ram_bank_count = 4;
        break;
    case 0x04:
        ram_bank_size = 0x2000; // 16 x 8KB = 128KB
        ram_bank_count = 16;
        break;
    case 0x05:
        ram_bank_size = 0x2000; // 8 x 8KB = 64KB
        ram_bank_count = 8;
        break;
    default:
        std::cerr << "Unsupported RAM size code in ROM header: 0x"
                  << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ram_size_code)
                  << std::dec << std::endl;
        exit(1);
    }

    if (mbc_type == 2) {
        ram_bank_size = 0x200; // 512 bytes mirrored across address space
        ram_bank_count = 1;
    }

    if (cartridge_has_ram && ram_bank_size == 0 && mbc_type != 2) {
        ram_bank_size = 0x2000;
        ram_bank_count = 1;
    }

    if (!cartridge_has_ram || ram_bank_size == 0 || ram_bank_count == 0) {
        ram_banks.clear();
    } else {
        ram_banks.assign(ram_bank_size * ram_bank_count, 0);
    }
    set_ram_bank(0);
    ram_dirty = false;

    const size_t first_chunk = std::min(cartridge.size(), size_t(0x8000));
    std::copy(cartridge.begin(), cartridge.begin() + first_chunk, memory.begin());
    if (first_chunk < 0x8000) {
        std::fill(memory.begin() + first_chunk, memory.begin() + 0x8000, 0xFF);
    }

    load_save_ram();
}

void Gameboy::load_save_ram()
{
    if (!cartridge_has_ram || !cartridge_has_battery || ram_banks.empty()) {
        return;
    }

    std::error_code ec;
    if (!fs::exists(save_path, ec)) {
        if (ec) {
            std::cerr << "Failed to access save file: " << save_path << " (" << ec.message() << ")" << std::endl;
        }
        return;
    }

    const auto expected_size = ram_banks.size();
    ec.clear();
    const auto actual_size = fs::file_size(save_path, ec);
    bool file_size_mismatch = false;
    if (ec) {
        std::cerr << "Failed to query save file size: " << save_path << " (" << ec.message() << ")" << std::endl;
    } else if (actual_size != expected_size) {
        file_size_mismatch = true;
        std::cout << "Save file size (" << actual_size << " bytes) does not match expected RAM size (" << expected_size
                  << " bytes); loading as much as possible." << std::endl;
    }

    std::ifstream file(save_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open save file for reading: " << save_path << std::endl;
        return;
    }

    file.read(reinterpret_cast<char*>(ram_banks.data()), static_cast<std::streamsize>(ram_banks.size()));
    const std::streamsize read_bytes = file.gcount();
    const size_t copied_bytes = read_bytes < 0 ? 0 : static_cast<size_t>(read_bytes);
    if (copied_bytes < ram_banks.size()) {
        std::fill(ram_banks.begin() + copied_bytes, ram_banks.end(), 0);
    }

    if (!file && !file.eof()) {
        std::cerr << "Failed while reading save file: " << save_path << std::endl;
        ram_dirty = true;
        return;
    }

    const bool partial_read = copied_bytes != expected_size;
    ram_dirty = file_size_mismatch || partial_read;

    if (ram_dirty) {
        std::cout << "Loaded save file: " << save_path << " (will rewrite to expected size)" << std::endl;
    } else {
        std::cout << "Loaded save file: " << save_path << std::endl;
    }
}

void Gameboy::save_save_ram()
{
    if (!cartridge_has_ram || !cartridge_has_battery || ram_banks.empty()) {
        return;
    }

    if (!ram_dirty) {
        return;
    }

    std::error_code ec;
    fs::path parent = save_path.parent_path();
    if (!parent.empty()) {
        if (!fs::exists(parent, ec)) {
            if (ec) {
                std::cerr << "Failed to access save directory: " << parent << " (" << ec.message() << ")" << std::endl;
                return;
            }
            ec.clear();
            if (!fs::create_directories(parent, ec)) {
                if (ec) {
                    std::cerr << "Failed to create save directory: " << parent << " (" << ec.message() << ")" << std::endl;
                    return;
                }
            }
        } else if (ec) {
            std::cerr << "Failed to access save directory: " << parent << " (" << ec.message() << ")" << std::endl;
            return;
        }
    }

    std::ofstream file(save_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        std::cerr << "Failed to open save file for writing: " << save_path << std::endl;
        return;
    }

    file.write(reinterpret_cast<const char*>(ram_banks.data()), static_cast<std::streamsize>(ram_banks.size()));
    if (!file) {
        std::cerr << "Failed to write save file: " << save_path << std::endl;
        return;
    }

    file.flush();
    if (!file) {
        std::cerr << "Failed to flush save file: " << save_path << std::endl;
        return;
    }

    ram_dirty = false;
    std::cout << "Saved save file: " << save_path << std::endl;
}

void Gameboy::extract_header_title()
{
    header_title.clear();
    for (int address = 0x134; address <= 0x143; ++address) {
        char c = static_cast<char>(read8(address));
        if (c == 0) {
            break;
        }
        header_title.push_back(c);
    }
}

void Gameboy::initialize_cpu_state()
{
    AF = 0x01B0;
    BC = 0x0013;
    DE = 0x00D8;
    HL = 0x014D;
    SP = 0xFFFE;
    PC = 0x0100;
}

void Gameboy::initialize_io_registers()
{
    memory[0xFF00] = 0xCF;
    memory[0xFF01] = 0x00;
    memory[0xFF02] = 0x7E;
    memory[0xFF04] = 0xAB;
    memory[0xFF05] = 0x00;
    memory[0xFF06] = 0x00;
    memory[0xFF07] = 0xF8;
    memory[0xFF0F] = 0xE1;
    memory[0xFF10] = 0x80;
    memory[0xFF11] = 0xBF;
    memory[0xFF12] = 0xF3;
    memory[0xFF13] = 0xFF;
    memory[0xFF14] = 0xBF;
    memory[0xFF16] = 0x3F;
    memory[0xFF17] = 0x00;
    memory[0xFF18] = 0xFF;
    memory[0xFF19] = 0xBF;
    memory[0xFF1A] = 0x7F;
    memory[0xFF1B] = 0xFF;
    memory[0xFF1C] = 0x9F;
    memory[0xFF1D] = 0xFF;
    memory[0xFF1E] = 0xBF;
    memory[0xFF20] = 0xFF;
    memory[0xFF21] = 0x00;
    memory[0xFF22] = 0x00;
    memory[0xFF23] = 0xBF;
    memory[0xFF24] = 0x77;
    memory[0xFF25] = 0xF3;
    memory[0xFF26] = 0xF1;
    memory[0xFF40] = 0x91;
    memory[0xFF41] = 0x85;
    memory[0xFF42] = 0x00;
    memory[0xFF43] = 0x00;
    memory[0xFF44] = 0x00;
    memory[0xFF45] = 0x00;
    memory[0xFF46] = 0xFF;
    memory[0xFF47] = 0xFC;
    memory[0xFF48] = 0x00;
    memory[0xFF49] = 0x00;
    memory[0xFF4A] = 0x00;
    memory[0xFF4B] = 0x00;
    memory[0xFFFF] = 0x00;

    refresh_palette_cache(0, memory[0xFF47]);
    refresh_palette_cache(1, memory[0xFF48]);
    refresh_palette_cache(2, memory[0xFF49]);

    ppu_mode = memory[0xFF41] & 0x03;
    update_stat_coincidence_flag();
}

void Gameboy::initialize_runtime_state()
{
    ime = false;
    ime_scheduled = false;
    halted = false;
    halt_bug = false;
    joypad_state = 0xFF; // all buttons unpressed
    timer_counter = 1024; // CLOCKSPEED / frequency (4096 Hz default)
    divider_counter = 0; // DIV increments at 16384 Hz
}

void Gameboy::initialize_opcode_tables()
{
    // init opcode table

    for (int i = 0; i < 256; i++) {
        opcodes[i] = op_unimplemented;
        cb_opcodes[i] = op_unimplemented;
    }

    opcodes[0x00] = op_0x00_NOP;
    opcodes[0x01] = op_0x01_LD_BC_u16;
    opcodes[0x02] = op_0x02_LD_BC_A;
    opcodes[0x03] = op_0x03_INC_BC;
    opcodes[0x04] = op_0x04_INC_B;
    opcodes[0x05] = op_0x05_DEC_B;
    opcodes[0x06] = op_0x06_LD_B_u8;
    opcodes[0x07] = op_0x07_RLCA;
    opcodes[0x08] = op_0x08_LD_u16_SP;
    opcodes[0x09] = op_0x09_ADD_HL_BC;
    opcodes[0x0A] = op_0x0A_LD_A_BC;
    opcodes[0x0B] = op_0x0B_DEC_BC;
    opcodes[0x0C] = op_0x0C_INC_C;
    opcodes[0x0D] = op_0x0D_DEC_C;
    opcodes[0x0E] = op_0x0E_LD_C_u8;
    opcodes[0x0F] = op_0x0F_RRCA;
    opcodes[0x10] = op_0x10_STOP;
    opcodes[0x11] = op_0x11_LD_DE_u16;
    opcodes[0x12] = op_0x12_LD_DE_A;
    opcodes[0x13] = op_0x13_INC_DE;
    opcodes[0x14] = op_0x14_INC_D;
    opcodes[0x15] = op_0x15_DEC_D;
    opcodes[0x16] = op_0x16_LD_D_u8;
    opcodes[0x17] = op_0x17_RLA;
    opcodes[0x18] = op_0x18_JR_i8;
    opcodes[0x19] = op_0x19_ADD_HL_DE;
    opcodes[0x1A] = op_0x1A_LD_A_DE;
    opcodes[0x1B] = op_0x1B_DEC_DE;
    opcodes[0x1C] = op_0x1C_INC_E;
    opcodes[0x1D] = op_0x1D_DEC_E;
    opcodes[0x1E] = op_0x1E_LD_E_u8;
    opcodes[0x1F] = op_0x1F_RRA;
    opcodes[0x20] = op_0x20_JR_NZ_i8;
    opcodes[0x21] = op_0x21_LD_HL_u16;
    opcodes[0x22] = op_0x22_LD_HLp_A;
    opcodes[0x23] = op_0x23_INC_HL;
    opcodes[0x24] = op_0x24_INC_H;
    opcodes[0x25] = op_0x25_DEC_H;
    opcodes[0x26] = op_0x26_LD_H_u8;
    opcodes[0x27] = op_0x27_DAA;
    opcodes[0x28] = op_0x28_JR_Z_i8;
    opcodes[0x29] = op_0x29_ADD_HL_HL;
    opcodes[0x2A] = op_0x2A_LD_A_HLp;
    opcodes[0x2B] = op_0x2B_DEC_HL;
    opcodes[0x2C] = op_0x2C_INC_L;
    opcodes[0x2D] = op_0x2D_DEC_L;
    opcodes[0x2E] = op_0x2E_LD_L_u8;
    opcodes[0x2F] = op_0x2F_CPL;
    opcodes[0x30] = op_0x30_JR_NC_i8;
    opcodes[0x31] = op_0x31_LD_SP_u16;
    opcodes[0x32] = op_0x32_LD_HLm_A;
    opcodes[0x33] = op_0x33_INC_SP;
    opcodes[0x34] = op_0x34_INC_HL;
    opcodes[0x35] = op_0x35_DEC_HL;
    opcodes[0x36] = op_0x36_LD_HL_u8;
    opcodes[0x37] = op_0x37_SCF;
    opcodes[0x38] = op_0x38_JR_C_i8;
    opcodes[0x39] = op_0x39_ADD_HL_SP;
    opcodes[0x3A] = op_0x3A_LD_A_HLm;
    opcodes[0x3B] = op_0x3B_DEC_SP;
    opcodes[0x3C] = op_0x3C_INC_A;
    opcodes[0x3D] = op_0x3D_DEC_A;
    opcodes[0x3E] = op_0x3E_LD_A_u8;
    opcodes[0x3F] = op_0x3F_CCF;
    opcodes[0x40] = op_0x40_LD_B_B;
    opcodes[0x41] = op_0x41_LD_B_C;
    opcodes[0x42] = op_0x42_LD_B_D;
    opcodes[0x43] = op_0x43_LD_B_E;
    opcodes[0x44] = op_0x44_LD_B_H;
    opcodes[0x45] = op_0x45_LD_B_L;
    opcodes[0x46] = op_0x46_LD_B_HL;
    opcodes[0x47] = op_0x47_LD_B_A;
    opcodes[0x48] = op_0x48_LD_C_B;
    opcodes[0x49] = op_0x49_LD_C_C;
    opcodes[0x4A] = op_0x4A_LD_C_D;
    opcodes[0x4B] = op_0x4B_LD_C_E;
    opcodes[0x4C] = op_0x4C_LD_C_H;
    opcodes[0x4D] = op_0x4D_LD_C_L;
    opcodes[0x4E] = op_0x4E_LD_C_HL;
    opcodes[0x4F] = op_0x4F_LD_C_A;
    opcodes[0x50] = op_0x50_LD_D_B;
    opcodes[0x51] = op_0x51_LD_D_C;
    opcodes[0x52] = op_0x52_LD_D_D;
    opcodes[0x53] = op_0x53_LD_D_E;
    opcodes[0x54] = op_0x54_LD_D_H;
    opcodes[0x55] = op_0x55_LD_D_L;
    opcodes[0x56] = op_0x56_LD_D_HL;
    opcodes[0x57] = op_0x57_LD_D_A;
    opcodes[0x58] = op_0x58_LD_E_B;
    opcodes[0x59] = op_0x59_LD_E_C;
    opcodes[0x5A] = op_0x5A_LD_E_D;
    opcodes[0x5B] = op_0x5B_LD_E_E;
    opcodes[0x5C] = op_0x5C_LD_E_H;
    opcodes[0x5D] = op_0x5D_LD_E_L;
    opcodes[0x5E] = op_0x5E_LD_E_HL;
    opcodes[0x5F] = op_0x5F_LD_E_A;
    opcodes[0x60] = op_0x60_LD_H_B;
    opcodes[0x61] = op_0x61_LD_H_C;
    opcodes[0x62] = op_0x62_LD_H_D;
    opcodes[0x63] = op_0x63_LD_H_E;
    opcodes[0x64] = op_0x64_LD_H_H;
    opcodes[0x65] = op_0x65_LD_H_L;
    opcodes[0x66] = op_0x66_LD_H_HL;
    opcodes[0x67] = op_0x67_LD_H_A;
    opcodes[0x68] = op_0x68_LD_L_B;
    opcodes[0x69] = op_0x69_LD_L_C;
    opcodes[0x6A] = op_0x6A_LD_L_D;
    opcodes[0x6B] = op_0x6B_LD_L_E;
    opcodes[0x6C] = op_0x6C_LD_L_H;
    opcodes[0x6D] = op_0x6D_LD_L_L;
    opcodes[0x6E] = op_0x6E_LD_L_HL;
    opcodes[0x6F] = op_0x6F_LD_L_A;
    opcodes[0x70] = op_0x70_LD_HL_B;
    opcodes[0x71] = op_0x71_LD_HL_C;
    opcodes[0x72] = op_0x72_LD_HL_D;
    opcodes[0x73] = op_0x73_LD_HL_E;
    opcodes[0x74] = op_0x74_LD_HL_H;
    opcodes[0x75] = op_0x75_LD_HL_L;
    opcodes[0x76] = op_0x76_HALT;
    opcodes[0x77] = op_0x77_LD_HL_A;
    opcodes[0x78] = op_0x78_LD_A_B;
    opcodes[0x79] = op_0x79_LD_A_C;
    opcodes[0x7A] = op_0x7A_LD_A_D;
    opcodes[0x7B] = op_0x7B_LD_A_E;
    opcodes[0x7C] = op_0x7C_LD_A_H;
    opcodes[0x7D] = op_0x7D_LD_A_L;
    opcodes[0x7E] = op_0x7E_LD_A_HL;
    opcodes[0x7F] = op_0x7F_LD_A_A;
    opcodes[0x80] = op_0x80_ADD_A_B;
    opcodes[0x81] = op_0x81_ADD_A_C;
    opcodes[0x82] = op_0x82_ADD_A_D;
    opcodes[0x83] = op_0x83_ADD_A_E;
    opcodes[0x84] = op_0x84_ADD_A_H;
    opcodes[0x85] = op_0x85_ADD_A_L;
    opcodes[0x86] = op_0x86_ADD_A_HL;
    opcodes[0x87] = op_0x87_ADD_A_A;
    opcodes[0x88] = op_0x88_ADC_A_B;
    opcodes[0x89] = op_0x89_ADC_A_C;
    opcodes[0x8A] = op_0x8A_ADC_A_D;
    opcodes[0x8B] = op_0x8B_ADC_A_E;
    opcodes[0x8C] = op_0x8C_ADC_A_H;
    opcodes[0x8D] = op_0x8D_ADC_A_L;
    opcodes[0x8E] = op_0x8E_ADC_A_HL;
    opcodes[0x8F] = op_0x8F_ADC_A_A;
    opcodes[0x90] = op_0x90_SUB_A_B;
    opcodes[0x91] = op_0x91_SUB_A_C;
    opcodes[0x92] = op_0x92_SUB_A_D;
    opcodes[0x93] = op_0x93_SUB_A_E;
    opcodes[0x94] = op_0x94_SUB_A_H;
    opcodes[0x95] = op_0x95_SUB_A_L;
    opcodes[0x96] = op_0x96_SUB_A_HL;
    opcodes[0x97] = op_0x97_SUB_A_A;
    opcodes[0x98] = op_0x98_SBC_A_B;
    opcodes[0x99] = op_0x99_SBC_A_C;
    opcodes[0x9A] = op_0x9A_SBC_A_D;
    opcodes[0x9B] = op_0x9B_SBC_A_E;
    opcodes[0x9C] = op_0x9C_SBC_A_H;
    opcodes[0x9D] = op_0x9D_SBC_A_L;
    opcodes[0x9E] = op_0x9E_SBC_A_HL;
    opcodes[0x9F] = op_0x9F_SBC_A_A;
    opcodes[0xA0] = op_0xA0_AND_A_B;
    opcodes[0xA1] = op_0xA1_AND_A_C;
    opcodes[0xA2] = op_0xA2_AND_A_D;
    opcodes[0xA3] = op_0xA3_AND_A_E;
    opcodes[0xA4] = op_0xA4_AND_A_H;
    opcodes[0xA5] = op_0xA5_AND_A_L;
    opcodes[0xA6] = op_0xA6_AND_A_HL;
    opcodes[0xA7] = op_0xA7_AND_A_A;
    opcodes[0xA8] = op_0xA8_XOR_A_B;
    opcodes[0xA9] = op_0xA9_XOR_A_C;
    opcodes[0xAA] = op_0xAA_XOR_A_D;
    opcodes[0xAB] = op_0xAB_XOR_A_E;
    opcodes[0xAC] = op_0xAC_XOR_A_H;
    opcodes[0xAD] = op_0xAD_XOR_A_L;
    opcodes[0xAE] = op_0xAE_XOR_A_HL;
    opcodes[0xAF] = op_0xAF_XOR_A_A;
    opcodes[0xB0] = op_0xB0_OR_A_B;
    opcodes[0xB1] = op_0xB1_OR_A_C;
    opcodes[0xB2] = op_0xB2_OR_A_D;
    opcodes[0xB3] = op_0xB3_OR_A_E;
    opcodes[0xB4] = op_0xB4_OR_A_H;
    opcodes[0xB5] = op_0xB5_OR_A_L;
    opcodes[0xB6] = op_0xB6_OR_A_HL;
    opcodes[0xB7] = op_0xB7_OR_A_A;
    opcodes[0xB8] = op_0xB8_CP_A_B;
    opcodes[0xB9] = op_0xB9_CP_A_C;
    opcodes[0xBA] = op_0xBA_CP_A_D;
    opcodes[0xBB] = op_0xBB_CP_A_E;
    opcodes[0xBC] = op_0xBC_CP_A_H;
    opcodes[0xBD] = op_0xBD_CP_A_L;
    opcodes[0xBE] = op_0xBE_CP_A_HL;
    opcodes[0xBF] = op_0xBF_CP_A_A;
    opcodes[0xC0] = op_0xC0_RET_NZ;
    opcodes[0xC1] = op_0xC1_POP_BC;
    opcodes[0xC2] = op_0xC2_JP_NZ_u16;
    opcodes[0xC3] = op_0xC3_JP_u16;
    opcodes[0xC4] = op_0xC4_CALL_NZ_u16;
    opcodes[0xC5] = op_0xC5_PUSH_BC;
    opcodes[0xC6] = op_0xC6_ADD_A_u8;
    opcodes[0xC7] = op_0xC7_RST_00h;
    opcodes[0xC8] = op_0xC8_RET_Z;
    opcodes[0xC9] = op_0xC9_RET;
    opcodes[0xCA] = op_0xCA_JP_Z_u16;
    opcodes[0xCB] = op_0xCB_prefixed;
    opcodes[0xCC] = op_0xCC_CALL_Z_u16;
    opcodes[0xCD] = op_0xCD_CALL_u16;
    opcodes[0xCE] = op_0xCE_ADC_A_u8;
    opcodes[0xCF] = op_0xCF_RST_08h;
    opcodes[0xD0] = op_0xD0_RET_NC;
    opcodes[0xD1] = op_0xD1_POP_DE;
    opcodes[0xD2] = op_0xD2_JP_NC_u16;
    opcodes[0xD4] = op_0xD4_CALL_NC_u16;
    opcodes[0xD5] = op_0xD5_PUSH_DE;
    opcodes[0xD6] = op_0xD6_SUB_A_u8;
    opcodes[0xD7] = op_0xD7_RST_10h;
    opcodes[0xD8] = op_0xD8_RET_C;
    opcodes[0xD9] = op_0xD9_RETI;
    opcodes[0xDA] = op_0xDA_JP_C_u16;
    opcodes[0xDC] = op_0xDC_CALL_C_u16;
    opcodes[0xDE] = op_0xDE_SBC_A_u8;
    opcodes[0xDF] = op_0xDF_RST_18h;
    opcodes[0xE0] = op_0xE0_LD_u8_A;
    opcodes[0xE1] = op_0xE1_POP_HL;
    opcodes[0xE2] = op_0xE2_LD_C_A;
    opcodes[0xE5] = op_0xE5_PUSH_HL;
    opcodes[0xE6] = op_0xE6_AND_A_u8;
    opcodes[0xE7] = op_0xE7_RST_20h;
    opcodes[0xE8] = op_0xE8_ADD_SP_i8;
    opcodes[0xE9] = op_0xE9_JP_HL;
    opcodes[0xEA] = op_0xEA_LD_u16_A;
    opcodes[0xEE] = op_0xEE_XOR_A_u8;
    opcodes[0xEF] = op_0xEF_RST_28h;
    opcodes[0xF0] = op_0xF0_LD_A_FF00_u8;
    opcodes[0xF1] = op_0xF1_POP_AF;
    opcodes[0xF2] = op_0xF2_LD_A_FF00_C;
    opcodes[0xF3] = op_0xF3_DI;
    opcodes[0xF5] = op_0xF5_PUSH_AF;
    opcodes[0xF6] = op_0xF6_OR_A_u8;
    opcodes[0xF7] = op_0xF7_RST_30h;
    opcodes[0xF8] = op_0xF8_LD_HL_SP_i8;
    opcodes[0xF9] = op_0xF9_LD_SP_HL;
    opcodes[0xFA] = op_0xFA_LD_A_u16;
    opcodes[0xFB] = op_0xFB_EI;
    opcodes[0xFE] = op_0xFE_CP_A_u8;
    opcodes[0xFF] = op_0xFF_RST_38h;

    cb_opcodes[0x00] = op_0xCB_0x00_RLC_B;
    cb_opcodes[0x01] = op_0xCB_0x01_RLC_C;
    cb_opcodes[0x02] = op_0xCB_0x02_RLC_D;
    cb_opcodes[0x03] = op_0xCB_0x03_RLC_E;
    cb_opcodes[0x04] = op_0xCB_0x04_RLC_H;
    cb_opcodes[0x05] = op_0xCB_0x05_RLC_L;
    cb_opcodes[0x06] = op_0xCB_0x06_RLC_HL;
    cb_opcodes[0x07] = op_0xCB_0x07_RLC_A;
    cb_opcodes[0x08] = op_0xCB_0x08_RRC_B;
    cb_opcodes[0x09] = op_0xCB_0x09_RRC_C;
    cb_opcodes[0x0A] = op_0xCB_0x0A_RRC_D;
    cb_opcodes[0x0B] = op_0xCB_0x0B_RRC_E;
    cb_opcodes[0x0C] = op_0xCB_0x0C_RRC_H;
    cb_opcodes[0x0D] = op_0xCB_0x0D_RRC_L;
    cb_opcodes[0x0E] = op_0xCB_0x0E_RRC_HL;
    cb_opcodes[0x0F] = op_0xCB_0x0F_RRC_A;
    cb_opcodes[0x10] = op_0xCB_0x10_RL_B;
    cb_opcodes[0x11] = op_0xCB_0x11_RL_C;
    cb_opcodes[0x12] = op_0xCB_0x12_RL_D;
    cb_opcodes[0x13] = op_0xCB_0x13_RL_E;
    cb_opcodes[0x14] = op_0xCB_0x14_RL_H;
    cb_opcodes[0x15] = op_0xCB_0x15_RL_L;
    cb_opcodes[0x16] = op_0xCB_0x16_RL_HL;
    cb_opcodes[0x17] = op_0xCB_0x17_RL_A;
    cb_opcodes[0x18] = op_0xCB_0x18_RR_B;
    cb_opcodes[0x19] = op_0xCB_0x19_RR_C;
    cb_opcodes[0x1A] = op_0xCB_0x1A_RR_D;
    cb_opcodes[0x1B] = op_0xCB_0x1B_RR_E;
    cb_opcodes[0x1C] = op_0xCB_0x1C_RR_H;
    cb_opcodes[0x1D] = op_0xCB_0x1D_RR_L;
    cb_opcodes[0x1E] = op_0xCB_0x1E_RR_HL;
    cb_opcodes[0x1F] = op_0xCB_0x1F_RR_A;
    cb_opcodes[0x20] = op_0xCB_0x20_SLA_B;
    cb_opcodes[0x21] = op_0xCB_0x21_SLA_C;
    cb_opcodes[0x22] = op_0xCB_0x22_SLA_D;
    cb_opcodes[0x23] = op_0xCB_0x23_SLA_E;
    cb_opcodes[0x24] = op_0xCB_0x24_SLA_H;
    cb_opcodes[0x25] = op_0xCB_0x25_SLA_L;
    cb_opcodes[0x26] = op_0xCB_0x26_SLA_HL;
    cb_opcodes[0x27] = op_0xCB_0x27_SLA_A;
    cb_opcodes[0x28] = op_0xCB_0x28_SRA_B;
    cb_opcodes[0x29] = op_0xCB_0x29_SRA_C;
    cb_opcodes[0x2A] = op_0xCB_0x2A_SRA_D;
    cb_opcodes[0x2B] = op_0xCB_0x2B_SRA_E;
    cb_opcodes[0x2C] = op_0xCB_0x2C_SRA_H;
    cb_opcodes[0x2D] = op_0xCB_0x2D_SRA_L;
    cb_opcodes[0x2E] = op_0xCB_0x2E_SRA_HL;
    cb_opcodes[0x2F] = op_0xCB_0x2F_SRA_A;
    cb_opcodes[0x30] = op_0xCB_0x30_SWAP_B;
    cb_opcodes[0x31] = op_0xCB_0x31_SWAP_C;
    cb_opcodes[0x32] = op_0xCB_0x32_SWAP_D;
    cb_opcodes[0x33] = op_0xCB_0x33_SWAP_E;
    cb_opcodes[0x34] = op_0xCB_0x34_SWAP_H;
    cb_opcodes[0x35] = op_0xCB_0x35_SWAP_L;
    cb_opcodes[0x36] = op_0xCB_0x36_SWAP_HL;
    cb_opcodes[0x37] = op_0xCB_0x37_SWAP_A;
    cb_opcodes[0x38] = op_0xCB_0x38_SRL_B;
    cb_opcodes[0x39] = op_0xCB_0x39_SRL_C;
    cb_opcodes[0x3A] = op_0xCB_0x3A_SRL_D;
    cb_opcodes[0x3B] = op_0xCB_0x3B_SRL_E;
    cb_opcodes[0x3C] = op_0xCB_0x3C_SRL_H;
    cb_opcodes[0x3D] = op_0xCB_0x3D_SRL_L;
    cb_opcodes[0x3E] = op_0xCB_0x3E_SRL_HL;
    cb_opcodes[0x3F] = op_0xCB_0x3F_SRL_A;
    cb_opcodes[0x40] = op_0xCB_0x40_BIT_0_B;
    cb_opcodes[0x41] = op_0xCB_0x41_BIT_0_C;
    cb_opcodes[0x42] = op_0xCB_0x42_BIT_0_D;
    cb_opcodes[0x43] = op_0xCB_0x43_BIT_0_E;
    cb_opcodes[0x44] = op_0xCB_0x44_BIT_0_H;
    cb_opcodes[0x45] = op_0xCB_0x45_BIT_0_L;
    cb_opcodes[0x46] = op_0xCB_0x46_BIT_0_HL;
    cb_opcodes[0x47] = op_0xCB_0x47_BIT_0_A;
    cb_opcodes[0x48] = op_0xCB_0x48_BIT_1_B;
    cb_opcodes[0x49] = op_0xCB_0x49_BIT_1_C;
    cb_opcodes[0x4A] = op_0xCB_0x4A_BIT_1_D;
    cb_opcodes[0x4B] = op_0xCB_0x4B_BIT_1_E;
    cb_opcodes[0x4C] = op_0xCB_0x4C_BIT_1_H;
    cb_opcodes[0x4D] = op_0xCB_0x4D_BIT_1_L;
    cb_opcodes[0x4E] = op_0xCB_0x4E_BIT_1_HL;
    cb_opcodes[0x4F] = op_0xCB_0x4F_BIT_1_A;
    cb_opcodes[0x50] = op_0xCB_0x50_BIT_2_B;
    cb_opcodes[0x51] = op_0xCB_0x51_BIT_2_C;
    cb_opcodes[0x52] = op_0xCB_0x52_BIT_2_D;
    cb_opcodes[0x53] = op_0xCB_0x53_BIT_2_E;
    cb_opcodes[0x54] = op_0xCB_0x54_BIT_2_H;
    cb_opcodes[0x55] = op_0xCB_0x55_BIT_2_L;
    cb_opcodes[0x56] = op_0xCB_0x56_BIT_2_HL;
    cb_opcodes[0x57] = op_0xCB_0x57_BIT_2_A;
    cb_opcodes[0x58] = op_0xCB_0x58_BIT_3_B;
    cb_opcodes[0x59] = op_0xCB_0x59_BIT_3_C;
    cb_opcodes[0x5A] = op_0xCB_0x5A_BIT_3_D;
    cb_opcodes[0x5B] = op_0xCB_0x5B_BIT_3_E;
    cb_opcodes[0x5C] = op_0xCB_0x5C_BIT_3_H;
    cb_opcodes[0x5D] = op_0xCB_0x5D_BIT_3_L;
    cb_opcodes[0x5E] = op_0xCB_0x5E_BIT_3_HL;
    cb_opcodes[0x5F] = op_0xCB_0x5F_BIT_3_A;
    cb_opcodes[0x60] = op_0xCB_0x60_BIT_4_B;
    cb_opcodes[0x61] = op_0xCB_0x61_BIT_4_C;
    cb_opcodes[0x62] = op_0xCB_0x62_BIT_4_D;
    cb_opcodes[0x63] = op_0xCB_0x63_BIT_4_E;
    cb_opcodes[0x64] = op_0xCB_0x64_BIT_4_H;
    cb_opcodes[0x65] = op_0xCB_0x65_BIT_4_L;
    cb_opcodes[0x66] = op_0xCB_0x66_BIT_4_HL;
    cb_opcodes[0x67] = op_0xCB_0x67_BIT_4_A;
    cb_opcodes[0x68] = op_0xCB_0x68_BIT_5_B;
    cb_opcodes[0x69] = op_0xCB_0x69_BIT_5_C;
    cb_opcodes[0x6A] = op_0xCB_0x6A_BIT_5_D;
    cb_opcodes[0x6B] = op_0xCB_0x6B_BIT_5_E;
    cb_opcodes[0x6C] = op_0xCB_0x6C_BIT_5_H;
    cb_opcodes[0x6D] = op_0xCB_0x6D_BIT_5_L;
    cb_opcodes[0x6E] = op_0xCB_0x6E_BIT_5_HL;
    cb_opcodes[0x6F] = op_0xCB_0x6F_BIT_5_A;
    cb_opcodes[0x70] = op_0xCB_0x70_BIT_6_B;
    cb_opcodes[0x71] = op_0xCB_0x71_BIT_6_C;
    cb_opcodes[0x72] = op_0xCB_0x72_BIT_6_D;
    cb_opcodes[0x73] = op_0xCB_0x73_BIT_6_E;
    cb_opcodes[0x74] = op_0xCB_0x74_BIT_6_H;
    cb_opcodes[0x75] = op_0xCB_0x75_BIT_6_L;
    cb_opcodes[0x76] = op_0xCB_0x76_BIT_6_HL;
    cb_opcodes[0x77] = op_0xCB_0x77_BIT_6_A;
    cb_opcodes[0x78] = op_0xCB_0x78_BIT_7_B;
    cb_opcodes[0x79] = op_0xCB_0x79_BIT_7_C;
    cb_opcodes[0x7A] = op_0xCB_0x7A_BIT_7_D;
    cb_opcodes[0x7B] = op_0xCB_0x7B_BIT_7_E;
    cb_opcodes[0x7C] = op_0xCB_0x7C_BIT_7_H;
    cb_opcodes[0x7D] = op_0xCB_0x7D_BIT_7_L;
    cb_opcodes[0x7E] = op_0xCB_0x7E_BIT_7_HL;
    cb_opcodes[0x7F] = op_0xCB_0x7F_BIT_7_A;
    cb_opcodes[0x80] = op_0xCB_0x80_RES_0_B;
    cb_opcodes[0x81] = op_0xCB_0x81_RES_0_C;
    cb_opcodes[0x82] = op_0xCB_0x82_RES_0_D;
    cb_opcodes[0x83] = op_0xCB_0x83_RES_0_E;
    cb_opcodes[0x84] = op_0xCB_0x84_RES_0_H;
    cb_opcodes[0x85] = op_0xCB_0x85_RES_0_L;
    cb_opcodes[0x86] = op_0xCB_0x86_RES_0_HL;
    cb_opcodes[0x87] = op_0xCB_0x87_RES_0_A;
    cb_opcodes[0x88] = op_0xCB_0x88_RES_1_B;
    cb_opcodes[0x89] = op_0xCB_0x89_RES_1_C;
    cb_opcodes[0x8A] = op_0xCB_0x8A_RES_1_D;
    cb_opcodes[0x8B] = op_0xCB_0x8B_RES_1_E;
    cb_opcodes[0x8C] = op_0xCB_0x8C_RES_1_H;
    cb_opcodes[0x8D] = op_0xCB_0x8D_RES_1_L;
    cb_opcodes[0x8E] = op_0xCB_0x8E_RES_1_HL;
    cb_opcodes[0x8F] = op_0xCB_0x8F_RES_1_A;
    cb_opcodes[0x90] = op_0xCB_0x90_RES_2_B;
    cb_opcodes[0x91] = op_0xCB_0x91_RES_2_C;
    cb_opcodes[0x92] = op_0xCB_0x92_RES_2_D;
    cb_opcodes[0x93] = op_0xCB_0x93_RES_2_E;
    cb_opcodes[0x94] = op_0xCB_0x94_RES_2_H;
    cb_opcodes[0x95] = op_0xCB_0x95_RES_2_L;
    cb_opcodes[0x96] = op_0xCB_0x96_RES_2_HL;
    cb_opcodes[0x97] = op_0xCB_0x97_RES_2_A;
    cb_opcodes[0x98] = op_0xCB_0x98_RES_3_B;
    cb_opcodes[0x99] = op_0xCB_0x99_RES_3_C;
    cb_opcodes[0x9A] = op_0xCB_0x9A_RES_3_D;
    cb_opcodes[0x9B] = op_0xCB_0x9B_RES_3_E;
    cb_opcodes[0x9C] = op_0xCB_0x9C_RES_3_H;
    cb_opcodes[0x9D] = op_0xCB_0x9D_RES_3_L;
    cb_opcodes[0x9E] = op_0xCB_0x9E_RES_3_HL;
    cb_opcodes[0x9F] = op_0xCB_0x9F_RES_3_A;
    cb_opcodes[0xA0] = op_0xCB_0xA0_RES_4_B;
    cb_opcodes[0xA1] = op_0xCB_0xA1_RES_4_C;
    cb_opcodes[0xA2] = op_0xCB_0xA2_RES_4_D;
    cb_opcodes[0xA3] = op_0xCB_0xA3_RES_4_E;
    cb_opcodes[0xA4] = op_0xCB_0xA4_RES_4_H;
    cb_opcodes[0xA5] = op_0xCB_0xA5_RES_4_L;
    cb_opcodes[0xA6] = op_0xCB_0xA6_RES_4_HL;
    cb_opcodes[0xA7] = op_0xCB_0xA7_RES_4_A;
    cb_opcodes[0xA8] = op_0xCB_0xA8_RES_5_B;
    cb_opcodes[0xA9] = op_0xCB_0xA9_RES_5_C;
    cb_opcodes[0xAA] = op_0xCB_0xAA_RES_5_D;
    cb_opcodes[0xAB] = op_0xCB_0xAB_RES_5_E;
    cb_opcodes[0xAC] = op_0xCB_0xAC_RES_5_H;
    cb_opcodes[0xAD] = op_0xCB_0xAD_RES_5_L;
    cb_opcodes[0xAE] = op_0xCB_0xAE_RES_5_HL;
    cb_opcodes[0xAF] = op_0xCB_0xAF_RES_5_A;
    cb_opcodes[0xB0] = op_0xCB_0xB0_RES_6_B;
    cb_opcodes[0xB1] = op_0xCB_0xB1_RES_6_C;
    cb_opcodes[0xB2] = op_0xCB_0xB2_RES_6_D;
    cb_opcodes[0xB3] = op_0xCB_0xB3_RES_6_E;
    cb_opcodes[0xB4] = op_0xCB_0xB4_RES_6_H;
    cb_opcodes[0xB5] = op_0xCB_0xB5_RES_6_L;
    cb_opcodes[0xB6] = op_0xCB_0xB6_RES_6_HL;
    cb_opcodes[0xB7] = op_0xCB_0xB7_RES_6_A;
    cb_opcodes[0xB8] = op_0xCB_0xB8_RES_7_B;
    cb_opcodes[0xB9] = op_0xCB_0xB9_RES_7_C;
    cb_opcodes[0xBA] = op_0xCB_0xBA_RES_7_D;
    cb_opcodes[0xBB] = op_0xCB_0xBB_RES_7_E;
    cb_opcodes[0xBC] = op_0xCB_0xBC_RES_7_H;
    cb_opcodes[0xBD] = op_0xCB_0xBD_RES_7_L;
    cb_opcodes[0xBE] = op_0xCB_0xBE_RES_7_HL;
    cb_opcodes[0xBF] = op_0xCB_0xBF_RES_7_A;
    cb_opcodes[0xC0] = op_0xCB_0xC0_SET_0_B;
    cb_opcodes[0xC1] = op_0xCB_0xC1_SET_0_C;
    cb_opcodes[0xC2] = op_0xCB_0xC2_SET_0_D;
    cb_opcodes[0xC3] = op_0xCB_0xC3_SET_0_E;
    cb_opcodes[0xC4] = op_0xCB_0xC4_SET_0_H;
    cb_opcodes[0xC5] = op_0xCB_0xC5_SET_0_L;
    cb_opcodes[0xC6] = op_0xCB_0xC6_SET_0_HL;
    cb_opcodes[0xC7] = op_0xCB_0xC7_SET_0_A;
    cb_opcodes[0xC8] = op_0xCB_0xC8_SET_1_B;
    cb_opcodes[0xC9] = op_0xCB_0xC9_SET_1_C;
    cb_opcodes[0xCA] = op_0xCB_0xCA_SET_1_D;
    cb_opcodes[0xCB] = op_0xCB_0xCB_SET_1_E;
    cb_opcodes[0xCC] = op_0xCB_0xCC_SET_1_H;
    cb_opcodes[0xCD] = op_0xCB_0xCD_SET_1_L;
    cb_opcodes[0xCE] = op_0xCB_0xCE_SET_1_HL;
    cb_opcodes[0xCF] = op_0xCB_0xCF_SET_1_A;
    cb_opcodes[0xD0] = op_0xCB_0xD0_SET_2_B;
    cb_opcodes[0xD1] = op_0xCB_0xD1_SET_2_C;
    cb_opcodes[0xD2] = op_0xCB_0xD2_SET_2_D;
    cb_opcodes[0xD3] = op_0xCB_0xD3_SET_2_E;
    cb_opcodes[0xD4] = op_0xCB_0xD4_SET_2_H;
    cb_opcodes[0xD5] = op_0xCB_0xD5_SET_2_L;
    cb_opcodes[0xD6] = op_0xCB_0xD6_SET_2_HL;
    cb_opcodes[0xD7] = op_0xCB_0xD7_SET_2_A;
    cb_opcodes[0xD8] = op_0xCB_0xD8_SET_3_B;
    cb_opcodes[0xD9] = op_0xCB_0xD9_SET_3_C;
    cb_opcodes[0xDA] = op_0xCB_0xDA_SET_3_D;
    cb_opcodes[0xDB] = op_0xCB_0xDB_SET_3_E;
    cb_opcodes[0xDC] = op_0xCB_0xDC_SET_3_H;
    cb_opcodes[0xDD] = op_0xCB_0xDD_SET_3_L;
    cb_opcodes[0xDE] = op_0xCB_0xDE_SET_3_HL;
    cb_opcodes[0xDF] = op_0xCB_0xDF_SET_3_A;
    cb_opcodes[0xE0] = op_0xCB_0xE0_SET_4_B;
    cb_opcodes[0xE1] = op_0xCB_0xE1_SET_4_C;
    cb_opcodes[0xE2] = op_0xCB_0xE2_SET_4_D;
    cb_opcodes[0xE3] = op_0xCB_0xE3_SET_4_E;
    cb_opcodes[0xE4] = op_0xCB_0xE4_SET_4_H;
    cb_opcodes[0xE5] = op_0xCB_0xE5_SET_4_L;
    cb_opcodes[0xE6] = op_0xCB_0xE6_SET_4_HL;
    cb_opcodes[0xE7] = op_0xCB_0xE7_SET_4_A;
    cb_opcodes[0xE8] = op_0xCB_0xE8_SET_5_B;
    cb_opcodes[0xE9] = op_0xCB_0xE9_SET_5_C;
    cb_opcodes[0xEA] = op_0xCB_0xEA_SET_5_D;
    cb_opcodes[0xEB] = op_0xCB_0xEB_SET_5_E;
    cb_opcodes[0xEC] = op_0xCB_0xEC_SET_5_H;
    cb_opcodes[0xED] = op_0xCB_0xED_SET_5_L;
    cb_opcodes[0xEE] = op_0xCB_0xEE_SET_5_HL;
    cb_opcodes[0xEF] = op_0xCB_0xEF_SET_5_A;
    cb_opcodes[0xF0] = op_0xCB_0xF0_SET_6_B;
    cb_opcodes[0xF1] = op_0xCB_0xF1_SET_6_C;
    cb_opcodes[0xF2] = op_0xCB_0xF2_SET_6_D;
    cb_opcodes[0xF3] = op_0xCB_0xF3_SET_6_E;
    cb_opcodes[0xF4] = op_0xCB_0xF4_SET_6_H;
    cb_opcodes[0xF5] = op_0xCB_0xF5_SET_6_L;
    cb_opcodes[0xF6] = op_0xCB_0xF6_SET_6_HL;
    cb_opcodes[0xF7] = op_0xCB_0xF7_SET_6_A;
    cb_opcodes[0xF8] = op_0xCB_0xF8_SET_7_B;
    cb_opcodes[0xF9] = op_0xCB_0xF9_SET_7_C;
    cb_opcodes[0xFA] = op_0xCB_0xFA_SET_7_D;
    cb_opcodes[0xFB] = op_0xCB_0xFB_SET_7_E;
    cb_opcodes[0xFC] = op_0xCB_0xFC_SET_7_H;
    cb_opcodes[0xFD] = op_0xCB_0xFD_SET_7_L;
    cb_opcodes[0xFE] = op_0xCB_0xFE_SET_7_HL;
    cb_opcodes[0xFF] = op_0xCB_0xFF_SET_7_A;
}

void Gameboy::init_graphics()
{
    target_fps = 60;

    window_title = "Gameboy Emulator - " + header_title;

    InitWindow(SCREEN_WIDTH * SCREEN_SCALE, SCREEN_HEIGHT * SCREEN_SCALE, window_title.c_str());
    // SetTargetFPS(target_fps);
    SetExitKey(0); // Disable ESC exit key

    // Create texture for framebuffer
    Image image = {
        .data = framebuffer_front.data(),
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    Texture2D* tex = new Texture2D(LoadTextureFromImage(image));
    SetTextureFilter(*tex, TEXTURE_FILTER_POINT);
    texture = static_cast<void*>(tex);
}

void Gameboy::update_window_title(size_t measured_fps)
{
    SetWindowTitle(std::format("{} - {} FPS", window_title, measured_fps).c_str());
}

void Gameboy::cleanup_graphics()
{
    if (texture) {
        Texture2D* tex = static_cast<Texture2D*>(texture);
        UnloadTexture(*tex);
        delete tex;
        texture = nullptr;
    }
    CloseWindow();
}

void Gameboy::request_interrupt(u8 bit)
{
    write8(0xFF0F, static_cast<u8>(read8(0xFF0F) | (1 << bit)));
}

void Gameboy::write8(u16 addr, u8 value)
{
    if (addr < 0x8000) {
        handle_banking(addr, value);
    } else if ((addr >= 0xA000) && (addr < 0xC000)) {
        if (!ram_enabled) {
            return;
        }

        if (mbc_type == 3 && rtc_selected_register <= 0x04) {
            rtc_registers[rtc_selected_register] = value;
            return;
        }

        if (ram_banks.empty() || ram_bank_size == 0 || ram_bank_count == 0) {
            return;
        }

        const size_t offset = addr - 0xA000;
        const size_t bank_offset = ram_bank_size ? (offset % ram_bank_size) : 0;
        const size_t bank_index = ram_bank_count ? (current_ram_bank % ram_bank_count) : 0;
        const size_t absolute_index = bank_index * ram_bank_size + bank_offset;
        if (absolute_index >= ram_banks.size()) {
            return;
        }

        u8& target = ram_banks[absolute_index];
        if (target != value) {
            target = value;
            ram_dirty = true;
        }
    } else if (addr == 0xFF00) {
        // joypad register, only bits 4-5 are writable
        memory[0xFF00] = (memory[0xFF00] & 0xCF) | (value & 0x30);
    } else if (addr == DIV) {
        // divider register, writing any value resets it to 0
        memory[DIV] = 0;
    } else if (addr == TMC) {
        // timer control register
        u8 old_freq = memory[TMC] & 0x3; // bits 0-1
        memory[TMC] = value;
        u8 new_freq = value & 0x3;
        if (old_freq != new_freq) {
            switch (new_freq) {
            case 0:
                timer_counter = 1024;
                break; // 4096 Hz
            case 1:
                timer_counter = 16;
                break; // 262144 Hz
            case 2:
                timer_counter = 64;
                break; // 65536 Hz
            case 3:
                timer_counter = 256;
                break; // 16384 Hz
            }
        }
    } else if (addr == 0xFF0F) {
        // IF (0xFF0F): only bits 0-4 are writable
        memory[0xFF0F] = value & 0x1F;
    } else if (addr == 0xFF44) {
        // writing to LY register resets it to 0
        memory[0xFF44] = 0;
    } else if (addr == 0xFF46) {
        // DMA transfer
        u16 source = static_cast<u16>(value) << 8;
        for (int i = 0; i < 0xA0; i++) {
            write8(0xFE00 + i, read8(static_cast<u16>(source + i)));
        }
    } else if (addr == 0xFF47 || addr == 0xFF48 || addr == 0xFF49) {
        memory[addr] = value;
        refresh_palette_cache(static_cast<u8>(addr - 0xFF47), value);
    } else {
        memory[addr] = value;
    }
}

void Gameboy::write16(u16 addr, u16 value)
{
    write8(addr, value & 0xFF); // low byte
    write8(addr + 1, (value >> 8) & 0xFF); // high byte
}

void Gameboy::handle_banking(u16 addr, u8 value)
{
    // RAM enabling
    if (addr < 0x2000) {
        bool previous_ram_enabled = ram_enabled;
        if (mbc_type == 2) {

            // DoRAMBankEnable

            // if bit 4 of addr is set do nothing
            if (addr & (1 << 4)) {
                return;
            }
        }

        if (mbc_type == 1 || mbc_type == 2) {
            u8 masked = value & 0x0F;
            if (masked == 0x0A) {
                ram_enabled = true;
            } else if (masked == 0x00) {
                ram_enabled = false;
            }
        } else if (mbc_type == 3) {
            value &= 0x0F;
            ram_enabled = (value == 0x0A);
            if (!ram_enabled) {
                rtc_selected_register = 0xFF;
            }
        }

        if (previous_ram_enabled && !ram_enabled) {
            save_save_ram();
        }
    }
    // ROM bank change
    else if ((addr >= 0x2000) && (addr < 0x4000)) {
        if (mbc_type == 1 || mbc_type == 2) {

            // DoChangeLoROMBank

            if (mbc_type == 2) {
                u16 bank = value & 0x0F;
                set_rom_bank(bank);
                return;
            }

            u16 bank = (current_rom_bank & 0xE0) | (value & 0x1F); // combine high bits with new low bits
            set_rom_bank(bank);
        }
        if (mbc_type == 3) {
            u16 bank = value & 0x7F;
            set_rom_bank(bank);
        }
    }
    // do ROM or RAM bank change
    else if ((addr >= 0x4000) && (addr < 0x6000)) {
        // there is no MBC2 RAM banking
        if (mbc_type == 1) {
            if (rom_banking) {

                // DoChangeHiRomBank

                u16 bank = (current_rom_bank & 0x1F) | (value & 0xE0); // combine low bits with new high bits
                set_rom_bank(bank);

            } else {

                // DoRAMBankChange

                set_ram_bank(value & 0x03);
            }
        }
        if (mbc_type == 3) {
            if ((value & 0x0F) <= 0x03) {
                set_ram_bank(value & 0x03);
                rtc_selected_register = 0xFF;
            } else if ((value & 0x0F) >= 0x08 && (value & 0x0F) <= 0x0C) {
                rtc_selected_register = (value & 0x0F) - 0x08;
            } else {
                rtc_selected_register = 0xFF;
            }
        }
    }
    // this will change whether we are doing ROM banking
    // or RAM banking with the above if statement
    else if ((addr >= 0x6000) && (addr < 0x8000)) {
        if (mbc_type == 1) {

            // DoChangeROMRAMMode

            value &= 0x01;
            rom_banking = (value == 0);
            if (rom_banking) {
                set_ram_bank(0);
            }
        } else if (mbc_type == 3) {
            if (value == 0x00) {
                rtc_latch_active = false;
                rtc_latch_previous_value = 0x00;
            } else if (value == 0x01) {
                if (rtc_latch_previous_value == 0x00) {
                    rtc_latched_registers = rtc_registers;
                    rtc_latch_active = true;
                }
                rtc_latch_previous_value = 0x01;
            }
        }
    }
}

void Gameboy::set_rom_bank(u16 bank)
{
    if (rom_bank_count == 0) {
        current_rom_bank = 0;
        current_rom_bank_ptr = memory.data() + 0x4000;
        return;
    }

    bank %= rom_bank_count;
    if (bank == 0 && rom_bank_count > 1) {
        bank = 1;
    }

    current_rom_bank = bank;
    const size_t offset = static_cast<size_t>(current_rom_bank) * 0x4000;
    current_rom_bank_ptr = cartridge.data() + offset;
}

void Gameboy::set_ram_bank(u8 bank)
{
    if (ram_bank_count == 0) {
        current_ram_bank = 0;
        return;
    }

    current_ram_bank = bank % ram_bank_count;
}

void Gameboy::refresh_palette_cache(u8 index, u8 value)
{
    auto& cache = palette_cache[index];
    cache[0] = DMG_PALETTE[(value >> 0) & 0x03];
    cache[1] = DMG_PALETTE[(value >> 2) & 0x03];
    cache[2] = DMG_PALETTE[(value >> 4) & 0x03];
    cache[3] = DMG_PALETTE[(value >> 6) & 0x03];
}

u8 Gameboy::run_opcode()
{
    if (halted) {
        return 4; // 4 t-cycles
    }

    bool should_enable_ime = ime_scheduled;
    bool had_halt_bug = halt_bug;

    u8 opcode = read8(had_halt_bug ? PC + 1 : PC);
    u8 cycles = opcodes[opcode](*this);

    if (had_halt_bug) {
        halt_bug = false;
    }

    if (should_enable_ime) {
        ime = true;
        ime_scheduled = false;
    }

    return cycles;
}

void Gameboy::update_inputs()
{
    // handle FPS updates with page up and down

    if (IsKeyPressed(KEY_PAGE_UP)) {
        target_fps += 30;
        SetTargetFPS(target_fps);
    } else if (IsKeyPressed(KEY_PAGE_DOWN)) {
        target_fps = std::max(target_fps - 30, 30);
        SetTargetFPS(target_fps);
    }

    // now the actual gameboy inputs

    u8 new_state = 0xFF; // all buttons unpressed

    if (IsKeyDown(KEY_RIGHT)) {
        new_state &= ~(1 << 0);
    }
    if (IsKeyDown(KEY_LEFT)) {
        new_state &= ~(1 << 1);
    }
    if (IsKeyDown(KEY_UP)) {
        new_state &= ~(1 << 2);
    }
    if (IsKeyDown(KEY_DOWN)) {
        new_state &= ~(1 << 3);
    }

    if (IsKeyDown(KEY_A)) {
        new_state &= ~(1 << 4); // A button
    }
    if (IsKeyDown(KEY_S)) {
        new_state &= ~(1 << 5); // B button
    }
    if (IsKeyDown(KEY_BACKSPACE)) {
        new_state &= ~(1 << 6); // Select
    }
    if (IsKeyDown(KEY_ENTER)) {
        new_state &= ~(1 << 7); // Start
    }

    u8 pressed = joypad_state & ~new_state; // bits that went from 1 to 0
    joypad_state = new_state; // save new state

    // check if joypad interrupt should be requested
    if (pressed) {
        u8 joyp = memory[0xFF00];
        bool dir_selected = !(joyp & 0x10);
        bool btn_selected = !(joyp & 0x20);
        if ((dir_selected && (pressed & 0x0F)) || (btn_selected && (pressed & 0xF0))) {
            request_interrupt(4);
        }
    }
}

u8 Gameboy::check_interrupts()
{
    u8 requested = read8(0xFF0F);
    u8 enabled = read8(0xFFFF);
    u8 triggered = requested & enabled & 0x1F;

    if (triggered == 0) {
        return 0;
    }

    if (halted) {
        halted = false;
        halt_bug = false;
        if (!ime) {
            return 0;
        }
    }

    if (ime) {
        int bit_idx = std::countr_zero(triggered); // find first set bit

        ime = false;
        ime_scheduled = false;
        write8(0xFF0F, requested & ~(1 << bit_idx));

        SP -= 2;
        write16(SP, PC);

        switch (bit_idx) {
        case 0:
            PC = 0x40;
            break; // V-Blank
        case 1:
            PC = 0x48;
            break; // LCD STAT
        case 2:
            PC = 0x50;
            break; // Timer overflow
        case 3:
            PC = 0x58;
            break; // Serial
        case 4:
            PC = 0x60;
            break; // Joypad
        }

        return 20; // 20 t-cycles
    }

    return 0;
}

void Gameboy::update_timers(u8 cycles)
{
    // of interest for better accuracy:
    // https://github.com/Ashiepaws/GBEDG/blob/master/timers/index.md

    divider_counter += cycles;
    if (divider_counter >= 256) {
        divider_counter -= 256;
        memory[0xFF04]++; // write8 would reset it to 0
    }

    // timer enabled if bit 2 of TMC is set
    if (read8(TMC) & (1 << 2)) {

        timer_counter -= cycles;

        while (timer_counter <= 0) {

            // reset clock based on frequency
            switch (read8(TMC) & 0x3) {
            case 0:
                timer_counter += 1024;
                break; // 4096 Hz
            case 1:
                timer_counter += 16;
                break; // 262144 Hz
            case 2:
                timer_counter += 64;
                break; // 65536 Hz
            case 3:
                timer_counter += 256;
                break; // 16384 Hz
            }

            // check for timer overflow interrupt
            if (read8(TIMA) == 255) {
                write8(TIMA, read8(TMA));
                request_interrupt(2);
            } else {
                write8(TIMA, read8(TIMA) + 1);
            }
        }
    }
}

PPU_Color Gameboy::get_color(u16 palette_register, u8 color_id)
{
    const u8 index = color_id & 0x03;
    switch (palette_register) {
    case 0xFF47:
        return std::bit_cast<PPU_Color>(palette_cache[0][index]);
    case 0xFF48:
        return std::bit_cast<PPU_Color>(palette_cache[1][index]);
    case 0xFF49:
        return std::bit_cast<PPU_Color>(palette_cache[2][index]);
    default:
        return std::bit_cast<PPU_Color>(DMG_PALETTE[(read8(palette_register) >> (index * 2)) & 0x03]);
    }
}

void Gameboy::set_ppu_mode(u8 mode)
{
    mode &= 0x03;
    if (ppu_mode == mode) {
        return;
    }

    ppu_mode = mode;

    u8 stat = memory[0xFF41];
    stat = (stat & ~0x03) | mode;
    memory[0xFF41] = stat;

    bool request_stat = false;
    switch (mode) {
    case 0:
        request_stat = stat & 0x08;
        break;
    case 1:
        request_stat = stat & 0x10;
        break;
    case 2:
        request_stat = stat & 0x20;
        break;
    default:
        break;
    }

    if (request_stat) {
        request_interrupt(1);
    }
}

void Gameboy::update_stat_coincidence_flag()
{
    u8 stat = memory[0xFF41];
    bool was_coincident = stat & 0x04;

    if (memory[0xFF44] == memory[0xFF45]) {
        stat |= 0x04;
        if (!was_coincident && (stat & 0x40)) {
            request_interrupt(1);
        }
    } else {
        stat &= 0xFB;
    }

    stat = (stat & ~0x03) | (ppu_mode & 0x03);
    memory[0xFF41] = stat;
}

void Gameboy::evaluate_sprites(u8 ly)
{
    scanline_sprite_count = 0;

    u8 lcdc = read8(0xFF40);
    if (!(lcdc & 0x02)) {
        return;
    }

    bool tall_sprites = lcdc & 0x04;
    int sprite_height = tall_sprites ? 16 : 8;

    for (int i = 0; i < 40 && scanline_sprite_count < 10; ++i) {
        u8 y = memory[0xFE00 + i * 4];
        u8 x = memory[0xFE00 + i * 4 + 1];
        u8 tile = memory[0xFE00 + i * 4 + 2];
        u8 attributes = memory[0xFE00 + i * 4 + 3];

        int sprite_y = static_cast<int>(y) - 16;
        if (sprite_y <= static_cast<int>(ly) && static_cast<int>(ly) < sprite_y + sprite_height) {
            scanline_sprites[scanline_sprite_count++] = { x, y, tile, attributes, static_cast<u8>(i) };
        }
    }
}

bool Gameboy::render_scanline()
{
    const u8 ly = memory[0xFF44];
    if (ly >= SCREEN_HEIGHT) {
        return false;
    }

    const u8 lcdc = read8(0xFF40);
    const bool bg_enabled = lcdc & 0x01;
    const bool sprite_enabled = lcdc & 0x02;
    const bool tall_sprites = lcdc & 0x04;
    const bool window_enabled = lcdc & 0x20;

    const u16 bg_map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    const u16 window_map_base = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    const bool use_signed_tile_index = !(lcdc & 0x10);

    const u8 scx = read8(0xFF43);
    const u8 scy = read8(0xFF42);
    const u8 wx = read8(0xFF4B);
    const u8 wy = read8(0xFF4A);

    const bool window_possible = window_enabled && ly >= wy && wx <= 166;
    const int window_screen_x = std::max(0, static_cast<int>(wx) - 7);

    const int bg_y = (static_cast<int>(scy) + ly) & 0xFF;
    const int bg_tile_row = (bg_y >> 3) & 0x1F;
    const int bg_tile_line = bg_y & 0x07;
    const int bg_tile_col_base = scx >> 3;
    const int scx_offset = scx & 0x07;

    const int window_line = window_line_counter;
    const int window_tile_row = (window_line >> 3) & 0x1F;
    const int window_tile_line = window_line & 0x07;

    const u8* mem = memory.data();

    auto compute_tile_addr = [use_signed_tile_index](u8 tile_number) -> u16 {
        if (!use_signed_tile_index) {
            return static_cast<u16>(0x8000 + static_cast<u16>(tile_number) * 16);
        }
        int signed_index = static_cast<int8_t>(tile_number);
        return static_cast<u16>(0x9000 + signed_index * 16);
    };

    std::array<u8, SCREEN_WIDTH> bg_colors {};
    if (bg_enabled || sprite_enabled) {
        int x = 0;
        int tile_col = bg_tile_col_base;
        int pixel_offset = scx_offset;
        const int row_offset = (bg_tile_row & 0x1F) * 32;

        while (x < SCREEN_WIDTH) {
            const u16 map_addr = static_cast<u16>(bg_map_base + ((tile_col & 0x1F) + row_offset));
            const u8 tile_number = mem[map_addr];
            const u16 tile_addr = static_cast<u16>(compute_tile_addr(tile_number) + bg_tile_line * 2);
            const u8 low = mem[tile_addr];
            const u8 high = mem[tile_addr + 1];

            for (int px = pixel_offset; px < 8 && x < SCREEN_WIDTH; ++px, ++x) {
                const int bit = 7 - px;
                bg_colors[x] = static_cast<u8>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
            }

            pixel_offset = 0;
            ++tile_col;
        }
    }

    bool window_used_this_line = false;
    if (window_possible && window_screen_x < SCREEN_WIDTH) {
        window_used_this_line = true;
        int x = window_screen_x;
        int tile_col = 0;
        const int row_offset = (window_tile_row & 0x1F) * 32;

        while (x < SCREEN_WIDTH) {
            const u16 map_addr = static_cast<u16>(window_map_base + (row_offset + (tile_col & 0x1F)));
            const u8 tile_number = mem[map_addr];
            const u16 tile_addr = static_cast<u16>(compute_tile_addr(tile_number) + window_tile_line * 2);
            const u8 low = mem[tile_addr];
            const u8 high = mem[tile_addr + 1];

            for (int px = 0; px < 8 && x < SCREEN_WIDTH; ++px, ++x) {
                const int bit = 7 - px;
                bg_colors[x] = static_cast<u8>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
            }

            ++tile_col;
        }
    }

    std::array<u8, SCREEN_WIDTH> sprite_color {};
    std::array<u8, SCREEN_WIDTH> sprite_palette_idx {};
    std::array<u8, SCREEN_WIDTH> sprite_priority {};
    std::array<u8, SCREEN_WIDTH> sprite_has {};

    if (sprite_enabled && scanline_sprite_count > 0) {
        const int sprite_height = tall_sprites ? 16 : 8;
        for (int i = 0; i < scanline_sprite_count; ++i) {
            const Sprite& sprite = scanline_sprites[i]; // Preserve OAM order for sprite priority

            int screen_x = static_cast<int>(sprite.x) - 8;
            if (screen_x >= SCREEN_WIDTH || screen_x <= -8) {
                continue;
            }

            int sprite_y = static_cast<int>(sprite.y) - 16;
            int line = static_cast<int>(ly) - sprite_y;
            if (line < 0 || line >= sprite_height) {
                continue;
            }

            if (sprite.attributes & 0x40) {
                line = sprite_height - 1 - line;
            }

            u8 tile_index = sprite.tile;
            int tile_line = line;
            if (tall_sprites) {
                tile_index &= 0xFE;
                if (tile_line >= 8) {
                    tile_index += 1;
                    tile_line -= 8;
                }
            }

            const u16 tile_addr = static_cast<u16>(0x8000 + static_cast<u16>(tile_index) * 16 + tile_line * 2);
            const u8 low = mem[tile_addr];
            const u8 high = mem[tile_addr + 1];

            const bool flip_x = sprite.attributes & 0x20;
            const int start_px = std::max(0, -screen_x);
            const int end_px = std::min(8, SCREEN_WIDTH - screen_x);
            for (int px = start_px; px < end_px; ++px) {
                const int bit = flip_x ? px : (7 - px);
                const u8 color = static_cast<u8>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
                if (color == 0) {
                    continue;
                }

                const int target_x = screen_x + px;
                if (sprite_has[target_x]) {
                    continue;
                }

                sprite_has[target_x] = 1;
                sprite_color[target_x] = color;
                sprite_palette_idx[target_x] = (sprite.attributes & 0x10) ? 1 : 0;
                sprite_priority[target_x] = (sprite.attributes & 0x80) ? 1 : 0;
            }
        }
    }

    const u32* const bg_palette = palette_cache[0].data();
    const u32* const obj_palette0 = palette_cache[1].data();
    const u32* const obj_palette1 = palette_cache[2].data();

    u32* framebuffer_line = framebuffer_back.data() + static_cast<size_t>(ly) * SCREEN_WIDTH;
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        const u8 bg_color = bg_colors[x];
        u8 color_id = bg_enabled ? bg_color : 0;
        const u32* palette_ptr = bg_palette;

        if (sprite_enabled && sprite_has[x]) {
            bool draw_sprite = true;
            if (sprite_priority[x] && bg_enabled && bg_color != 0) {
                draw_sprite = false;
            }
            if (draw_sprite) {
                color_id = sprite_color[x];
                palette_ptr = sprite_palette_idx[x] ? obj_palette1 : obj_palette0;
            }
        }

        framebuffer_line[x] = palette_ptr[color_id & 0x03];
    }

    return window_used_this_line;
}

void Gameboy::ppu_step(u8 cycles)
{
    if (!(memory[0xFF40] & 0x80)) {
        ppu_cycle = 0;
        scanline_counter = 0;
        scanline_sprite_count = 0;
        scanline_rendered = false;
        window_line_counter = 0;
        memory[0xFF44] = 0;
        ppu_mode = 0;
        update_stat_coincidence_flag();
        return;
    }

    while (cycles > 0) {
        const u8 ly = memory[0xFF44];
        const bool visible_scanline = ly < 144;

        u16 target_cycle = 456;

        if (visible_scanline) {
            if (ppu_cycle < 80) {
                if (ppu_mode != 2) {
                    set_ppu_mode(2);
                    evaluate_sprites(ly);
                    scanline_rendered = false;
                }
                target_cycle = 80;
            } else if (ppu_cycle < 252) {
                if (ppu_mode != 3) {
                    set_ppu_mode(3);
                }
                if (!scanline_rendered) {
                    bool window_used = render_scanline();
                    if (window_used) {
                        window_line_counter++;
                    }
                    scanline_rendered = true;
                }
                target_cycle = 252;
            } else {
                if (ppu_mode != 0) {
                    set_ppu_mode(0);
                }
                target_cycle = 456;
            }
        } else {
            if (ppu_mode != 1) {
                set_ppu_mode(1);
            }
            target_cycle = 456;
        }

        const u16 diff = target_cycle - ppu_cycle;
        const u8 step = diff <= cycles ? static_cast<u8>(std::min<u16>(diff, 255)) : cycles;

        update_stat_coincidence_flag();

        ppu_cycle += step;
        scanline_counter = ppu_cycle;
        cycles -= step;

        if (ppu_cycle >= 456) {
            ppu_cycle -= 456;
            scanline_counter = ppu_cycle;

            u8 new_ly = static_cast<u8>(memory[0xFF44] + 1);
            memory[0xFF44] = new_ly;

            const u8 lcdc_after = memory[0xFF40];
            if ((lcdc_after & 0x20) && new_ly == memory[0xFF4A]) {
                window_line_counter = 0;
            }

            if (new_ly == 144) {
                set_ppu_mode(1);
                request_interrupt(0);
                framebuffer_front = framebuffer_back;
            } else if (new_ly > 153) {
                memory[0xFF44] = 0;
                window_line_counter = 0;
                scanline_rendered = false;
                evaluate_sprites(0);
                set_ppu_mode(2);
            } else if (new_ly < 144) {
                scanline_rendered = false;
                evaluate_sprites(new_ly);
                set_ppu_mode(2);
            }

            update_stat_coincidence_flag();
        }
    }
}

void Gameboy::run_one_frame()
{
    update_inputs();

    u32 cycles_this_frame = 0;
    while (cycles_this_frame < CYCLES_PER_FRAME) {
        u8 cycles = run_opcode();
        cycles += check_interrupts();
        update_timers(cycles);
        ppu_step(cycles);
        cycles_this_frame += cycles;
    }
}

void Gameboy::render_screen()
{
    Texture2D* tex = static_cast<Texture2D*>(texture);
    UpdateTexture(*tex, framebuffer_front.data());

    BeginDrawing();
    ClearBackground(BLACK);

    DrawTexturePro(
        *tex,
        { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT },
        { 0, 0, (float)(SCREEN_WIDTH * SCREEN_SCALE), (float)(SCREEN_HEIGHT * SCREEN_SCALE) },
        { 0, 0 },
        0.0f,
        WHITE);

    EndDrawing();
}

Gameboy::~Gameboy()
{
    save_save_ram();
    cleanup_graphics();
}
