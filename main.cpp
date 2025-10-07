#include "gameboy.h"
#include "opcodes.h"

int main()
{
    Gameboy gb("roms/Tetris.gb");

    while (1) {
        gb.run_one_frame();
        gb.render_screen();
    }

    return 0;
}