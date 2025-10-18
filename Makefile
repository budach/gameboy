COMPILER = g++
COMMONFLAGS = -Wall -Wextra -Werror -Wshadow -Wdouble-promotion -Wpedantic -Wformat=2 -pipe -std=c++20
DEBUGFLAGS = -O0 -g3
RELEASEFLAGS = -flto=auto -march=native -mtune=native -O3 -DNDEBUG -fno-plt -fno-rtti
LDFLAGS = -Wl,-O2,--as-needed,--gc-sections,--relax -lraylib

FILES = main.cpp gameboy.cpp opcodes.cpp
EXECUTABLE = gameboy

release:
	$(COMPILER) $(COMMONFLAGS) $(RELEASEFLAGS) $(FILES) -o $(EXECUTABLE) $(LDFLAGS)
	strip --strip-all -R .comment -R .note $(EXECUTABLE)

debug:
	$(COMPILER) $(COMMONFLAGS) $(DEBUGFLAGS) $(FILES) -o $(EXECUTABLE) $(LDFLAGS)