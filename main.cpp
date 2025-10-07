#include "gameboy.h"

int main()
{
    Gameboy demo_gb("roms/Tetris.gb");

    while (1) {
        demo_gb.run_one_frame();
        demo_gb.render_screen();
    }

    return 0;
}

// to run single step JSON tests
// #include "single_step_tests.h"
// run_all_tests("tests/");