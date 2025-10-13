// testing the json files from https://github.com/SingleStepTests/sm83
// just before and after, not cycle-by-cycle
// note: these tests require read8 and write8 to have unrestricted access to memory

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "gameboy.h"
#include "third-party/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

void run_test_file(const std::string& path)
{

    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        exit(1);
    }
    json tests = json::parse(file);
    file.close();

    std::cout << path << ": " << tests.size() << " tests... ";

    for (const auto& t : tests) {
        Gameboy gb("roms/Tetris.gb");

        gb.PC = t["initial"]["pc"].get<u16>();
        gb.SP = t["initial"]["sp"].get<u16>();
        gb.AF_bytes.A = t["initial"]["a"].get<u8>();
        gb.BC_bytes.B = t["initial"]["b"].get<u8>();
        gb.BC_bytes.C = t["initial"]["c"].get<u8>();
        gb.DE_bytes.D = t["initial"]["d"].get<u8>();
        gb.DE_bytes.E = t["initial"]["e"].get<u8>();
        gb.AF_bytes.F = t["initial"]["f"].get<u8>();
        gb.HL_bytes.H = t["initial"]["h"].get<u8>();
        gb.HL_bytes.L = t["initial"]["l"].get<u8>();

        for (const auto& pair : t["initial"]["ram"]) {
            u16 addr = pair[0].get<u16>();
            u8 value = pair[1].get<u8>();
            gb.write8(addr, value);
        }

        gb.run_opcode();

        assert(gb.PC == t["final"]["pc"].get<u16>());
        assert(gb.SP == t["final"]["sp"].get<u16>());
        assert(gb.AF_bytes.A == t["final"]["a"].get<u8>());
        assert(gb.BC_bytes.B == t["final"]["b"].get<u8>());
        assert(gb.BC_bytes.C == t["final"]["c"].get<u8>());
        assert(gb.DE_bytes.D == t["final"]["d"].get<u8>());
        assert(gb.DE_bytes.E == t["final"]["e"].get<u8>());
        assert(gb.AF_bytes.F == t["final"]["f"].get<u8>());
        assert(gb.HL_bytes.H == t["final"]["h"].get<u8>());
        assert(gb.HL_bytes.L == t["final"]["l"].get<u8>());

        for (const auto& pair : t["final"]["ram"]) {
            u16 addr = pair[0].get<u16>();
            u8 expected_value = pair[1].get<u8>();
            assert(gb.read8(addr) == expected_value);
        }
    }

    std::cout << " all passed!\n";
}

void run_all_tests(const std::string& directory)
{
    std::vector<std::string> test_files;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".json") {
            test_files.push_back(entry.path().string());
        }
    }

    std::cout << "Found " << test_files.size() << " JSON test files.\n";

    for (const auto& f : test_files) {
        run_test_file(f);
    }

    std::cout << "All tests finished!\n";
}
