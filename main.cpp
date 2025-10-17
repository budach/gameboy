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

    float total_time = 0.0f;
    size_t frames = 0;

    while (!WindowShouldClose()) {
        gb.run_one_frame();
        gb.render_screen();

        total_time += GetFrameTime();
        frames++;

        if (total_time >= 1.0f) {
            std::cout << "FPS: " << frames << std::endl;
            total_time -= 1.0f;
            frames = 0;
        }
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