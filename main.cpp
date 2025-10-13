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

// to run single step JSON tests
// #include "single_step_tests.h"
// run_all_tests("tests/");