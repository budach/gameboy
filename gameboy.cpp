#include "gameboy.h"
#include "opcodes.h"

Gameboy::Gameboy(const std::string& path_rom)
{
    // init memory

    memory.resize(0x10000, 0);

    // load rom

    std::ifstream file(path_rom, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open ROM file: " << path_rom << std::endl;
        exit(1);
    }
    cartridge = std::vector<u8>(std::istreambuf_iterator<char>(file), {});
    file.close();
    if (cartridge.size() != 0x8000) {
        std::cerr << "ROM must be exactly 32KB in size" << std::endl;
        exit(1);
    }

    std::copy(cartridge.begin(), cartridge.end(), memory.begin());

    // init registers and misc (state after boot ROM)

    AF = 0x01B0;
    BC = 0x0013;
    DE = 0x00D8;
    HL = 0x014D;
    SP = 0xFFFE;
    PC = 0x0100;
    low = 0x00;
    high = 0x00;

    // init opcode table
    for (int i = 0; i < 256; i++) {
        opcodes[i] = { op_unimplemented };
    }

    opcodes[0x00] = {};
    opcodes[0x01] = { op_set_C, op_set_B };
    opcodes[0x08] = { op_read_byte_low, op_read_byte_high, op_write_SP, op_nop };
    opcodes[0x11] = { op_set_E, op_set_D };
    opcodes[0x21] = { op_set_L, op_set_H };
    opcodes[0x31] = { op_set_SP, op_nop };
    opcodes[0xC1] = { op_pop_C, op_pop_B };
    opcodes[0xC5] = { op_nop, op_push_B, op_push_C };
    opcodes[0xD1] = { op_pop_E, op_pop_D };
    opcodes[0xD5] = { op_nop, op_push_D, op_push_E };
    opcodes[0xE1] = { op_pop_L, op_pop_H };
    opcodes[0xE5] = { op_nop, op_push_H, op_push_L };
    opcodes[0xF1] = { op_pop_F, op_pop_A };
    opcodes[0xF5] = { op_nop, op_push_A, op_push_F };
    opcodes[0xF9] = { op_write_SP_HL };
    opcodes[0xC3] = { op_read_byte_low, op_read_byte_high, op_jump_u16 };
}

u8 Gameboy::read8(u16 addr) const
{
    return memory[addr];
}

u16 Gameboy::read16(u16 addr) const
{
    return (read8(addr + 1) << 8) | read8(addr);
}

void Gameboy::write8(u16 addr, u8 value)
{
    memory[addr] = value;
}

void Gameboy::write16(u16 addr, u16 value)
{
    write8(addr, value & 0xFF); // low byte
    write8(addr + 1, (value >> 8) & 0xFF); // high byte
}