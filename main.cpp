#include <chrono>
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

    // once per second print interrupt counts for debugging
    auto last_time = std::chrono::high_resolution_clock::now();

    while (!WindowShouldClose()) {
        gb.run_one_frame();
        gb.render_screen();

        auto current_time = std::chrono::high_resolution_clock::now();
        if ((current_time - last_time).count() >= 1000000000) {
            last_time = current_time;
            std::cout << "Interrupt counts: "
                      << "VBLANK=" << gb.interrupt_counts[0] << ", " // 60 max per second?
                      << "LCD STAT=" << gb.interrupt_counts[1] << ", "
                      << "TIMER=" << gb.interrupt_counts[2] << ", "
                      << "SERIAL=" << gb.interrupt_counts[3] << ", "
                      << "JOYPAD=" << gb.interrupt_counts[4] << "\n";
        }
    }

    return 0;
}

// currently passing tests:
// single-step JSON tests
// blargg individual CPU instruction tests
// blargg instr_timing test

// todo: halt_bug test

// to run single step JSON tests
// #include "single_step_tests.h"
// run_all_tests("tests/");