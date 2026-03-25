# CnC_and_Red_Alert

Enhanced cross-platform SDL2 port of Command & Conquer: Red Alert and Tiberian Dawn.

## Enhancements over original source release

### Graphics
- **Ultrawide resolution** — 960x400 internal resolution (2.4:1 aspect ratio) for ultrawide monitors
- **xBR pixel-art upscaling** — 2x upscale with edge-aware anti-aliasing using optimized xBR algorithm (960x400 -> 1920x800)
- **Bilinear texture filtering** — smooth final scaling from upscaled resolution to display
- **Centered main menu** — title screen and buttons properly centered on ultrawide displays

### Audio
- **Fixed music playback** — resolved int16 volume overflow that silenced music entirely
- **12 simultaneous sound channels** — increased from 4 to prevent dropped sound effects
- **Westwood codec support** — music streaming now supports both SOS and Westwood compression
- **Positional audio** — distance-based volume attenuation from viewport center with stereo panning

### Unit AI
- **Smart defense** — player units automatically retaliate when attacked and scatter to dodge fire
- **Nearby unit defense** — idle friendly units within 5 cells auto-engage attackers
- **Auto-target with priority** — guard mode scans for enemy units first, then buildings as secondary targets
- **Attack-move** — units engage enemies within weapon range while moving
- **Target priority switching** — units attacking buildings automatically switch to enemy units that enter range

### Controls
- **WASD keyboard scrolling** — smooth map scrolling with comfortable speed
- **Skip intro** — click or keypress to skip splash screens

### Stability fixes
- Fix crash when starting new game (NULL theme name lookup)
- Fix KeyFrameSlots use-before-init
- Fix harvester pathfinding stutter when blocked by scattered units
- Fix Longbow not landing on empty helipad
- Fix paradrops on impassable terrain
- Fix AI units driving to upper-left corner on pathfinding failure

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
- For RA: needs REDALERT.MIX, MAIN.MIX, etc. from original CD
- For TD: needs similar MIX files from original game
- Run `rasdl` for Red Alert, `tdsdl` for Tiberian Dawn

## Architecture
- `RA/` — Red Alert game source (~200 cpp files)
- `TD/` — Tiberian Dawn game source (~150 cpp files, shares some files from RA/)
- `SDLLIB/` — SDL2 abstraction layer (graphics, audio, input, networking, xBR upscaler)
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
