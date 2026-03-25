
# Command & Conquer: Red Alert — Enhanced SDL2 Port

An enhanced cross-platform SDL2 port of Command & Conquer: Red Alert with ultrawide support, xBR sprite upscaling, improved unit AI, positional audio, and modern quality-of-life features.

Based on the [EA open source release](https://github.com/electronicarts/CnC_Red_Alert), partially merged with [Tiberian Dawn](https://github.com/electronicarts/CnC_Tiberian_Dawn).

## What's New

### Graphics
- **Ultrawide resolution** — 960x400 internal (2.4:1) for ultrawide monitors, shows more battlefield
- **xBR pixel-art upscaling** — 2x upscale (960x400 → 1920x800) with edge-aware anti-aliasing
- **Realism post-processor** — local contrast enhancement, edge ambient occlusion, cinematic color grading, 6-bit de-banding
- **Bilinear texture filtering** — smooth final scaling to any display resolution
- **Centered main menu** — title screen and buttons properly centered on ultrawide
- **FPS counter** — displayed in window title bar

### Audio
- **Fixed music playback** — resolved volume overflow that silenced music entirely
- **12 simultaneous sound channels** — increased from 4, no more dropped sound effects
- **Westwood codec support** — music streaming supports both SOS and Westwood compression
- **Positional audio** — distance-based volume attenuation from viewport center with stereo panning

### Unit AI
- **Smart defense** — player units automatically retaliate when attacked and scatter to dodge fire
- **Nearby unit defense** — idle friendly units within 5 cells auto-engage attackers
- **Auto-target with priority** — guard mode targets enemy units first, buildings second
- **Attack-move** — units engage enemies within weapon range while moving to a destination
- **Target priority switching** — units attacking buildings switch to enemy units that enter range

### Controls
- **WASD keyboard scrolling** — smooth map scrolling at comfortable speed
- **Skip intro** — click or keypress to skip splash screens

### Stability Fixes
- Fix crash on new game (NULL theme name lookup)
- Fix KeyFrameSlots use-before-init
- Fix harvester pathfinding stutter
- Fix Longbow not landing on empty helipad
- Fix paradrops on impassable terrain
- Fix AI units driving to map corner on pathfinding failure

## Setup Guide

### Requirements
- Original Red Alert game files (from CD or digital download)
- C++ compiler (MSVC, GCC, or Clang)
- CMake 3.12+
- SDL2

### Step 1: Build the Game

**Windows (MSVC):**
```bash
# Download SDL2-devel-VC from https://github.com/libsdl-org/SDL/releases
cmake -Bbuild -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=<path-to-SDL2>
cmake --build build --config Release
```

**Linux:**
```bash
sudo apt install libsdl2-dev
cmake -Bbuild
cmake --build build
```

**macOS:**
```bash
brew install sdl2
cmake -Bbuild
cmake --build build
```

### Step 2: Copy Game Data Files

The game needs the original Red Alert data files. Copy these from your Red Alert CD or installation:

**From CD1 (Allied Disc):**
```
CD1_ALLIED_DISC/MAIN.MIX        →  build/RA/Release/MAIN.MIX
CD1_ALLIED_DISC/INSTALL/REDALERT.MIX  →  build/RA/Release/REDALERT.MIX
```

**From CD2 (Soviet Disc) — optional, for Soviet campaign:**
```
CD2_SOVIET_DISC/MAIN.MIX        →  build/RA/Release/MAIN.MIX  (overwrite)
```

**On Windows, also copy SDL2.dll:**
```
SDL2.dll  →  build/RA/Release/SDL2.dll
```

The minimum required files in the `build/RA/Release/` directory:
```
rasdl.exe          (built executable)
SDL2.dll           (SDL2 runtime - Windows only)
REDALERT.MIX       (core game data - from CD INSTALL folder)
MAIN.MIX           (maps, sprites, audio - from CD root)
```

Optional files for full experience:
```
REDALERT.INI       (game settings - auto-created on first run)
SCORES.MIX         (music - inside MAIN.MIX, auto-loaded)
SPEECH.MIX         (unit voice lines - inside MAIN.MIX, auto-loaded)
EXPAND.MIX         (Counterstrike expansion)
EXPAND2.MIX        (Aftermath expansion)
```

### Step 3: Run

```bash
# Red Alert
./build/RA/Release/rasdl

# Tiberian Dawn (if built)
./build/TD/Release/tdsdl
```

## Architecture

| Directory | Contents |
|-----------|----------|
| `RA/` | Red Alert game source (~200 cpp files) |
| `TD/` | Tiberian Dawn game source (~150 cpp files) |
| `SDLLIB/` | SDL2 abstraction layer (graphics, audio, input, xBR upscaler, realism filter) |
| `WINVQ/` | VQA video codec (cutscene playback) |
| `port/` | Portability layer (string functions, platform abstractions) |

## Build Targets

| Target | Description |
|--------|-------------|
| `rasdl` | Red Alert executable |
| `tdsdl` | Tiberian Dawn executable |

## Known Limitations

- Internet multiplayer (WChat/DDE) requires a modern replacement
- Some later campaign missions may have issues with untranslated assembly code
- MCI MPEG movie playback is Windows-only (VQA cutscenes work cross-platform)
- LAN multiplayer via IPX-over-UDP is functional but lightly tested

## Original README

[EA's original README](README-EA.md)

## License

This program is free software under the GNU General Public License v3. See the original EA release for details.
