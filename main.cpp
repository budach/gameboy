#include <iostream>

#include "gameboy.h"
#include "raylib.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_rom>" << std::endl;
        return 1;
    }

    Gameboy gb(argv[1]);

    while (!WindowShouldClose()) {
        gb.run_one_frame();
        gb.render_screen();
    }

    return 0;
}

// currently passing tests:
// sm83 single-step JSON tests
// blargg cpu_instrs.gb
// blargg instr_timing.gb

// todo: halt_bug.gb (needs graphics)

// to run single step JSON tests
// #include "single_step_tests.h"
// run_all_tests("tests/");

// to optimize based on flamegraph:
// read8()
// render_scanline()
// update_stat_coincidence_flag()
// get_color()