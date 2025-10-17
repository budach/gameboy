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
            gb.update_window_title(frames);
            total_time -= 1.0f;
            frames = 0;
        }
    }

    return 0;
}

// dmg-acid2.gb test (left mole) fails on purpose.
// other emus (e.g mGBA) also fail it. will make real games look better
// e.g. with passing the left mole test player and NPC sprites in
// Link's Awakening can overlap in weird ways

// to run single step JSON tests
// #include "single_step_tests.h"
// run_all_tests("tests/");
