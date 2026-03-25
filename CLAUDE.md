# CnC_and_Red_Alert

Cross-platform SDL2 port of Command & Conquer: Red Alert and Tiberian Dawn.

## Build

### Dependencies
- C++11 compiler (MSVC, GCC, Clang)
- CMake 3.12+
- SDL2

### Windows (MSVC)
```
# Download SDL2-devel-VC from https://github.com/libsdl-org/SDL/releases
cmake -Bbuild -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=<path-to-SDL2>
cmake --build build --config Release
# Copy SDL2.dll next to the executables
```

### Linux
```
sudo apt install libsdl2-dev
cmake -Bbuild
cmake --build build
```

### macOS
```
brew install sdl2
cmake -Bbuild
cmake --build build
```

## Running
- Copy game data files (MIX archives) next to `rasdl`/`tdsdl` executables
- For RA: needs REDALERT.MIX, MAIN.MIX, etc.
- For TD: needs similar MIX files from original game
- Run `rasdl` for Red Alert, `tdsdl` for Tiberian Dawn

## Architecture
- `RA/` — Red Alert game source (~200 cpp files)
- `TD/` — Tiberian Dawn game source (~150 cpp files, shares some files from RA/)
- `SDLLIB/` — SDL2 abstraction layer (graphics, audio, input, networking)
- `WINVQ/` — VQA video codec (cutscene playback)
- `port/` — Portability layer (string functions, platform abstractions)

## Build targets
- `rasdl` — Red Alert executable
- `tdsdl` — Tiberian Dawn executable
- Both are built with `PORTABLE=1` and `WIN32` definitions

## Key conventions
- Original 1996 C++ code — pre-STL, manual memory management
- Assembly routines translated to C++ in `winasm.cpp`, `2keyfbuf.cpp`
- `#ifdef PORTABLE` guards platform-specific code (SDL path)
- `#ifdef WIN32` used for Windows-only features (DDE, MCI, registry)
- IPX networking emulated over UDP via `ipxstub.cpp` (TD) and wsproto (RA)
- VQA audio uses `VQASDL_SOUND` path when not on DirectSound

## Known limitations
- Internet multiplayer (WChat/DDE) is Windows-only — requires modern replacement
- Some later campaign missions may have issues (untranslated assembly code)
- MCI MPEG movie playback is Windows-only (VQA cutscenes work cross-platform)
