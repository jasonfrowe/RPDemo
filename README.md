# RP6502 Game Demo for the [Picocomputer 6502](https://github.com/picocomputer) using the [RP6502 SDK](https://picocomputer.github.io/sdk.html) and [llvm-mos](https://github.com/llvm-mos/llvm-mos-sdk)

![TitleScreen](Screenshots/Star_Hopper_Final.gif)

## Star Hopper — How to Play

Star Hopper is a vertical shoot-em-up for the Picocomputer (RP6502). Fight through 7 levels of enemy waves, defeat a boss at the end of each level, and survive to reach the YOU WIN screen.

### Controls

| Action | Gamepad |
|---|---|
| Move | D-Pad or Left Stick |
| Fire | X |
| Pause | Start |

### Enemies and Scoring

There are 7 enemy types, each worth more points than the last (10, 15, 20 … ). Destroying enemies without taking damage builds your **score multiplier** (1× up to 5×). Taking a hit resets the multiplier to 1×. 

After clearing a level, a **bonus screen** tallies your kills per enemy type and awards bonus points scaled by the level number.

### Boss Fights

Each level ends with a boss encounter.  Bosses have a vulerable that is bright yellow.  Shoot it to deal damage.  Some bosses will only be vunerable after certain conditions are met, which will be telegraphed visually.  For example, Boss Variant Four (Level 4) requires you to destroy all but one of the smaller enemies on screen before its vulnerable point will appear.

You have **4 minutes** to defeat the boss. If time runs out, the boss retreats and the level is marked **LEVEL FAILED**. Press Start to retry the same level.

### Asteroids and Power-Ups

Destroying asteroids can reveal power-up capsules. Pickups follow a fixed repeating sequence across the whole run:

| Icon | Pick-Up | Effect |
|---|---|---|
| **P** | Power | Increases fire rate (faster shots, down to a minimum cooldown) |
| **E** | Energy | Restores 8 HP |
| **S** | Speed | Raises your maximum movement speed |


### Health

Your ship has 48 HP. The health bar at the top right turns red when HP drops below 12. You have a brief invincibility window after each hit. Reaching 0 HP triggers a game-over.

You get an extra life every 100 000 points. You them wisely. 

---

## Table of Contents
- [Introduction](#introduction)
- [Platform Concepts](#platform-concepts)
- [Getting Started](#getting-started)
- [Provided Image Assets](#provided-image-assets)
- [Setting up Graphics](#setting-up-graphics)
- [Adding a Sprite](#adding-a-sprite)
- [Converting PNG Assets](#converting-png-assets)
- [Creating Graphics in Aseprite](#creating-graphics-in-aseprite)
- [Input System](#input-system)
- [Tilemaps and Backgrounds](#tilemaps-and-backgrounds)
- [Music](#music)
- [Making Music in Furnace (OPL2/YM3812 -> VGM)](#making-music-in-furnace-opl2ym3812---vgm)
- [Animations and Palette Swapping](#animations-and-palette-swapping)
- [Adding Bullets](#adding-bullets)
- [Gameplay Loop](#gameplay-loop)
- [Enemies and Collision Detection](#enemies-and-collision-detection)

## Introduction

This is a complete shoot-em-up (**Star Hopper**) built for the Picocomputer (RP6502) with the [RP6502 SDK](https://picocomputer.github.io/sdk.html) and the llvm-mos C compiler. The SDK's project template can build with cc65 or llvm-mos, but Star Hopper builds with the llvm-mos presets only. The game was written incrementally, and this README follows the same steps — you can read the code and explanations side by side and build your own game the same way.

The Picocomputer is built around a real WDC 65C02 CPU. Programming it feels like classic 8-bit development, but the surrounding hardware — VGA, OPL2 audio, gamepads, WiFi — is all modern and fully open source. Before jumping into code, it helps to understand a few concepts that are unique to this platform.

## Platform Concepts

Understanding these ideas first will make every later section much easier to follow.

### System RAM and XRAM

The 6502 sees its normal 64 KB of system RAM (`0x0000–0xFFFF`). This is where program code, the stack, and variables live. The RP6502 also has a second 64 KB called **Extended RAM (XRAM)**. XRAM is *not* directly addressable by the 6502 — there is no `LDA` or `STA` for it. Instead, the RIA chip provides two portals — `ADDR0/RW0` and `ADDR1/RW1` at hardware registers `0xFFE4–0xFFEB` — that let you read and write XRAM one byte at a time with auto-incrementing addresses. The LLVM-MOS SDK's `rp6502.h` wraps this in convenient macros:

```c
xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, 120); // write one struct field into XRAM
```

XRAM has no fixed map: your program decides where everything goes. In this project that map is one file, `src/xram.h`. It defines the structures (such as `mode5_sprite_t`) and one `XRAM_` name for each address (such as `XRAM_PLAYER_CONFIG`). The map starts small in [Adding a Sprite](#adding-a-sprite) and grows as the tutorial goes on.

### XRAM is the VGA System's Memory

The most important concept: **the VGA module reads XRAM directly and continuously.** You do not call a "draw sprite at X, Y" function each frame. Instead, you write a sprite's configuration (position, frame pointer, palette pointer) into a small struct in XRAM once, and the VGA hardware renders it automatically on every frame — until you change it.

To move a sprite, you write two new 16-bit values into XRAM. There is no draw call. This is what makes smooth 60 FPS animation possible even on a slow 6502.

### Three Planes, Fill + Sprite Layers

The VGA system has three numbered planes (0, 1, 2). Each plane has two independent layers:
- A **fill layer** — a tile map, bitmap, or console that covers the plane background.
- A **sprite layer** — a pool of hardware sprites drawn over the fill.

Different scanline ranges of the same plane can use different modes. This is how the HUD occupies just the top 24 scanlines of plane 2, while the gameplay tiles use the remaining scanlines.

This demo's plane layout:

| Plane | Fill layer | Sprite layer |
|---|---|---|
| 0 | Background star tiles (scanlines 24–239) |  Projectile sprites (full screen) |
| 1 | Foreground star tiles (scanlines 24–239) | Enemy sprites (scanlines 24–239) |
| 2 | HUD tile map (scanlines 0–23) | Player |

### Canvas and Vsync

`xreg_vga_canvas(CANVAS_320X240)` selects a canvas resolution. This demo uses `CANVAS_320X240` (320×240, 4:3). Available options:
- `CANVAS_CONSOLE` — 80-column console
- `CANVAS_320X240` — 320×240 (4:3)
- `CANVAS_320X180` — 320×180 (16:9)
- `CANVAS_640X480` — 640×480 (4:3)
- `CANVAS_640X360` — 640×360 (16:9)

These names and the `xreg_vga_canvas()` macro are not in `rp6502.h`. They come from the `xram.h` code block in the [VGA datasheet](https://picocomputer.github.io/vga.html)'s Key Registers section, which you copy into `src/xram.h`.

The register `RIA.vsync` at address `0xFFE3` increments once per frame (~60 Hz) when a VGA module is connected. In practice, this tick is generated at the frame boundary (after the last programmed scanline), so it lines up closely with the start of vertical blanking. That gives you a short, reliable window to update graphics registers without visible tearing.

The API name is `vsync`, but when writing gameplay code it is often easiest to think of it as a frame/vblank boundary signal. Your game loop compares it against a saved value to know when a new frame has started:

```c
if (RIA.vsync == vsync_last) continue; // same frame — spin
vsync_last = RIA.vsync;               // new frame — run game logic
```

### ROM Assets and the XRAM Address Offset

In `CMakeLists.txt`, an XRAM asset is loaded at one of the `XRAM_` names from `src/xram.h`, wrapped in `XRAM()`:

```cmake
rp6502_map(RPStarHopper src/xram.h "XRAM_.*")
rp6502_asset(RPStarHopper XRAM(XRAM_PLAYER_DATA) images/Player_4bpp.bin)
```

Within your C code, XRAM is addressed `0x0000–0xFFFF`, and `XRAM_PLAYER_DATA` is one of those plain 16-bit addresses. Your `ADDR0` portal and all XRAM struct pointers always use them. The ROM file uses a different convention: the ROM loader uses addresses `0x10000–0x1FFFF` to mean "load this into XRAM". `XRAM()` bridges the two. It takes the 16-bit XRAM address your program uses, adds the loader's `0x10000`, and stops CMake with an error if the address is outside XRAM.

`rp6502_map()` is what lets CMake use the name at all. When CMake configures, it compiles `src/xram.h` and reads the value of every `#define` that matches `XRAM_.*`. Each address is written once, in the header, and never repeated in `CMakeLists.txt`. Change the header and the next build configures again, so CMake always agrees with the C code. It must come after `add_executable()` and before any `rp6502_asset()` that uses its names.

Quick mental model for addresses:
- **System RAM (`0x0000–0xFFFF`)**: normal 6502-visible RAM for code/data/stack.
- **XRAM (`0x0000–0xFFFF`)**: separate 64 KB memory accessed through RIA portals (`ADDR0/RW0`, `ADDR1/RW1`). Every `XRAM_` name in `src/xram.h` is an address here.
- **ROM-packaging XRAM alias (`0x10000–0x1FFFF`)**: build-time/load-time notation meaning "copy into XRAM at low 16 bits". `XRAM()` writes it for you.

Example: `rp6502_asset(... XRAM(XRAM_PROJECTILE_DATA) images/Projectiles_4bpp.bin)` packages the file as a ROM chunk that loads into XRAM at `XRAM_PROJECTILE_DATA`, wherever the layout in `src/xram.h` puts it.

You will also see **named ROM assets** like `Title.vgm` or `StarFields_HUD_map.bin`. These are opened by name via `open("ROM:...")` and are not fixed XRAM addresses unless your code explicitly copies them into XRAM.

### Configuring Video Modes with XREG

`xreg_vga_mode2()` and `xreg_vga_mode5()` install a video mode for a range of scanlines. Each one tells the VGA: "use this mode, reading configuration from XRAM address Y, on plane Z, for scanlines BEGIN through END." This is only called once at startup — not every frame.

```c
xreg_vga_mode2(options, config, plane, begin, end);         // Mode 2: tile maps
xreg_vga_mode5(options, config, length, plane, begin, end); // Mode 5: paletted sprites
```

- `options` — color depth and tile/sprite size, built from named constants.
- `config` — the XRAM address of the mode's configuration: a `mode2_config_t`, or an array of `mode5_sprite_t`. In this project that is always an `XRAM_..._CONFIG` name from `src/xram.h`.
- `length` (Mode 5 only) — how many sprites are in the config array.
- `plane` — which of the three planes (0–2) to program.
- `begin`, `end` — the scanlines to program. An `end` of `0` means the bottom of the canvas.

Both macros come from the `xram.h` blocks in the Mode 2 and Mode 5 sections of the VGA datasheet. They are `xreg()` with the device, channel, register and mode number already filled in, which is why the mode number isn't an argument. `options` is a color depth OR'd with a size, using constants from the same blocks:

| Used for | `options` | Meaning |
|---|---|---|
| Player and enemy sprites | `MODE5_4BPP \| MODE5_16X16` | 16×16 sprites, 4bpp |
| Projectile sprites | `MODE5_4BPP \| MODE5_8X8` | 8×8 sprites, 4bpp |
| Background, foreground and HUD tiles | `MODE2_4BPP \| MODE2_8X8` | 8×8 tiles, 4bpp |

See the [VGA documentation](https://picocomputer.github.io/vga.html) for the full reference.

## Getting Started

This project is built with the [RP6502 SDK](https://picocomputer.github.io/sdk.html). The SDK documentation covers everything in this section in more depth, so keep it open alongside this tutorial.

Tool prerequisites for this repo (the first five come from the [project template's README](https://github.com/picocomputer/rp6502-sdk)):

- CMake 3.21 or newer
- Python 3
- git
- A build tool CMake can drive: GNU Make or Ninja
- [llvm-mos](https://llvm-mos.org/wiki/Welcome), installed with its own installer. Do not use an llvm-mos from a package manager; it will be too old.
- VS Code, with the extensions the project recommends (VS Code prompts for them when you open the folder): the C/C++ Extension Pack, which includes CMake Tools, to build; LLDB DAP, to debug in the emulator; and Python Debugger, to send the ROM to a Picocomputer.
- Pillow for this repo's image conversion scripts

Install Python dependency:

```bash
python3 -m pip install pillow
```

On Windows, the template's README also describes a `generator` line to add to `CMakePresets.json` before you configure for the first time.

Get started with the RP6502 project template from https://github.com/picocomputer/rp6502-sdk. Select "Use this template", then "Create a new repository". GitHub creates a new repository with a copy of the template. Clone it and open the folder in VS Code.

The first time the project opens, CMake Tools asks for a configure preset. Choose **llvm-mos/Debug**. Debug builds carry the information breakpoints and stepping need; **llvm-mos/Release** is the optimized build for a ROM you share. The template also has `cc65/Debug` and `cc65/Release` presets, but this game is written for llvm-mos, so this repo's `CMakePresets.json` keeps only the two llvm-mos presets. You can delete the cc65 presets from your copy too, along with the template's example sources you don't use (`src/main-cc65.s` and `src/main-llvm-mos.s`).

The first configure downloads the tools into `tools/`: the CMake functions, `rp6502.py`, and the emulator for your system. Commit `tools/` (the emulator binary is ignored by git) so every clone builds with the same tools. The tools change only when you update them, with the "RP6502: update tools" task (Terminal > Run Task) or from the command line:

```bash
cmake -P tools/rp6502.cmake
```

Now we are going to update CMakeLists.txt to build the demo game.  Update the contents of CMakeLists.txt to have the name of the game you want to make.  In this example, we are going to make a game called RPStarHopper.  The CMakeLists.txt file should look something like this:

```cmake
cmake_minimum_required(VERSION 3.21)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(RPStarHopper C CXX ASM)

add_executable(RPStarHopper)
rp6502_map(RPStarHopper src/xram.h "XRAM_.*")
rp6502_asset(RPStarHopper help src/help.txt)
rp6502_executable(RPStarHopper DATA default RESET default)
target_sources(RPStarHopper PRIVATE
    src/main.c
)
```

- `include()` loads the RP6502 CMake functions from `tools/`.
- `add_executable(RPStarHopper)` creates the program, a CMake target named `RPStarHopper`.
- `rp6502_map()` reads the XRAM addresses for `RPStarHopper` from `src/xram.h`. We start using it in [Adding a Sprite](#adding-a-sprite).
- `rp6502_asset()` adds an asset to the ROM. The asset named `help` is the ROM's help text, shown by the Picocomputer's HELP and INFO commands.
- `rp6502_executable()` packages the program and its assets into `RPStarHopper.rp6502`. `DATA default RESET default` takes the load and start addresses from the llvm-mos linker output.
- `target_sources()` lists the program's source files.

At this point, you should be able to build and run the project. Press F5 ("Start Debugging"), and pick a launch configuration in the Run and Debug side panel:

- **RP6502 (Emulator)** is the default. It builds the project and runs it in the emulator, with breakpoints, stepping, the call stack, variables and watch expressions. The emulator window may open behind VS Code.
- **RP6502 (Hardware)** builds the project and runs it on your Picocomputer, over USB serial or over telnet with an RP6502-RIA-W. First set `device`, and `key` for telnet, in the `[RP6502][Launch]` section of the `.rp6502` settings file in the project folder (it is created the first time the tools run). The ROM is copied to the USB drive plugged into the Picocomputer, so a drive must be plugged in. There are no breakpoints on hardware; the program's console opens in a VS Code terminal.

You can also build and run from the command line. The ROM is written to the preset's build folder, `build/llvm-mos/debug/`:

```bash
cmake --preset llvm-mos/Debug
cmake --build --preset llvm-mos/Debug

# Run in the emulator (tools/rp6502-emu.exe on Windows and WSL)
tools/rp6502-emu build/llvm-mos/debug/RPStarHopper.rp6502

# Run on a Picocomputer, using the same .rp6502 settings file as VS Code
python3 tools/rp6502.py -c .rp6502 run build/llvm-mos/debug/RPStarHopper.rp6502
```

`rp6502.py` works the same on Linux, macOS and Windows. In its terminal, Ctrl-A then X exits, and Ctrl-A then B sends a break.

## Provided Image Assets

Before we start graphics setup, here is what is already provided in `images/` and how each file is used.

### Runtime assets used by the game

| File | Size | Purpose |
|---|---:|---|
| `Player_4bpp.bin` | 768 bytes | Player sprite sheet (6 frames, 16x16, 4bpp). |
| `Projectiles_4bpp.bin` | 416 bytes | Projectiles, pickups, asteroids, and explosion frames (13 frames, 8x8, 4bpp). |
| `Enemies_4bpp.bin` | 22528 bytes | Enemy + boss sprite sheet (176 frames, 16x16, 4bpp). |
| `StarFields_tiles_4bpp.bin` | 8096 bytes | Shared tile pixel data for BG/FG/HUD; 8x8 tiles at 4bpp (currently 253 tiles present, 256 max supported). |
| `StarFields_BG_map.bin` | 2400 bytes | Background tilemap index grid (40x60, 1 byte per tile). |
| `StarFields_FG_map.bin` | 2400 bytes | Foreground tilemap index grid (40x60, 1 byte per tile). |
| `StarFields_HUD_map.bin` | 1200 bytes | HUD tilemap index grid (40x30, 1 byte per tile). |
| `StarFields_HUD_map1.bin` | 1200 bytes | ROM-named HUD map variant used for restoring HUD tiles from ROM when needed. |

### Palette helper files

These are generated helper artifacts from conversion scripts. They are useful for editing and code generation, but the game primarily consumes the `.bin` image/map assets above.

| File pattern | Purpose |
|---|---|
| `*_4bpp_palette.bin` | Raw 16-color palette data (32 bytes) for a converted asset. |
| `*_4bpp_palette.h` | C header with palette constants for compile-time use. |

Asset naming convention in this project:
- `*_4bpp.bin` = pixel data (tile/sprite frames)
- `*_map.bin` = tile index map data
- `*_palette.*` = palette helper output

### XRAM placement summary

Every XRAM asset is a member of `xram_layout_t` in `src/xram.h`, and `CMakeLists.txt` loads it with `rp6502_asset(RPStarHopper XRAM(<name>) <file>)`. The member's type sets its size, and its `XRAM_` name is the address the C code and CMake both use.

| `xram_layout_t` member | Address name | File | Size | Notes |
|---|---|---|---:|---|
| `player_data` | `XRAM_PLAYER_DATA` | `images/Player_4bpp.bin` | 768 bytes | Player sprite frames (`PLAYER_FRAME_COUNT` × `sprite_16x16_t`) |
| `starfield_bg_data` | `XRAM_STARFIELD_BG_DATA` | `images/StarFields_BG_map.bin` | 2400 bytes | BG tile index map |
| `starfield_fg_data` | `XRAM_STARFIELD_FG_DATA` | `images/StarFields_FG_map.bin` | 2400 bytes | FG tile index map |
| `starfield_hud_data` | `XRAM_STARFIELD_HUD_DATA` | `images/StarFields_HUD_map.bin` | 1200 bytes | HUD tile index map |
| `starfield_tiles_data` | `XRAM_STARFIELD_TILES_DATA` | `images/StarFields_tiles_4bpp.bin` | 8192 bytes | Shared tile pixels (`STARFIELD_TILE_COUNT` × `tile_8x8_t`) |
| `projectile_data` | `XRAM_PROJECTILE_DATA` | `images/Projectiles_4bpp.bin` | 416 bytes | Projectile/pickup/asteroid/explosion frames |
| `enemy_data` | `XRAM_ENEMY_DATA` | `images/Enemies_4bpp.bin` | 22528 bytes | Enemy + boss frames |
| `sfx_data` | `XRAM_SFX_DATA` | `music/sfx/sfx_xram.bin` | 2204 bytes | SFX command streams (`SFX_DATA_SIZE`, generated by `tools/generate_sfx.py`) |

There is no address column, because nobody writes the addresses down. The compiler works each one out from the layout, so when something grows, everything after it moves and both the C code and CMake follow. `sfx_data` is the last member so that regenerating the SFX never moves anything else.

The layout also holds XRAM the program fills in at run time rather than loading from the ROM: the OPL2 registers (`XRAM_OPL`, the first member so it starts on a page boundary), the mode configurations (`XRAM_PLAYER_CONFIG`, `XRAM_TILE_BG_CONFIG` and the rest), the 16-color palettes (`XRAM_PLAYER_PALETTE` and the rest), and the keyboard and gamepad input that the RIA writes (`XRAM_KEYBOARD`, `XRAM_GAMEPAD`).

## Setting up Graphics

The documentation for the Picocomputer is excellent:
https://picocomputer.github.io

For this demo we are going to work with a 320x240 canvas.   Let's start by initializing the graphics system.  We can do this by calling the xreg_vga_canvas function with a parameter of `CANVAS_320X240`.

`xreg_vga_canvas()` and `CANVAS_320X240` are not part of `rp6502.h`. Open the [VGA datasheet](https://picocomputer.github.io/vga.html), find the code block captioned `xram.h` in the Key Registers section, select the C tab, and use its "Copy to clipboard" button. Paste it into `src/xram.h`, after the `#include` lines. Leave the template's placeholder layout (`xram_feature_t` and `XRAM_FOO`) where it is for now; the next section replaces it with a real one.

Add the following to your main.c file, as well as ```#include <stdbool.h>``` and ```#include "xram.h"``` at the top of the file:


```c
static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(CANVAS_320X240);
    if (rc < 0) {
        puts("Error: xreg_vga_canvas(CANVAS_320X240) failed");
        return false;
    }
    return true;
}
```

The key line here is ```xreg_vga_canvas(CANVAS_320X240);``` which initializes the VGA system and sets up a canvas with a resolution of 320x240 pixels.  If the function returns a negative value, it means that there was an error initializing the graphics system, so we print an error message and return false.  If the initialization is successful, we return true.  We can now call this function from our main, and also set up a vsync loop to keep the program running.  Update your main function to look like this:

```c

uint8_t vsync_last = 0;

int main(void)
{
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;
    }

    return 0;
}
```

We have added ```vsync_last``` to keep track of the last vsync state.  In our main loop, we check if the current vsync state is the same as the last one, and if it is, we continue to the next iteration of the loop.  This effectively creates a loop that runs once per frame, synced to the vertical refresh of the display.  This is important for ensuring smooth graphics and avoiding screen tearing.

If you build and run this code, you should see a blank screen on your Picocomputer.  This means that we have successfully initialized the graphics system and are running a main loop that is synced to the vertical refresh of the display.  In the next section, we will start drawing some pixels to the screen!  To exit the program hit ```ALT + F4``` on the keyboard connected to your Picocomputer.

## Adding a Sprite

We are going to use Mode 5 Sprite system for the demo.  It's very flexible and powerful.  We are going to add a 4-bpp (16-color) sprite with a custom palette and start to get a feel for the XRAM system.   The images folder contains ```Player_4bpp.bin``` which is a 16x16 pixel tile-based Sprite in the 4-bpp format.  We will learn later how to make our own sprites and convert them to the correct format.  

We are going to load this sprite into XRAM and then draw it to the screen.  First, we need to add the sprite as an asset in our CMakeLists.txt file.  Update your CMakeLists.txt to add a new rp6502_asset for the sprite, and make sure to include the correct path to the image file.  Your CMakeLists.txt should now look like this:

```cmake
cmake_minimum_required(VERSION 3.21)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(RPStarHopper C CXX ASM)

add_executable(RPStarHopper)
rp6502_map(RPStarHopper src/xram.h "XRAM_.*")

rp6502_asset(RPStarHopper XRAM(XRAM_PLAYER_DATA) images/Player_4bpp.bin)
rp6502_asset(RPStarHopper help src/help.txt)

rp6502_executable(RPStarHopper DATA default RESET default)

target_sources(RPStarHopper PRIVATE
    src/main.c
)
```

This will place the sprite in XRAM at `XRAM_PLAYER_DATA`.  So where is that?  We decide, in `src/xram.h`.

The whole XRAM map lives in one file, `src/xram.h`, and it has two parts.

First come the structures of each device the program uses. You don't write these yourself: each one is in the [RIA](https://picocomputer.github.io/ria.html) or [VGA](https://picocomputer.github.io/vga.html) datasheet, in a code block captioned `xram.h`, together with the device's constants and XREG macro. Select the C tab and use the block's "Copy to clipboard" button to paste the whole block into `src/xram.h`. For the player sprite we need two blocks from the VGA datasheet: the Key Registers block you already copied (the canvas), and the block from the "Mode 5: Sprite 1,2,4,8-bit" section. The part of the Mode 5 block we use first is the structure that configures one sprite:

```c
typedef struct
{
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} mode5_sprite_t;
```

Then comes the layout: one `xram_layout_t` struct that places everything the program keeps in XRAM, one member after another, and one `#define XRAM_<NAME> offsetof(xram_layout_t, <member>)` that names each address. `xram_layout_t` is never created as a variable. It only describes XRAM, and `offsetof()` turns each member into its address. Replace the template's placeholder layout with this one for the player:

```c
#ifndef XRAM_H
#define XRAM_H

#include <rp6502.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constants.h"

/* VGA Canvas */
/* ...the xram.h block from the VGA datasheet's Key Registers section... */

/* VGA Mode 5: Sprite */
/* ...the xram.h block from the VGA datasheet's Mode 5 section... */

/* Star Hopper's XRAM. The sprites are 4-bit color, */
/* so the images are 16-color and each palette has 1 << 4 entries. */

typedef MODE5_IMAGE(4, 16) sprite_16x16_t;

#define PLAYER_FRAME_SIZE sizeof(sprite_16x16_t)

typedef struct
{
    mode5_sprite_t player_config;
    uint16_t player_palette[1 << 4];
    sprite_16x16_t player_data[PLAYER_FRAME_COUNT];
} xram_layout_t;

#define XRAM_PLAYER_CONFIG offsetof(xram_layout_t, player_config)
#define XRAM_PLAYER_PALETTE offsetof(xram_layout_t, player_palette)
#define XRAM_PLAYER_DATA offsetof(xram_layout_t, player_data)

#endif
```

`MODE5_IMAGE(4, 16)` comes from the Mode 5 block, and declares one 16x16 image at 4 bits per pixel (4bpp): 16 rows of 8 bytes, 128 bytes (16 * 16 * 4 bits / 8 bits per byte = 128 bytes).  The player sprite sheet is six of them, 768 bytes.  The player's palette is 16 colors of 2 bytes each, 32 bytes.  The sprite config, `mode5_sprite_t`, is 8 bytes.

There is no hex math anywhere in this file.  The compiler works out every address from the sizes of the members before it, so in this layout `XRAM_PLAYER_CONFIG` is 0, `XRAM_PLAYER_PALETTE` is 8, and `XRAM_PLAYER_DATA` is 40.  If the sprite sheet gets another frame, or we add a member in the middle, every address after it moves automatically.  CMake reads the very same names through `rp6502_map()`, so `XRAM(XRAM_PLAYER_DATA)` in CMakeLists.txt always matches `XRAM_PLAYER_DATA` in the C code, and no address is ever written twice.  `rp6502_map()` also checks the layout for you: the build fails if an address is odd (mode configurations and palettes are read as 16-bit values, so they need an even address) or if the layout is larger than the 64 KB of XRAM.  The [XRAM Memory Map](https://picocomputer.github.io/sdk.html#xram-memory-map) section of the SDK documentation describes this file in full.

The sizes and counts that the layout and the game code use live in ```constants.h```, which `xram.h` includes.  A file that includes `xram.h` gets both.

```c
#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions -- must match xreg_vga_canvas(CANVAS_320X240) in main.c
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite and tile data. The XRAM layout that holds them is src/xram.h.
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_COUNT      6                  // idle, left, right, explode frames (3, 4, 5)

#endif // CONSTANTS_H
```

We will load our custom palette data into `XRAM_PLAYER_PALETTE` and then point the VGA system to it when we set up our sprite.  Notice that in CMakeLists.txt the sprite is loaded at `XRAM(XRAM_PLAYER_DATA)` and in the C code it is at `XRAM_PLAYER_DATA`: the same 16-bit XRAM address, and `XRAM()` adds the ROM loader's `0x10000` for you.

Now let's create ```sprite_mode5.c``` and ````sprite_mode5.h```` files to handle the sprite drawing logic.  In these files, we will write functions to initialize the sprite system, load our sprite data into XRAM, and draw the sprite to the screen.  This will help us keep our main.c file clean and organized.  Here is an example of what the contents of these files might look like.  Here is the code for ```sprite_mode5.c```:

```c
#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include "xram.h"
#include "sprite_mode5.h"

void sprite_mode5_init(void) {
    int rc;
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, xram_sprite_ptr, XRAM_PLAYER_DATA);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, palette_ptr, XRAM_PLAYER_PALETTE);


    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_PLAYER_CONFIG, 1, 2, 0, 0) < 0) {
        puts("xreg_vga_mode5 failed");
        return;
    }


    RIA.addr0 = XRAM_PLAYER_PALETTE;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }


    puts("Mode5 player sprite ready");
}
```

`XRAM_PLAYER_CONFIG` is a compile-time constant from `src/xram.h`, so there is no variable to hold the config address and nothing to compute at run time.

Here is the code for ```sprite_mode5.h```.  It doesn't declare the sprite structure itself: `mode5_sprite_t` comes from `xram.h`.

```c
#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include "xram.h"

// Palette extracted from Sprites/Player.png
static const uint16_t player_palette[16] = {
    0x0000, // transparent
    0xA820,
    0x0560,
    0xAD60,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF,
    0xFABF,
    0x57FF,
    0xFFFF,
};

void sprite_mode5_init(void);

#endif // SPRITE_MODE5_H
```

We need to update our CMakeLists.txt to include the new source file:

```cmake
target_sources(RPStarHopper PRIVATE
    src/main.c
    src/sprite_mode5.c
)
```

and update main.c to look like this:

```c
#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "xram.h"
#include "sprite_mode5.h"



static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(CANVAS_320X240);
    if (rc < 0) {
        puts("Error: xreg_vga_canvas(CANVAS_320X240) failed");
        return false;
    }

    sprite_mode5_init();

    return true;
}

uint8_t vsync_last = 0;

int main(void)
{
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;
    }

    return 0;
}
```
At this point, if you build and run the code, you should see your player sprite displayed in the center of the screen!  Congratulations, you have successfully loaded a sprite into XRAM and drawn it to the screen using Mode 5!  In the next section we will learn how to convert PNGs and then adding some interactivity and movement to our sprite.

![First Milestone](Screenshots/Screenshot_001.png)

## Converting PNG Assets

Use `tools/convert_sprite.py` to convert source PNG files into binary assets for RP6502.

Prerequisite: `convert_sprite.py` uses Pillow (`PIL`). If needed:

```bash
python3 -m pip install pillow
```

Basic usage:

```bash
python3 ./tools/convert_sprite.py <input_file> [--bpp 1|2|4|8|16] [--mode tile|bitmap] [--out-dir images] [--extract-palette]
```

Key options:

- `--mode tile` (default): treats the image as a horizontal strip of square frames where `tile_size = image_height` and `image_width` must be a multiple of `image_height`.
- `--mode bitmap`: exports full scanline bitmap data (no frame splitting).
- `--bpp`: output format. If omitted, the script auto-detects from image mode/color usage.
- `--extract-palette`: for indexed formats (1/2/4/8 bpp), also writes:
    - `<name>_<bpp>bpp_palette.bin`
    - `<name>_<bpp>bpp_palette.h`

Example commands used in this project:

```bash
# Player sprite sheet (16x16 frames in a horizontal strip)
python3 ./tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/Player.png 

# Enemy sprite sheet
python3 ./tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/Enemies.png

# Projectile/asteroid sheet
python3 ./tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/Projectiles.png

# Tile set (full image treated as a bitmap, not split into frames)
python3 ./tools/convert_sprite.py --bpp 4 --mode tile --extract-palette Sprites/StarFields_tiles.png

```

Output naming convention:

- Main binary: `<name>_<bpp>bpp.bin`
- Palette binary/header (if requested): `<name>_<bpp>bpp_palette.bin` and `<name>_<bpp>bpp_palette.h`

For this repo, generated binaries are copied/renamed into the `images/` asset filenames referenced by `rp6502_asset(...)` entries in `CMakeLists.txt`.

If the BPP is not specified, the script will auto-detect based on the image mode and color usage, but may not be dependable.  Additionally, if the BPP is different that the input PNG the script will attempt to requantize the palette to match the requested BBP, which may lead to unexpected color changes.  For best results, create your source PNGs in the target color depth (for example, Indexed Color mode with 16 colors for 4bpp) and use the `--bpp` flag to explicitly specify the output format.

## Creating Graphics in Aseprite

Aseprite is a very good fit for RP6502 graphics work because the Picocomputer's tile and sprite modes are fundamentally palette-based. If you build your art as indexed-color pixel art from the start, the exported data maps cleanly into Mode 2 tiles and Mode 5 sprites.

### Why 4bpp is the sweet spot

For this project, **4bpp** has been the practical sweet spot:
- `4bpp` means 16 colors per asset palette.
- Files are half the size of `8bpp` assets and one quarter the size of `16bpp` assets.
- Smaller files use less XRAM, make ROMs smaller, and reduce the amount of data that has to be copied into XRAM.
- On RP6502, that also means less PIX/XRAM traffic whenever you stream or update pixel data.

For the same image dimensions, memory scales directly with bits per pixel:

| Asset type | 4bpp | 8bpp | 16bpp |
|---|---:|---:|---:|
| One 8x8 tile | 32 bytes | 64 bytes | 128 bytes |
| One 8x8 projectile frame | 32 bytes | 64 bytes | 128 bytes |
| One 16x16 sprite frame | 128 bytes | 256 bytes | 512 bytes |

Using this repo's actual art assets, the numbers look like this:

| Asset | Current size at 4bpp | Equivalent at 8bpp | Equivalent at 16bpp |
|---|---:|---:|---:|
| Player sprite sheet (`6` frames, `16x16`) | 768 bytes | 1536 bytes | 3072 bytes |
| Tile set (`StarFields_tiles_4bpp.bin`, currently `253` tiles, `8x8`) | 8096 bytes | 16192 bytes | 32384 bytes |
| Projectile sheet (`13` frames, `8x8`) | 416 bytes | 832 bytes | 1664 bytes |
| Enemy sheet (`176` frames, `16x16`) | 22528 bytes | 45056 bytes | 90112 bytes |

The three tile maps are index grids, so their size does **not** change with color depth:
- Background map: 2400 bytes
- Foreground map: 2400 bytes
- HUD map: 1200 bytes

That gives these total XRAM requirements for the current visual assets:

| Format choice | Total asset memory |
|---|---:|
| Current 4bpp setup | 37904 bytes |
| Same assets at 8bpp | 69616 bytes |
| Same assets at 16bpp | 133232 bytes |

Since XRAM is only 65536 bytes total, the current 4bpp setup fits, but the same art at 8bpp would already overflow XRAM before accounting for config structs, palettes, input buffers, or OPL registers. That is the strongest practical reason to stay with 4bpp unless you truly need more colors.

Note: The Picocomputer has fast access to USB flash media, so it is technically possible to stream larger assets from storage into XRAM on demand. However, that adds complexity and is out of scope for this project.

One more important detail: Picocomputer **Mode 2** tiles and **Mode 5** sprites are palette-based modes and top out at `8bpp`. A `16bpp` version of the same art would require different video modes and much more memory, so 16-bit color is usually the wrong choice for this style of game.

### Aseprite workflow for sprites

For sprite sheets such as the player, enemies, and projectiles:

1. Create the artwork in **Indexed Color** mode.
2. Keep each frame square if you plan to use `--mode tile` with `convert_sprite.py`.
3. Arrange animation frames horizontally in one strip.
4. Keep the image height equal to the frame size.

Examples from this project:
- Player sheet: `16x16` frames in a horizontal strip
- Enemy sheet: `16x16` frames in a horizontal strip
- Projectile sheet: `8x8` frames in a horizontal strip

That layout matches the converter's expectations:

```bash
python3 ./tools/convert_sprite.py Sprites/Player.png --mode tile --bpp 4 --out-dir images --extract-palette
```

### Making a tile map in Aseprite

For scrolling backgrounds and HUD layers, Aseprite's tilemap tools are a good fit.

Recommended workflow:

1. Create or import your tile artwork as an indexed-color tileset.
2. Make sure the layer you paint on is an actual **TileMap** layer in Aseprite, not a normal raster layer.
3. Paint the level or background using tile IDs instead of drawing pixels directly.
4. Export the **tileset artwork** as well as the tilemap itself. RP6502 needs both pieces: the tile pixels and the tile index grid.
5. Keep your tile size aligned with the mode you plan to use on RP6502.

For this project, the tile system uses:
- `8x8` tiles
- up to `256` tile IDs
- one byte per tile in the exported map

The key idea is that the tilemap file is just a grid of tile indices. The tile pixels live in a separate tileset binary, and the map only says which tile goes at each position.

For this project, that means you typically export two different things from Aseprite:
- The **tileset image** itself, which later becomes something like `StarFields_tiles_4bpp.bin`
- The **tilemap layer**, which becomes something like `StarFields_BG_map.bin`

Both are required. The map by itself is only tile numbers; it does not contain the actual tile pixel art.

### Exporting tilemaps with `tools/export_map.lua`

This project includes [tools/export_map.lua](/Users/rowe/Software/rp6502/RPDemo/tools/export_map.lua), which is the exact tool used to export the game's tilemaps.

What the script does:
- Reads the **active sprite** in Aseprite
- Requires the **active layer** to be a **tilemap layer**
- Reads the **active frame**
- Exports the tilemap as raw bytes, one byte per tile, row by row

Important quirk:
- The script exports the tilemap based on Aseprite's **bounding box** for the tile content, not a fixed logical map size.
- In practice, that means you should place a non-transparent tile at the **top-left** corner and another at the **bottom-right** corner of the map area you want exported.
- If you do not do this, Aseprite may crop the exported tilemap smaller than you expected.

That exported `.bin` file can be used directly as an RP6502 tilemap asset.

Example flow:

1. Open your Aseprite tilemap file.
2. Confirm that the layer you want to export is a **TileMap** layer.
3. Export the tileset artwork separately.
4. Select the tilemap layer you want to export.
5. Make sure there is a non-transparent tile at the top-left and bottom-right corners of the area you want exported.
6. Run the Lua script from Aseprite.
7. Save the output as something like `StarFields_BG_map.bin`.
8. Add a member for each to `xram_layout_t` in `src/xram.h`, with its `XRAM_` define, sized from the map's width and height in tiles (see [Tilemaps and Backgrounds](#tilemaps-and-backgrounds)).
9. Add both the tileset asset and the tilemap asset to `CMakeLists.txt`, loaded at those names.

Example:

```c
// src/xram.h, inside xram_layout_t
uint8_t starfield_bg_data[STARFIELD_BG_HEIGHT][STARFIELD_BG_WIDTH];

// src/xram.h, after xram_layout_t
#define XRAM_STARFIELD_BG_DATA offsetof(xram_layout_t, starfield_bg_data)
```

```cmake
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_BG_DATA) images/StarFields_BG_map.bin)
```

The script writes one byte per tile ID, so it naturally matches Mode 2 tilemap data:

```c
struct {
    uint8_t tile_id;
} data[width_tiles * height_tiles];
```

That direct mapping is the main reason this workflow is nice: what you paint in Aseprite as a tilemap becomes exactly what the RP6502 tile engine expects in XRAM.

### Study the source art

The [Sprites](/Users/rowe/Software/rp6502/RPDemo/Sprites) folder contains the Aseprite source files used to build this game. These are useful reference material if you are learning the workflow or want to reuse the same setup for your own project.

Relevant files include:
- `Player.aseprite`
- `Enemies.aseprite`
- `Projectiles.aseprite`
- `StarFields.aseprite`
- `Boss_001.aseprite` through `Boss_07.aseprite`

If you want to learn how the graphics were organized, start there. You can inspect frame layout, palette usage, tilesets, and tilemap structure directly in Aseprite instead of guessing from the exported binary files.

### Practical advice

- Use indexed color early. Converting full-color art down later is usually painful.
- Keep your tile and sprite palettes intentional and limited.
- Prefer 4bpp unless there is a concrete visual reason to move to 8bpp.
- Treat tilemaps and tilesets as separate assets: one file for tile pixels, one file for tile placement.
- If an asset is going into Mode 2 or Mode 5, think in terms of palettes and tile/sprite frames, not full-color bitmaps.

## Input System

We are going to use ```input.c```, ```input.h```, ```player_controller.c```, and ```player_controller.h``` which are designed to make handling inputs a bit easier and also allow for custom key mappings for any gamepad you want to use.  I strongly recommend reading the Picocomputer documentation.  To get started add ```input.c``` and ```player_controller.c``` to CMakeLists.txt and include the headers in main.c.  

We need to allocate XRAM to fetch the current state of the inputs.  The RIA writes the keyboard and gamepad state into XRAM continuously, but XRAM has no fixed map: the program chooses where that state goes and tells the RIA with an XREG.  In our `src/xram.h` file, that means adding the input devices to the layout.

First copy two more `xram.h` blocks, both from the [RIA datasheet](https://picocomputer.github.io/ria.html): the one in the Keyboard section, which defines `keyboard_t` (a 32-byte bit array of USB HID keycodes) and `xreg_ria_keyboard()`, and the one in the Gamepads section, which defines `gamepad_t` (10 bytes for each of 4 gamepads, 40 bytes) and `xreg_ria_gamepad()`.  Then add a member for each to `xram_layout_t`, with a name for each address:

```c
typedef struct
{
    mode5_sprite_t player_config;

    uint16_t player_palette[1 << 4];

    keyboard_t keyboard;
    gamepad_t gamepad;

    sprite_16x16_t player_data[PLAYER_FRAME_COUNT];
} xram_layout_t;

#define XRAM_PLAYER_CONFIG offsetof(xram_layout_t, player_config)

#define XRAM_PLAYER_PALETTE offsetof(xram_layout_t, player_palette)

#define XRAM_KEYBOARD offsetof(xram_layout_t, keyboard)
#define XRAM_GAMEPAD offsetof(xram_layout_t, gamepad)

#define XRAM_PLAYER_DATA offsetof(xram_layout_t, player_data)
```

The input members go before the sprite data, which pushes `XRAM_PLAYER_DATA` up by 72 bytes.  Nothing else needs to change: the C code and `XRAM(XRAM_PLAYER_DATA)` in CMakeLists.txt both pick up the new address on the next build.  `input.c` reads the state from `XRAM_KEYBOARD` and `XRAM_GAMEPAD`, so `input.h` includes `xram.h` too.

and then update your main function look like:

```c
int main(void)
{

    // Initialize input
    xreg_ria_keyboard(XRAM_KEYBOARD);
    xreg_ria_gamepad(XRAM_GAMEPAD);

    // Initialize graphics
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }
    init_input_system();
    player_controller_init();

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        // 3. UPDATE
        player_controller_update();
    }

    return 0;
}
```

Let's break this down.  
```c
    // Initialize input
    xreg_ria_keyboard(XRAM_KEYBOARD);
    xreg_ria_gamepad(XRAM_GAMEPAD);
```
This sets the XRAM addresses for the keyboard and gamepad inputs and enables the devices.  `xreg_ria_keyboard()` and `xreg_ria_gamepad()` are the XREG macros from the blocks we just copied; each one is `xreg()` with the RIA's device, channel and register filled in.  

```c
    init_input_system();
    player_controller_init();
```
This initializes our input handling system and our player controller.  The input system will also look for `JOYSTICK_SH.DAT` and if it exists, it will load custom key mappings from that file.  This allows you to set up custom key mappings for any gamepad you want to use with your Picocomputer.

### Mapping any gamepad with `GamepadMapper`

This repo includes a small utility program (`src/gamepad_mapper.c`) that lets you map controls for any gamepad and save the result to `JOYSTICK_SH.DAT`.

What it does:
- Prompts you for each in-game action (`MOVE UP`, `MOVE DOWN`, `MOVE LEFT`, `MOVE RIGHT`, `BUTTON A/B/X/Y`, `BUTTON LT/RT`, `SELECT`, `START`).
- Records the actual gamepad field/mask values for the button you press.
- Writes the mapping file `JOYSTICK_SH.DAT` to storage.

At game startup, `init_input_system()` in `input.c` automatically loads `JOYSTICK_SH.DAT` (if present), so your custom mapping is applied without any code changes.

Recommended workflow:

1. Build the `GamepadMapper` target.
2. Run/upload `GamepadMapper` on the Picocomputer.
3. Follow the on-screen prompts and press the requested control for each action.
4. Confirm `JOYSTICK_SH.DAT` was saved.
5. Run the main game; it will pick up that mapping automatically.

If `JOYSTICK_SH.DAT` is missing or invalid, the game falls back to the default mappings in `reset_button_mappings()`.

In our VSYNC loop we have added:

```c
// 2. INPUT
        handle_input();

        // 3. UPDATE
        player_controller_update();
```
The function ```handle_input()``` will read the current state of the inputs from XRAM and update the internal state of the input system.  The function ```player_controller_update()``` will read the current input state and update the position of the player sprite accordingly.  You can customize the logic in ```player_controller_update()``` to create different movement patterns or add additional actions based on the inputs.

In ```sprite_mode5.c``` we add function to update the sprite location:

```c
void sprite_mode5_set_position(int16_t x, int16_t y)
{
    // Clamp X to valid screen range (0 to SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX)
    if (x < 0) x = 0;
    if (x > (int16_t)(SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX)) {
        x = (int16_t)(SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Clamp Y to valid screen range (0 to SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)
    if (y < 0) y = 0;
    if (y > (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) {
        y = (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Update sprite position in XRAM
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, y);
}
```

and add an extern to the header file:

```c
void sprite_mode5_set_position(int16_t x, int16_t y);
```

The key here is,

```c
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, y);
```
We are directly updating the XRAM values for the sprite's position.  This is a powerful feature of the Picocomputer, as it allows us to update sprite properties directly from our game logic without needing to make expensive system calls.  By writing directly to XRAM, we can achieve very fast updates to our sprites, which is essential for smooth gameplay.

At this point, you should be able to move your player sprite around the screen using the D-pad or controller sticks.  You can customize the input handling logic in ```player_controller_update()``` or replace it with your own control scheme. 

## Tilemaps and Backgrounds

Each layer can have a fill and sprite component.  So far we have added a sprite to layer 2 (top).  Now we are going to add tiles to layers 0, 1 and 2 to add a slowly moving background, with a faster foreground to create a parallax effect.  Then we add a top layer for a HUD to show a score.  

This will be done with ```tile_mode2.c``` and ```tile_mode2.h```.  You can add ```tile_mode2.c``` to your CMakeLists.txt, add the header to main.c, and add ```tile_mode2_init();``` to ```init_graphics()``` just like we did with the sprite system.  In ```main.c``` we add ```tile_mode2_update_scroll();``` to our main loop which will update the scroll position of the tilemaps to create a parallax scrolling effect.  The tile data is stored in XRAM and we can update it directly from our game logic just like we did with the sprites.  This allows us to create dynamic backgrounds that can change based on the player's actions or the game's state. 

```c
int main(void)
{

    // Initialize input
    xreg_ria_keyboard(XRAM_KEYBOARD);
    xreg_ria_gamepad(XRAM_GAMEPAD);

    // Initialize graphics
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }
    init_input_system();
    player_controller_init();

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        // 3. UPDATE
        tile_mode2_update_scroll();
        player_controller_update();
    }

    return 0;
}
```

This code will not work until we set up the tilemaps and load the tile data into XRAM.  Let's start by looking at the assets we need for the tilemaps.  We have a number of new assets:
- ```images/StarFields_BG_map.bin``` - This contains the tile index for each 8x8 tile in the background layer (layer 0).  It is a 40x60 tilemap.  We can have up to 256 tiles, so we need 1 byte per tile, which means this tilemap requires 2400 bytes of memory (40 tiles * 60 tiles * 1 byte per tile = 2400 bytes).  Notice that the tilemap is larger than the screen size, this allows us to scroll the background to create a parallax effect.
- ```images/StarFields_FG_map.bin``` - This will be our foreground layer (layer 1) and it is also a 40x60 tilemap with 1 byte per tile, so it also requires 2400 bytes of memory.
- ```images/StarFields_HUD_map.bin``` - This will be our HUD layer (layer 2) and will be a 40x30 tilemap, since we don't need to scroll it.  
- ```images/StarFields_tiles_4bpp.bin``` - This contains the pixel data for our tiles.  Each tile is 8x8 pixels and we are using a 4bpp format, which means each pixel takes up 4 bits, so we can fit two pixels in one byte.  Therefore, each tile requires 32 bytes of memory (8 * 8 * 4 bits / 8 bits per byte = 32 bytes).  The engine layout reserves space for up to 256 tiles (8192 bytes), while the current file contains 253 tiles (8096 bytes).  

Note, we are going to share 1 set of tiles for all 3 layers, but you can have a different tileset for each layer if you want.  We will learn how to generate tile maps later on, for now we are just learning how to use them.  The tilemaps and tileset are loaded into XRAM as assets in our CMakeLists.txt file, just like we did with the sprite.  We will then set up the tilemaps in XRAM and point the VGA system to them.  Once that is done, we can update the scroll position of the tilemaps in our main loop to create a parallax scrolling effect.

Let's look at part of the code for initializing the tilemaps in ```tile_mode2.c```, which includes ```xram.h```:

```c
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, x_wrap, true);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, y_wrap, true);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, x_pos_px, 0);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, y_pos_px, 0);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, width_tiles,  STARFIELD_BG_WIDTH);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, height_tiles, STARFIELD_BG_HEIGHT);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, xram_data_ptr,    XRAM_STARFIELD_BG_DATA); // tile ID grid
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, xram_palette_ptr, XRAM_TILE_BG_PALETTE);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, xram_tile_ptr,    XRAM_STARFIELD_TILES_DATA);  

    // Mode 2 args: OPTIONS, CONFIG, PLANE, BEGIN, END
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_BG_CONFIG, 0, HUD_TOP_PX, 0) < 0) {
        puts("xreg_vga_mode2 failed");
        return;
    }
```

Let's look at this step by step.
```c
#define XRAM_TILE_BG_CONFIG offsetof(xram_layout_t, tile_bg_config)
```
We are setting up the tilemap configuration in XRAM.  `XRAM_TILE_BG_CONFIG` is the address of the `tile_bg_config` member we add to `xram_layout_t` below, placed just after the sprite configuration.  There is no config variable and no address arithmetic in ```tile_mode2.c```: the layout decides where the configuration for the background layer (layer 0) goes, and the compiler works out its address.  


```c
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, x_wrap, true);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, y_wrap, true);
```
We set the wrapping mode for both X and Y to true, which means that when we scroll the tilemap, it will wrap around to the other side. 

```c
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, x_pos_px, 0);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, y_pos_px, 0);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, width_tiles,  STARFIELD_BG_WIDTH);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, height_tiles, STARFIELD_BG_HEIGHT);
```
We set the initial position of the tilemap to (0, 0) and we specify the width and height of the tilemap in tiles.  


```c
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, xram_data_ptr,    XRAM_STARFIELD_BG_DATA); // tile ID grid
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, xram_palette_ptr, XRAM_TILE_BG_PALETTE);
    xram0_struct_set(XRAM_TILE_BG_CONFIG, mode2_config_t, xram_tile_ptr,    XRAM_STARFIELD_TILES_DATA); 
```
We then point to the XRAM address where our tilemap data is stored, as well as the address of our palette and our tileset.  

```c
    xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_BG_CONFIG, 0, HUD_TOP_PX, 0)
```
We then enable the tilemap layer by calling `xreg_vga_mode2()`, which programs Mode 2, the tilemap mode.  The options `MODE2_4BPP | MODE2_8X8` select 8x8 tiles with an 4-bit color index (which allows for up to 16 colors in our palette).  We specify that this is plane 0, which means it will be behind the sprites on plane 1.  We also specify the begin and end scanlines for this layer, in this case we begin at `HUD_TOP_PX`, excluding the top 24 scanlines to leave room for our HUD layer.

We repeat this process for the foreground layer (layer 1) and the HUD layer (layer 2), each with its own configuration member in the layout: 

```c
    xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_FG_CONFIG, 1, HUD_TOP_PX, 0);
    xreg_vga_mode2(MODE2_4BPP | MODE2_8X8, XRAM_TILE_HUD_CONFIG, 2, 0, 0);
```

Next we update ```src/xram.h``` to include the new assets and the XRAM layout for the tilemaps.  Copy the `xram.h` block from the "Mode 2: Tile" section of the [VGA datasheet](https://picocomputer.github.io/vga.html), which defines `mode2_config_t`, `xreg_vga_mode2()`, the `MODE2_*` options and `MODE2_TILE()`.  Then add a configuration, a palette, and the data for each tile plane to the layout.  Only the part after the datasheet blocks is shown:

```c
/* Star Hopper's XRAM. Every asset and config is 4-bit color, */
/* so the images are 16-color and each palette has 1 << 4 entries. */

typedef MODE5_IMAGE(4, 16) sprite_16x16_t;
typedef MODE2_TILE(4, 8) tile_8x8_t;

#define PLAYER_FRAME_SIZE sizeof(sprite_16x16_t)

typedef struct
{
    mode5_sprite_t player_config;
    mode2_config_t tile_bg_config;
    mode2_config_t tile_fg_config;
    mode2_config_t tile_hud_config;

    uint16_t player_palette[1 << 4];
    uint16_t tile_bg_palette[1 << 4];
    uint16_t tile_fg_palette[1 << 4];
    uint16_t tile_hud_palette[1 << 4];

    keyboard_t keyboard;
    gamepad_t gamepad;

    /* Loaded from the ROM by CMakeLists.txt. */
    sprite_16x16_t player_data[PLAYER_FRAME_COUNT];
    uint8_t starfield_bg_data[STARFIELD_BG_HEIGHT][STARFIELD_BG_WIDTH];
    uint8_t starfield_fg_data[STARFIELD_FG_HEIGHT][STARFIELD_FG_WIDTH];
    uint8_t starfield_hud_data[STARFIELD_HUD_HEIGHT][STARFIELD_HUD_WIDTH];
    tile_8x8_t starfield_tiles_data[STARFIELD_TILE_COUNT];
} xram_layout_t;

#define XRAM_PLAYER_CONFIG offsetof(xram_layout_t, player_config)
#define XRAM_TILE_BG_CONFIG offsetof(xram_layout_t, tile_bg_config)
#define XRAM_TILE_FG_CONFIG offsetof(xram_layout_t, tile_fg_config)
#define XRAM_TILE_HUD_CONFIG offsetof(xram_layout_t, tile_hud_config)

#define XRAM_PLAYER_PALETTE offsetof(xram_layout_t, player_palette)
#define XRAM_TILE_BG_PALETTE offsetof(xram_layout_t, tile_bg_palette)
#define XRAM_TILE_FG_PALETTE offsetof(xram_layout_t, tile_fg_palette)
#define XRAM_TILE_HUD_PALETTE offsetof(xram_layout_t, tile_hud_palette)

#define XRAM_KEYBOARD offsetof(xram_layout_t, keyboard)
#define XRAM_GAMEPAD offsetof(xram_layout_t, gamepad)

#define XRAM_PLAYER_DATA offsetof(xram_layout_t, player_data)
#define XRAM_STARFIELD_BG_DATA offsetof(xram_layout_t, starfield_bg_data)
#define XRAM_STARFIELD_FG_DATA offsetof(xram_layout_t, starfield_fg_data)
#define XRAM_STARFIELD_HUD_DATA offsetof(xram_layout_t, starfield_hud_data)
#define XRAM_STARFIELD_TILES_DATA offsetof(xram_layout_t, starfield_tiles_data)
```

Each tile map is one byte per tile, so a 40x60 map is `uint8_t [60][40]`, 2400 bytes, stored row by row just as the map file is.  `MODE2_TILE(4, 8)` declares one 8x8 tile at 4bpp, 32 bytes, and the tile set reserves room for `STARFIELD_TILE_COUNT` of them.  The sizes come from ```constants.h```:

```c
#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions -- must match xreg_vga_canvas(CANVAS_320X240) in main.c
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite and tile data. The XRAM layout that holds them is src/xram.h.
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels
#define PLAYER_FRAME_COUNT      6                  // idle, left, right, explode frames (3, 4, 5)

#define STARFIELD_BG_WIDTH      40                 // Width of starfield background in tiles
#define STARFIELD_BG_HEIGHT     60                 // Height of starfield background in tiles

#define STARFIELD_FG_WIDTH      40                 // Width of starfield foreground in tiles
#define STARFIELD_FG_HEIGHT     60                 // Height of starfield foreground in tiles

#define STARFIELD_HUD_WIDTH     40                 // Width of starfield HUD in tiles
#define STARFIELD_HUD_HEIGHT    30                 // Height of starfield HUD in tiles
#define STARFIELD_HUD_SIZE      (STARFIELD_HUD_WIDTH * STARFIELD_HUD_HEIGHT) // 1200 bytes

#define STARFIELD_TILE_COUNT    256                // 8x8 4bpp tiles shared by all three tile planes

#define HUD_TOP_PX              24                  // Rows 0-23 are HUD; bullets expire when y < HUD_TOP_PX

#endif // CONSTANTS_H
```
Notice how the layout now holds all of our XRAM, including the sprite data, tilemap data, palette data and input, grouped as configurations, then palettes, then input, then the data loaded from the ROM. This allows us to easily keep track of where everything is in memory and avoid any conflicts, because two members of a struct can never overlap. You may notice that our player sprite is actually 3 frames of animation (idle, left, right) which is why we have allocated 384 bytes for the player sprite data (3 frames * 128 bytes per frame = 384 bytes). We will show how to update the sprite data in XRAM to animate the player in a later section. We have also defined some constants for the screen dimensions and the size of our sprite and tilemaps.

There is no hex math to do and nothing to copy by hand: every asset is loaded at its `XRAM_` name. Next we update CMakeLists.txt to include the new assets:

```cmake
rp6502_asset(RPStarHopper XRAM(XRAM_PLAYER_DATA) images/Player_4bpp.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_BG_DATA) images/StarFields_BG_map.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_FG_DATA) images/StarFields_FG_map.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_HUD_DATA) images/StarFields_HUD_map.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_TILES_DATA) images/StarFields_tiles_4bpp.bin)
```

If we look back at our main loop, we are calling ```tile_mode2_update_scroll();``` every frame.  This function will update the scroll position of the tilemaps to create a parallax scrolling effect.  The background layer will scroll slower than the foreground layer, which creates a sense of depth and movement in the scene.  You can customize the scrolling logic in ```tile_mode2_update_scroll()``` to create different scrolling patterns or to scroll based on player movement or other game events.  With the tilemaps set up and scrolling, you should now see a starfield background with a faster scrolling foreground layer, and a HUD layer at the top of the screen. 

![Scrolling background](Screenshots/Screenshot_002.png)

## Music

The Picocomputer has a built-in OPL2 emulator which is very powerful and can create a wide range of sounds and music. You can use the OPL2 to create music for your game, and you can also use it to create sound effects. We are going to use VGM music files for our game, which is a common format for chiptune music. You can find a large library of VGM music files online, or you can create your own using a tracker software like Furnace. The VGM is streamed from the disk, so you can have long music tracks without taking up valuable XRAM space.

### Making Music in Furnace (OPL2/YM3812 -> VGM)

For this project, compose directly for **YM3812 (OPL2)** in Furnace, then export as **VGM**.

Recommended workflow:

1. Start a new song in Furnace and set the target sound chip to **Yamaha YM3812 (OPL2)**.
2. Build your instruments and patterns with OPL2 in mind (9 FM channels, classic OPL2 timbre).
3. Set tempo/speed and loop points in Furnace so your stage music loops cleanly.
4. Export to **.vgm** (not .vgz) and test the result in-game.

Practical notes:

- In Furnace, go to **File -> Manage chips**, add **Yamaha YM3812 (OPL2)**, and remove any other chips that may be loaded.
- For a quick instrument start, find `Apogee-IMF-90.wopl`. In Furnace, open the **Instruments** tab, choose **Open**, and load the `.wopl` bank for classic Apogee-style OPL2 patches.
- Have fun and experiment with chip effects early, especially **arpeggio**, to get to playable chiptune ideas quickly.
- Keep the source module file (`.fur`) in your project for future edits.
- Export each track as `.vgm` for runtime playback.
- If you receive `.vgz` files, unzip them first; the game player expects raw `.vgm`.
- Prefer OPL2-only content when authoring so playback matches the target hardware path.

Suggested asset flow:

- Author and save: `music/MyTrack.fur`
- Export: `music/MyTrack.vgm`
- Add as ROM asset in CMake with a `RESOURCE.###.vgm` name for runtime loading.

The key files are ```music.c```, ```opl.c```, ```vgm.c``` and their corresponding header files.  You can add these to your CMakeLists.txt and include the headers in main.c.  The music system will read the VGM file from the disk and stream the OPL commands to the sound chip in real time. VGM tracks are stored as named ROM assets (e.g. `RESOURCE.001.vgm`) and opened at runtime via `open("ROM:RESOURCE.001.vgm", O_RDONLY)`. 

### Development vs Distribution

There are two good ways to load assets on the Picocomputer, and it is worth using each at the right time.

During **development and debugging**, prefer loading music and other large assets as normal external files from the filesystem.  The practical reason is speed: if you change one file, it is much faster to copy that file to the SD card or USB storage than it is to rebuild and upload the entire ROM over a terminal connection.

During **final distribution**, prefer packaging those assets into the ROM and opening them with the `ROM:` prefix.  That gives you a single self-contained game image with the executable, help text, music, and other assets all bundled together.

In practice, the same game code can support both styles:

```c
// Development: read from external storage
music_set_track("music/RESOURCE.001.vgm");

// Finished release: read from the bundled ROM asset
music_set_track("ROM:RESOURCE.001.vgm");
```

Recommended workflow:
- Use external files while iterating quickly on music, art, and data files.
- Switch to `ROM:` paths when you are preparing a finished build to share with other people.

If you want CMake to skip packaging named ROM assets while developing, a simple toggle works well.

In `CMakeLists.txt`:

```cmake
option(RPSTARHOPPER_PACKAGE_ROM_ASSETS "Bundle named ROM assets" ON)

if (RPSTARHOPPER_PACKAGE_ROM_ASSETS)
    target_compile_definitions(RPStarHopper PRIVATE ASSET_PREFIX="ROM:")

    rp6502_asset(RPStarHopper RESOURCE.001.vgm music/RESOURCE.001.vgm)
    rp6502_asset(RPStarHopper RESOURCE.002.vgm music/RESOURCE.002.vgm)
    # ...other named ROM assets...
else()
    target_compile_definitions(RPStarHopper PRIVATE ASSET_PREFIX="")
endif()
```

In code, build paths from one prefix:

```c
#ifndef ASSET_PREFIX
#define ASSET_PREFIX "ROM:"
#endif

#define TRACK_PATH(name) ASSET_PREFIX name

music_set_track(TRACK_PATH("RESOURCE.001.vgm"));
```

Then configure per build:

```bash
# Development: do not package named ROM assets
cmake --preset llvm-mos/Debug -DRPSTARHOPPER_PACKAGE_ROM_ASSETS=OFF

# Distribution: package named ROM assets and use ROM: prefix
cmake --preset llvm-mos/Release -DRPSTARHOPPER_PACKAGE_ROM_ASSETS=ON
```

For development builds, upload only changed files instead of a full ROM image. The helper script supports direct file upload and a reusable serial config file:

```bash
# Creates/uses .rp6502 with saved device settings
python3 ./tools/rp6502.py --config .rp6502 upload music/RESOURCE.001.vgm
```

If you upload to a subdirectory on USB media, include that directory in runtime paths (for example `music/RESOURCE.001.vgm`).

The OPL2 needs XRAM too: 256 bytes that will contain all the OPL2 registers.  Copy the `xram.h` block from the "Yamaha OPL2 FM Sound Generator" section of the [RIA datasheet](https://picocomputer.github.io/ria.html), which defines `opl_t` and `xreg_ria_opl()`, into ```src/xram.h```.  Then add an `opl_t` member to the layout, as the very first member, and give it a name:

```c
typedef struct
{
    /* First, so the OPL2 registers start on a page boundary. */
    opl_t opl;

    mode5_sprite_t player_config;
    /* ...the rest of the layout, unchanged... */
} xram_layout_t;

#define XRAM_OPL offsetof(xram_layout_t, opl)
_Static_assert((XRAM_OPL & 0xFF) == 0, "The OPL2 registers must start on a page boundary.");
```

The OPL2 registers must start on a page boundary: an address whose low byte is `00`, such as `0x0000` or `0x4200`.  Offset 0 is always a page boundary, which is why `opl` goes first.  `rp6502_map()` checks that every address is even, but it cannot check page alignment, so ```src/xram.h``` checks it with `_Static_assert`: if a later change ever moves `opl` off a page boundary, the build fails with that message.  Everything after `opl` moves up by 256 bytes, and as before, the C code and CMake follow automatically.

The music system enables the OPL2 at that address in `music_init()`.  `opl_config()` in ```opl.c``` makes the XREG call:

```c
// music.c, music_init()
opl_config(1, XRAM_OPL);

// opl.c
void opl_config(uint8_t enable, uint16_t addr) {
    (void)enable;
    xreg_ria_opl(addr);
}
```

In ```main.c``` we simply need to add ```music_init();```
and ```music_update();``` to play our music track, so our main loop will look like this:

```c
int main(void)
{

    // Initialize input
    xreg_ria_keyboard(XRAM_KEYBOARD);
    xreg_ria_gamepad(XRAM_GAMEPAD);

    // Initialise graphics
    if (!init_graphics()) {
        puts("Fatal: graphics initialization failed");
        return 1;
    }
    music_init();  // <- Add this line to initialize the music system
    init_input_system();
    player_controller_init();

    // Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        // 3. UPDATE
        music_update(); // <- Add this line to update the music system
        tile_mode2_update_scroll();
        player_controller_update();
    }

    return 0;
}
```

Note that the player only supports VGM files that use OPL2 commands.  The player does not support VGZ directly.  VGZ is simply gzipped VGM, so you can convert a VGZ file to VGM by unzipping it.  

This is great!  We have implemented sprites, tilemaps, and music in our game!  We have a lot of tools at our disposal to create a fun and engaging game.  In the next section, we will start adding some polish to our game by implementing animations and palette swapping effects.

## Animations and Palette Swapping

We can now add frame-based animation to our player sprite by updating the sprite data in XRAM.  We can also create palette swapping effects by updating the palette data in XRAM.  This allows us to create a wide range of visual effects without needing to make expensive system calls, as we are directly manipulating the data in XRAM that the VGA system is using to render the sprites and tiles. This is the main reason for using Mode-5 for our sprites.  It saves XRAM and provides very fast updates for animations and palette swaps.

If you look at ```Player_4bpp.bin``` you will see that it contains 3 frames of animation for the player sprite: an idle frame, a left movement frame, and a right movement frame.  Each frame is 128 bytes (16x16 pixels at 4bpp), so the total size of the sprite data is 384 bytes.  We can update the sprite's current frame by changing the XRAM address that the VGA system is using to fetch the sprite data.  This allows us to create animations by simply updating the frame index in XRAM. 

We can also create palette swapping effects by updating the palette data in XRAM.  For example, we could change the player's colors when they take damage or pick up a power-up by updating the palette entries in XRAM.  This allows us to create dynamic visual effects without needing to change the sprite data itself, which can save memory and allow for more complex animations.  

In this example, we will change palettes to show when the player is moving up, down, left or right.  This will give the player some visual feedback on their movement and make the game feel more responsive.


```c
void player_controller_update(void)
{
    // Tap LT to decrease speed by 1, tap RT to increase speed by 1.
    bool speed_down_now = is_action_pressed(0, ACTION_BTN_LT);
    bool speed_up_now = is_action_pressed(0, ACTION_BTN_RT);

    if (speed_down_now && !prev_speed_down) {
        player_controller_set_speed(player_speed - 1);
    }
    if (speed_up_now && !prev_speed_up) {
        player_controller_set_speed(player_speed + 1);
    }
    prev_speed_down = speed_down_now;
    prev_speed_up = speed_up_now;

    bool moving_up = is_action_pressed(0, ACTION_MOVE_UP);
    bool moving_down = is_action_pressed(0, ACTION_MOVE_DOWN);
    bool moving_left = is_action_pressed(0, ACTION_MOVE_LEFT);
    bool moving_right = is_action_pressed(0, ACTION_MOVE_RIGHT);

    int32_t speed_q8 = SPEED_TO_Q8(player_speed);

    if (moving_up)    player_y_q8 -= speed_q8;
    if (moving_down)  player_y_q8 += speed_q8;
    if (moving_left)  player_x_q8 -= speed_q8;
    if (moving_right) player_x_q8 += speed_q8;

    sprite_mode5_update_engine(moving_down);

    if (moving_left && !moving_right) {
        sprite_mode5_set_frame(2);
    } else if (moving_right && !moving_left) {
        sprite_mode5_set_frame(1);
    } else {
        sprite_mode5_set_frame(0);
    }

    int32_t max_x_q8 = ((int32_t)(SCREEN_WIDTH  - PLAYER_SPRITE_SIZE_PX)) << Q8_SHIFT;
    int32_t max_y_q8 = ((int32_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) << Q8_SHIFT;

    if (player_x_q8 < 0)         player_x_q8 = 0;
    if (player_x_q8 > max_x_q8) player_x_q8 = max_x_q8;
    if (player_y_q8 < 0)         player_y_q8 = 0;
    if (player_y_q8 > max_y_q8) player_y_q8 = max_y_q8;

    sprite_mode5_set_position((int16_t)(player_x_q8 >> Q8_SHIFT), (int16_t)(player_y_q8 >> Q8_SHIFT));
}
```

We then use the boolean flags for movement to determine which frame of the sprite to show.  If the player is moving left, we show the left movement frame, if they are moving right we show the right movement frame, and if they are not moving left or right we show the idle frame.  This allows us to create a simple animation for the player sprite based on their movement.  You can expand on this logic to create more complex animations or to change the palette based on different conditions.  Here is our updated ```sprite_mode5.c``` file with the animation and palette swapping logic added:

```c
#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include "xram.h"
#include "sprite_mode5.h"

static uint8_t player_frame = 0;
static uint8_t engine_phase = 0;
static uint8_t engine_tick = 0;

#define PLAYER_ENGINE_PALETTE_INDEX 12
#define ENGINE_ANIM_TICK_FRAMES 4

static const uint16_t engine_colors[3] = {
    0x52BF,
    0x57FF,
    0xFFFF,
};

static void sprite_mode5_write_palette_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = (unsigned)(XRAM_PLAYER_PALETTE + ((unsigned)index * sizeof(uint16_t)));
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

void sprite_mode5_init(void) {
    int rc;
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, xram_sprite_ptr, XRAM_PLAYER_DATA);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, palette_ptr, XRAM_PLAYER_PALETTE);
    player_frame = 0;


    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_PLAYER_CONFIG, 1, 2, 0, 0) < 0) {
        puts("xreg_vga_mode5 failed");
        return;
    }


    RIA.addr0 = XRAM_PLAYER_PALETTE;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }

    sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, 0x0000);


    puts("Mode5 player sprite ready");
}

/**
 * Update sprite position on screen
 * Clamps position to screen bounds
 */
void sprite_mode5_set_position(int16_t x, int16_t y)
{
    // Clamp X to valid screen range (0 to SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX)
    if (x < 0) x = 0;
    if (x > (int16_t)(SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX)) {
        x = (int16_t)(SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Clamp Y to valid screen range (0 to SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)
    if (y < 0) y = 0;
    if (y > (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX)) {
        y = (int16_t)(SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX);
    }
    
    // Update sprite position in XRAM
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(XRAM_PLAYER_CONFIG, mode5_sprite_t, y_pos_px, y);
}

void sprite_mode5_set_frame(uint8_t frame_index)
{
    if (frame_index >= PLAYER_FRAME_COUNT) {
        frame_index = 0;
    }
    if (frame_index == player_frame) {
        return;
    }

    player_frame = frame_index;
    xram0_struct_set(
        XRAM_PLAYER_CONFIG,
        mode5_sprite_t,
        xram_sprite_ptr,
        (XRAM_PLAYER_DATA + ((unsigned)frame_index * PLAYER_FRAME_SIZE))
    );
}

void sprite_mode5_update_engine(bool moving_down)
{
    if (moving_down) {
        engine_tick = 0;
        sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, 0x0000);
        return;
    }

    if (engine_tick == 0) {
        sprite_mode5_write_palette_entry(PLAYER_ENGINE_PALETTE_INDEX, engine_colors[engine_phase]);
        engine_phase = (uint8_t)((engine_phase + 1) % 3);
    }

    engine_tick = (uint8_t)((engine_tick + 1) % ENGINE_ANIM_TICK_FRAMES);
}
```

and updated header file:

```c
#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

#include <stdbool.h>
#include "xram.h"

// Palette extracted from Sprites/Player.png
static const uint16_t player_palette[16] = {
    0x0000, // transparent
    0xA820,
    0x0560,
    0xAD60,
    0x0035,
    0xA835,
    0x02B5,
    0xAD75,
    0x52AA,
    0xFAAA,
    0x57EA,
    0xFFEA,
    0x52BF, // index 12 -- engine glow
    0xFABF,
    0x57FF,
    0xFFFF,
};

void sprite_mode5_init(void);
void sprite_mode5_set_position(int16_t x, int16_t y);
void sprite_mode5_set_frame(uint8_t frame_index);
void sprite_mode5_update_engine(bool moving_down);

#endif // SPRITE_MODE5_H
```

Here are the key functions to look at:

```c
sprite_mode5_set_frame(uint8_t frame_index)
```
This function updates the current frame of the sprite by changing the XRAM address that the VGA system uses to fetch the sprite data.  This allows us to create animations by simply updating the frame index in XRAM.

```c
sprite_mode5_update_engine(bool moving_down)
```
This function creates a simple animation effect for the player's engine by changing the color of a specific palette entry over time.  When the player is moving down, we reset the animation and set the engine color to black.  When the player is not moving down, we cycle through a set of colors to create a glowing effect for the engine.  This is done by updating the palette entry in XRAM that the sprite uses for the engine glow.  This allows us to create a dynamic visual effect without needing to change the sprite data itself, which can save memory and allow for more complex animations.

Once you have added this code, you should see the player sprite change its frame based on movement and the engine glow animating when the player is moving.  You can customize the animation logic and palette effects to create different visual styles for your game.


## Adding Bullets 

We can add a new asset for our projectile sprite data, and then set up a new sprite configuration in XRAM for the projectiles.  We can then create a pool of projectile sprites that we can activate and deactivate as needed to create bullets that the player can shoot.  This is a common technique in game development called object pooling, which allows us to reuse a fixed number of sprite instances for our bullets without needing to constantly create and destroy sprites, which can be expensive in terms of performance.

```cmake
rp6502_asset(RPStarHopper XRAM(XRAM_PROJECTILE_DATA) images/Projectiles_4bpp.bin)
```

Here is the layout for the projectile sprite data and configuration in XRAM.  In ```src/xram.h```, the projectiles get a config array (one `mode5_sprite_t` per projectile), a palette, and their frames, each next to its kind:

```c
typedef MODE5_IMAGE(4, 8) sprite_8x8_t;

#define PROJECTILE_FRAME_SIZE sizeof(sprite_8x8_t)

typedef struct
{
    /* ... */
    mode2_config_t tile_hud_config;
    mode5_sprite_t projectile_config[MAX_PROJECTILES];

    /* ... */
    uint16_t tile_hud_palette[1 << 4];
    uint16_t projectile_palette[1 << 4];

    /* ... */
    tile_8x8_t starfield_tiles_data[STARFIELD_TILE_COUNT];
    sprite_8x8_t projectile_data[PROJECTILE_FRAME_COUNT];
} xram_layout_t;

#define XRAM_PROJECTILE_CONFIG offsetof(xram_layout_t, projectile_config)
#define XRAM_PROJECTILE_PALETTE offsetof(xram_layout_t, projectile_palette)
#define XRAM_PROJECTILE_DATA offsetof(xram_layout_t, projectile_data)
```

and the sizes in ```constants.h```:

```c
#define PROJECTILE_SPRITE_SIZE_PX   8                 // Projectile sprite is 8x8 pixels
#define PROJECTILE_FRAME_COUNT  13                  // 13 frames for projectile/pickups/asteroids/explosions
#define MAX_PROJECTILES         40                  // Max number of projectiles on screen at once
#define MAX_PLAYER_PROJECTILES  8                   // Slots 0..(MAX_PLAYER_PROJECTILES-1) are reserved for the player
```

`projectile_config` is 40 sprite configs, 320 bytes, and `projectile_data` is 13 frames of 32 bytes, 416 bytes.

Projectile frame usage:
- `0`: player projectile
- `1`: enemy projectile
- `2`: energy pickup
- `3`: speed pickup
- `4`: power pickup
- `5,6,7`: asteroid animation sequence
- `10,11,12`: explosion animation sequence

Between-wave asteroid events:
- On non-final subwave transitions for waves `7..10`, `1..3` asteroids are spawned.
- Asteroids descend vertically at `1 px/frame` and animate through frames `5,6,7`.
- Asteroid collision uses a centered `6x6` hitbox inside the `8x8` sprite.
- When destroyed by a player shot, asteroids yield a pickup from a fixed repeating sequence:
    - Sequence: **P → E → (none) → S → E → E → P → E → (none) → E**, then repeats
    - The sequence persists across all levels and phases within a run

Pickup behavior and effects:
- Pickup sprites zig-zag horizontally while descending at `0.25 px/frame`.
- Pickup collection uses a centered `6x6` hitbox inside the `8x8` sprite.
- Energy pickup: restores `8` HP (capped by `PLAYER_MAX_HEALTH`).
- Speed pickup: increases the player's unlocked speed cap by `+1`, restores current speed to that cap, and is capped at `PLAYER_SPEED_MAX` (`10`, or `2.5 px/frame`). LT can reduce current speed temporarily; RT can only restore up to the unlocked cap.
- Power pickup: increases fire rate by `+1` unit (implemented as reducing shot cooldown by 1 frame), capped by `PLAYER_FIRE_RATE_MIN`.

Here is the code to initialize the projectile sprites in ```sprite_mode5.c```.  Note that we changed the options in the xreg_vga_mode5 call to `MODE5_4BPP | MODE5_8X8`, 8x8 sprites with a 4-bit color index, which is appropriate for our projectile sprites.  We also set up a pool of projectile sprites in XRAM, initializing their positions off-screen and pointing them to the correct sprite data and palette.  This allows us to activate and deactivate these projectile sprites as needed during gameplay to create shooting mechanics.
```c
void sprite_mode5_init_projectiles(void) {
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {

        unsigned ptr = XRAM_PROJECTILE_CONFIG + (i * sizeof(mode5_sprite_t));

        xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, -32); // Start off-screen
        xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, xram_sprite_ptr, XRAM_PROJECTILE_DATA);
        xram0_struct_set(ptr, mode5_sprite_t, palette_ptr, XRAM_PROJECTILE_PALETTE);
    }

    // Mode 5 args: OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode5(MODE5_4BPP | MODE5_8X8, XRAM_PROJECTILE_CONFIG, MAX_PROJECTILES, 0, HUD_TOP_PX, 0) < 0) {
        puts("xreg_vga_mode5 failed");
        return;
    }

    RIA.addr0 = XRAM_PROJECTILE_PALETTE;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = projectiles_palette[i] & 0xFF;
        RIA.rw0 = projectiles_palette[i] >> 8;
    }

    puts("Mode5 projectile sprites ready");
}
```

With the projectile sprites initialized, we can then create functions to fire projectiles and update their positions on the screen.  We will maintain an array of projectile instances in our game logic, which will track whether each projectile is active and its current position.  When the player fires a projectile, we will activate one of the projectile sprites from our pool and set its position to the player's current position.  We will then update the position of each active projectile in our main game loop, moving them upwards on the screen and deactivating them when they go off-screen.  This allows us to create a simple shooting mechanic for our game using the sprite system.

```c
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "projectile.h"
#include "sprite_mode5.h"

typedef struct {
    bool    active;
    int16_t x;
    int16_t y;
} Projectile;

static Projectile projectiles[MAX_PROJECTILES];

void projectile_init(void)
{
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = false;
        projectiles[i].x = -32;
        projectiles[i].y = -32;
    }
    // Hardware slots are already positioned off-screen by sprite_mode5_init_projectiles()
}

void projectile_fire_player(int16_t x, int16_t y)
{
    for (uint8_t i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active = true;
            projectiles[i].x = x;
            projectiles[i].y = y;
            sprite_mode5_set_projectile_position(i, x, y);
            return;
        }
    }
    // All player slots full — no-op
}

void projectile_update(void)
{
    for (uint8_t i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;

        projectiles[i].y -= PROJECTILE_SPEED_PX;

        if (projectiles[i].y < HUD_TOP_PX) {
            // Bullet has left the play area — deactivate
            projectiles[i].active = false;
            sprite_mode5_set_projectile_position(i, -32, -32);
        } else {
            sprite_mode5_set_projectile_position(i, projectiles[i].x, projectiles[i].y);
        }
    }
}
```

Our main.c file only needs a few updates: add `#include "projectile.h"`, call ```projectile_init()``` in ```init_graphics```, and add ```projectile_update()``` to the main loop.  With this code in place, you should now be able to fire projectiles from the player's position and see them move upwards on the screen until they go off-screen and are deactivated.  You can customize the projectile behavior, speed, and appearance by modifying the projectile data and update logic as needed for your game.

```c
// Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        // 3. UPDATE
        music_update();
        tile_mode2_update_scroll();
        player_controller_update();
        projectile_update();
    }
```

## Gameplay Loop

Right now our screen is very busy.  We have our title-card, music, flying starfield background, and a player sprite that we can move around.  This is great for testing our systems, but it's not really a game yet.  We need to add some structure to our game by implementing a game loop with different states for the title screen, gameplay, and game over screen.  This will allow us to create a more complete game experience with a clear flow from start to finish.  We can define an enum for our game states and then use a switch statement in our main loop to handle the logic for each state.  This will allow us to show the title screen when the game starts, transition to the gameplay state when the player presses a button, and then show a game over screen when the player loses.  This structure will make it easier to manage the different parts of our game and create a more polished experience for the player.

The example below shows how we can implement a simple game loop with a title screen and gameplay state.  We check for input to transition from the title screen to the gameplay state, and we update our music, tilemaps, player, and projectiles only when we are in the appropriate game state.  This allows us to create a more structured game experience with different states for different parts of the game.  We can change music tracks, update the title screen palette, and control the player and projectiles based on the current game state.
```c
// Main loop
    while (true) {
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. INPUT
        handle_input();

        {
            game_transition_t transition = game_state_handle_start_button(
                is_action_pressed(0, ACTION_BTN_START)
            );

            if (transition == GAME_TRANSITION_START_GAME) {
                tile_mode2_start_gameplay_transition();
                music_set_track("ROM:Level_01.vgm");
            }
        }

        // 3. UPDATE
        music_update();
        if (game_state_get() == GAME_STATE_TITLE) {
            tile_mode2_update_title_palette();
        }
        if (game_state_get() != GAME_STATE_PAUSED) {
            tile_mode2_update_scroll();
        }
        if (game_state_get() == GAME_STATE_PLAYING) {
            player_controller_update();
            projectile_update();
        }
    }
  ```


![Leaving Warp to start the game](Screenshots/Screenshot_003.png)

## Enemies and Collision Detection

Now we can add enemy sprites and basic combat.

The implementation has four parts:
1. Reserve memory and assets for enemy frames
2. Initialize an enemy sprite pool in Mode 5
3. Spawn/update enemies in waves
4. Detect bullet hits and despawn both objects

### 1. Enemy Data Layout and Asset

First, add the enemy sprite sheet to CMake:

```cmake
rp6502_asset(RPStarHopper XRAM(XRAM_ENEMY_DATA) images/Enemies_4bpp.bin)
```

Then add the enemies to the layout in ```src/xram.h```, the same way as the projectiles: a config array, a palette, and the frames.  With the enemies (and the sound effects, `sfx_data`, which this tutorial doesn't cover) in place, this is the complete layout from ```src/xram.h```, after the datasheet blocks:

```c
/* Star Hopper's XRAM. Every asset and config is 4-bit color, */
/* so the images are 16-color and each palette has 1 << 4 entries. */

typedef MODE5_IMAGE(4, 16) sprite_16x16_t;
typedef MODE5_IMAGE(4, 8) sprite_8x8_t;
typedef MODE2_TILE(4, 8) tile_8x8_t;

#define PLAYER_FRAME_SIZE sizeof(sprite_16x16_t)
#define PROJECTILE_FRAME_SIZE sizeof(sprite_8x8_t)
#define ENEMY_FRAME_SIZE sizeof(sprite_16x16_t)

typedef struct
{
    /* First, so the OPL2 registers start on a page boundary. */
    opl_t opl;

    mode5_sprite_t player_config;
    mode2_config_t tile_bg_config;
    mode2_config_t tile_fg_config;
    mode2_config_t tile_hud_config;
    mode5_sprite_t projectile_config[MAX_PROJECTILES];
    mode5_sprite_t enemy_config[MAX_ENEMIES];

    uint16_t player_palette[1 << 4];
    uint16_t tile_bg_palette[1 << 4];
    uint16_t tile_fg_palette[1 << 4];
    uint16_t tile_hud_palette[1 << 4];
    uint16_t projectile_palette[1 << 4];
    uint16_t enemy_palette[1 << 4];

    keyboard_t keyboard;
    gamepad_t gamepad;

    /* Loaded from the ROM by CMakeLists.txt. */
    sprite_16x16_t player_data[PLAYER_FRAME_COUNT];
    uint8_t starfield_bg_data[STARFIELD_BG_HEIGHT][STARFIELD_BG_WIDTH];
    uint8_t starfield_fg_data[STARFIELD_FG_HEIGHT][STARFIELD_FG_WIDTH];
    uint8_t starfield_hud_data[STARFIELD_HUD_HEIGHT][STARFIELD_HUD_WIDTH];
    tile_8x8_t starfield_tiles_data[STARFIELD_TILE_COUNT];
    sprite_8x8_t projectile_data[PROJECTILE_FRAME_COUNT];
    sprite_16x16_t enemy_data[ENEMY_FRAME_COUNT];

    /* Last, so regenerating the SFX never moves anything else. */
    uint8_t sfx_data[SFX_DATA_SIZE];
} xram_layout_t;

#define XRAM_OPL offsetof(xram_layout_t, opl)
_Static_assert((XRAM_OPL & 0xFF) == 0, "The OPL2 registers must start on a page boundary.");

#define XRAM_PLAYER_CONFIG offsetof(xram_layout_t, player_config)
#define XRAM_TILE_BG_CONFIG offsetof(xram_layout_t, tile_bg_config)
#define XRAM_TILE_FG_CONFIG offsetof(xram_layout_t, tile_fg_config)
#define XRAM_TILE_HUD_CONFIG offsetof(xram_layout_t, tile_hud_config)
#define XRAM_PROJECTILE_CONFIG offsetof(xram_layout_t, projectile_config)
#define XRAM_ENEMY_CONFIG offsetof(xram_layout_t, enemy_config)

#define XRAM_PLAYER_PALETTE offsetof(xram_layout_t, player_palette)
#define XRAM_TILE_BG_PALETTE offsetof(xram_layout_t, tile_bg_palette)
#define XRAM_TILE_FG_PALETTE offsetof(xram_layout_t, tile_fg_palette)
#define XRAM_TILE_HUD_PALETTE offsetof(xram_layout_t, tile_hud_palette)
#define XRAM_PROJECTILE_PALETTE offsetof(xram_layout_t, projectile_palette)
#define XRAM_ENEMY_PALETTE offsetof(xram_layout_t, enemy_palette)

#define XRAM_KEYBOARD offsetof(xram_layout_t, keyboard)
#define XRAM_GAMEPAD offsetof(xram_layout_t, gamepad)

#define XRAM_PLAYER_DATA offsetof(xram_layout_t, player_data)
#define XRAM_STARFIELD_BG_DATA offsetof(xram_layout_t, starfield_bg_data)
#define XRAM_STARFIELD_FG_DATA offsetof(xram_layout_t, starfield_fg_data)
#define XRAM_STARFIELD_HUD_DATA offsetof(xram_layout_t, starfield_hud_data)
#define XRAM_STARFIELD_TILES_DATA offsetof(xram_layout_t, starfield_tiles_data)
#define XRAM_PROJECTILE_DATA offsetof(xram_layout_t, projectile_data)
#define XRAM_ENEMY_DATA offsetof(xram_layout_t, enemy_data)
#define XRAM_SFX_DATA offsetof(xram_layout_t, sfx_data)
```

All of it comes to 41260 bytes, just over 40 KB of the 64 KB of XRAM.  `SFX_DATA_SIZE` comes from `src/sfx_layout.h`, which `tools/generate_sfx.py` writes and ```constants.h``` includes.  The enemy sizes go in ```constants.h```:

```c
#define ENEMY_SPRITE_SIZE_PX   16
#define ENEMY_FRAME_COUNT      176                  // enemies, GAME OVER letters and bosses
#define ENEMY_TYPE_COUNT       7
#define MAX_ENEMIES            32
```

`enemy_data` is 22528 bytes because each 16x16 frame at 4bpp is 128 bytes and the sprite sheet holds `ENEMY_FRAME_COUNT` (176) frames.

Enemy frame layout (per type):
- Base frames for types `0..6`: `0, 6, 12, 18, 24, 30, 36`
- Active animation for each type: base `+0, +1, +2`
- Destruction animation for each type: base `+3, +4, +5`

Game-over letter frames:
- `42..49` spell `GAME OVER`

### 2. Enemy Sprite Pool (Mode 5)

In sprite_mode5.c, enemy sprites are initialized after projectile sprites:

```c
void sprite_mode5_init_enemies(void) {
    for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
        unsigned ptr = XRAM_ENEMY_CONFIG + ((unsigned)i * sizeof(mode5_sprite_t));
        xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, -32);
        xram0_struct_set(ptr, mode5_sprite_t, xram_sprite_ptr, XRAM_ENEMY_DATA);
        xram0_struct_set(ptr, mode5_sprite_t, palette_ptr, XRAM_ENEMY_PALETTE);
    }

    if (xreg_vga_mode5(MODE5_4BPP | MODE5_16X16, XRAM_ENEMY_CONFIG, MAX_ENEMIES, 1, HUD_TOP_PX, 0) < 0) {
        puts("xreg_vga_mode5 failed");
        return;
    }
}
```

Important details:
- Plane `0` is used for enemies (with tiles and player in higher layers)
- `BEGIN=HUD_TOP_PX` (scanline 24) keeps them out of the HUD scanlines
- All enemy sprites start off-screen at `(-32, -32)`

To move and retarget enemy type frames:

```c
void sprite_mode5_set_enemy(uint8_t slot, int16_t x, int16_t y, uint8_t type)
{
    unsigned ptr = XRAM_ENEMY_CONFIG + ((unsigned)slot * sizeof(mode5_sprite_t));
    xram0_struct_set(ptr, mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(ptr, mode5_sprite_t, y_pos_px, y);
    xram0_struct_set(ptr, mode5_sprite_t, xram_sprite_ptr,
        (XRAM_ENEMY_DATA + ((unsigned)type * ENEMY_FRAME_SIZE)));
}
```

### 3. Enemy Waves and Zig-Zag Path

The enemy module keeps a fixed pool and a small wave state machine.

Wave states:
- `WAVE_STATE_DELAY`
- `WAVE_STATE_SPAWNING`
- `WAVE_STATE_CLEARING`

Enemies spawn one-by-one in a wave, follow a shared zig-zag path, and are disabled once they leave the bottom of the screen.

Wave spawning uses the full 32-enemy sprite pool rather than reusing only the first few hardware slots. When the next wave starts, new enemies are placed into the first free pool slots, so surviving enemies from previous waves remain active on screen.

Spawn starts off-screen so enemies enter cleanly (sprite coordinates are top-left):

```c
enemies[slot].y = -ENEMY_SPRITE_SIZE_PX;
```

Then each frame:

```c
enemies[i].y += ENEMY_SPEED_Y;
enemies[i].x = enemy_path_x_for_y(enemies[i].y);
```

Primary wave order uses a mirrored 13-step sequence:

```c
0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0
```

The next wave begins when either:
- all enemies spawned by the current wave are disabled, or
- `WAVE_CLEAR_TIMEOUT_FRAMES` expires after the last enemy in that wave is spawned.

Surviving enemies from older waves are not despawned when this happens; they continue to move and shoot while the new wave spawns into other free pool slots.

When a wave advances, the primary type is chosen from that mirrored sequence rather than simple modulo indexing:

```c
wave_primary_type = enemy_get_primary_type_for_subwave(current_subwave);
```

### 4. Bullet vs Enemy Collision

Collision is implemented as axis-aligned bounding box (AABB) overlap between an enemy rectangle and active player bullet rectangles.

When a bullet hit is confirmed, enemies now enter a dedicated dying state instead of being removed instantly:
- Dying enemies play the 3-frame destruction sequence (`+3,+4,+5`) at a cadence similar to player destruction.
- While dying, enemies do not move, do not fire, and do not collide with player bullets or the player.
- After the final death frame, the enemy is removed from the screen and deactivated.

In projectile.c:

```c
bool projectile_hit_test_enemy(int16_t x, int16_t y, int16_t width, int16_t height)
{
    // scans active player bullets, returns true on first overlap
    // hit bullet is consumed and moved off-screen
}
```

In enemy_update(), after movement:

```c
if (enemies[i].y >= HUD_TOP_PX &&
    projectile_hit_test_enemy(enemies[i].x, enemies[i].y,
                              ENEMY_SPRITE_SIZE_PX, ENEMY_SPRITE_SIZE_PX)) {
    enemies[i].active = false;
    sprite_mode5_set_enemy(i, -32, -32, enemies[i].type);
    continue;
}
```

This gives immediate hit feedback with visible destruction animation and clean deactivation.

### 5. Main Loop Integration

Initialization:

```c
sprite_mode5_init_projectiles();
sprite_mode5_init_enemies();
projectile_init();
enemy_init();
```

Per-frame update while playing:

```c
if (game_state_get() == GAME_STATE_PLAYING) {
    player_controller_update();
    projectile_update();
    enemy_update();
}
```

With this in place, the game now has enemy waves, multi-type spawning, and working player-bullet collision.

![Enemies](Screenshots/Screenshot_004.png)

### 6. Score System

Next, we add score rendering and point awards for enemy hits.

Score display location:
- HUD tile positions `(17,1)` through `(22,1)`
- 6 digits, initialized as `000000`
- Tile indices `19..28` are digits `0..9`
- Combo multiplier indicator uses the bottom-left HUD tiles `(0,28)` and `(1,28)` as `<digit>x`.
- Tile index `225` is used for the `x` symbol.
- While paused, `PAUSED` is rendered near HUD center using explicit tiles `242, 227, 247, 245, 231, 230`.
- HUD text messages force HUD palette index `2` to `0x57FF` (yellow) while drawing so title rainbow cycling does not tint gameplay text.

Gameplay transition messages:
- At level start (during fast->slow scroll transition), HUD shows `LEVEL XX`.
- `LEVEL XX` is removed once enemies start spawning.
- When a level completes, HUD shows `LEVEL COMPLETE` during autopilot movement to bonus position.
- After autopilot reaches the bonus position, there is a `1` second hold before bonus tally starts.

In `tile_mode2.c` we expose:

```c
void tile_mode2_set_score(uint32_t score)
void tile_mode2_set_multiplier(uint8_t multiplier)
```

This function clamps to `999999` and writes six tiles into the HUD tilemap (`XRAM_STARFIELD_HUD_DATA`), mapping each decimal digit to tile index `19 + digit`.

In `score.c` we keep a running score and update HUD digits whenever score changes:

```c
void score_init(void);
void score_add_enemy_kill(uint8_t enemy_type);
void score_reset_multiplier(void);
```

Kill combo multiplier rules are:
- Start at `1x`.
- Each enemy kill awards `base_points * current_multiplier`.
- Multiplier increases by `+1` after every `2` kills without being hit, up to `5x`.
- Any player damage resets multiplier to `1x`.

Point rules are:
- enemy type 0 = 10 points
- enemy type 1 = 15 points
- enemy type 2 = 20 points
- ...
- capped at 40 points for higher types

The cap keeps values aligned with your requested top score per kill and still supports future type expansion.

Collision integration happens in `enemy_update()` right where bullet hit is confirmed:

```c
score_add_enemy_kill(enemies[i].type);
```

Main initialization now includes:

```c
score_init();
```

so score is immediately drawn as `000000` before gameplay starts.

### 7. Pattern-Driven Enemy Waves and Enemy Fire

Once the basic enemy wave system was working, the next step was to give each enemy index its own movement and attack behavior.

The main architectural change was to refactor `enemy.c` away from a single shared movement rule and into a pattern-driven wave controller.  We still keep the same outer wave flow:

```c
WAVE_STATE_DELAY -> WAVE_STATE_SPAWNING -> WAVE_STATE_CLEARING
```

and we still advance through enemy indices sequentially:

```c
wave_type = (uint8_t)((wave_type + 1) % ENEMY_TYPE_COUNT);
```

However, each spawned enemy now carries its own behavior state:
- phase
- timers
- fire cadence
- target position
- waypoint index
- spiral step
- per-pattern motion parameters

This lets us reuse one enemy pool while still giving each wave a distinct identity.

#### Shared Projectile Pool

Rather than creating a second projectile system for enemy bullets, we expanded the existing projectile pool.

Slot usage is now:
- player bullets: slots `0..7`
- enemy bullets: slots `8..31`

In `projectile.h` this is represented with:

```c
#define FIRST_ENEMY_PROJECTILE_SLOT MAX_PLAYER_PROJECTILES
```

The projectile structure was expanded so bullets can belong to either side and can travel in arbitrary directions:

```c
typedef enum {
    PROJECTILE_OWNER_NONE,
    PROJECTILE_OWNER_PLAYER,
    PROJECTILE_OWNER_ENEMY,
} projectile_owner_t;
```

Each projectile now tracks:
- owner
- Q8 position
- Q8 velocity
- frame index

This is what allows enemy bullets to travel downward, diagonally, or in spiral and barrage patterns.

#### Player Position Accessors

Several enemy patterns need to aim at the player.  To support this cleanly, `player_controller.c` now exposes:

```c
void player_controller_get_position(int16_t *x, int16_t *y);
void player_controller_get_center_position(int16_t *x, int16_t *y);
```

Enemy code uses the center position whenever it needs to aim a dive or a bullet toward the player.

#### Enemy Pattern Summary

Enemy index `0`
- Spawns from a shared quasi-random point at the top of the screen
- Follows a zig-zag path
- Dives straight down once vertically aligned with the player
- Fires only occasionally with medium-speed bullets

Enemy index `1`
- Rises from the bottom of the screen
- Each enemy has its own horizontal offset
- Stops around mid-screen, rapid-fires, then exits upward

Enemy index `2`
- Enters from the top-left or top-right
- Follows a corner path around the screen
- Fires slow bullets at a regular cadence

Enemy index `3`
- Moves to a semi-random stop point
- Holds position and emits bullets in a spiral pattern

Enemy index `4`
- Descends slowly from the top
- After a delay, dives toward the player’s current position
- Does not fire bullets

Enemy index `5`
- Enters from the side and forms a space-invaders-style line
- Sweeps left and right while stepping downward
- Fires medium-speed bullets toward the player

Enemy index `6`
- Moves toward the player
- When close enough, despawns in a radial barrage of bullets

#### Pattern Helpers

To keep the code manageable, `enemy.c` now uses a set of shared helpers:
- deterministic pseudo-random number generation for quasi-random spawn and target points
- `enemy_compute_aim_velocity()` for aimed motion and aimed bullets
- `enemy_try_move_towards()` for waypoint and stop-point movement
- `enemy_fire_aimed()` and `enemy_fire_directional()` for attack logic
- `enemy_fire_barrage()` for radial burst attacks

This makes it possible to build new behaviors from the same small set of movement and fire primitives.

#### Current Scope

At this stage, enemy bullets are fully implemented as visuals and movement patterns, but player damage is intentionally not implemented yet.

That means:
- enemies can shoot
- enemy bullets move correctly
- enemy waves have different attack patterns
- player bullets can still destroy enemies
- enemy bullets do not yet reduce player health or trigger game over

That keeps the architecture clean while the attack patterns are being designed and tuned.

![GamePlay](Screenshots/Screenshot_005.png)

### 8. Player Collisions, Health, and Game Over

The current build now includes full player damage handling and a game-over flow.

#### Collision Sources

Player damage now comes from both:
- Enemy bullets (projectile slots reserved for enemies)
- Enemy sprite contact

Collision checks are AABB and use early-outs to keep frame cost bounded even when many sprites are active.

#### Health Model

Player health starts at `48` and is rendered on HUD row 2.

Health bar mapping:
- Tile range: `(17,2)` through `(22,2)`
- 6 tiles total
- 8 HP per tile
- Tile index `39` is full and `47` is empty

The game exposes tuning constants in `constants.h`:
- `PLAYER_MAX_HEALTH`
- `PLAYER_BULLET_DAMAGE`
- `PLAYER_CONTACT_DAMAGE`
- `PLAYER_HIT_COOLDOWN_FRAMES` (currently `108` frames)
- `PLAYER_HIT_FLASH_FRAMES`

#### Health FX

HUD palette index `10` is used for health feedback:
- Normal: default HUD palette color
- Damage flash: white blink for a short duration after hit
- Low health: red override once health drops under threshold

Player sprite feedback:
- Most ship pixels use palette index `15`
- On damage, index `15` blinks to the color from index `12` (red accent), then returns
- Damage flash lasts longer and matches the temporary post-hit invulnerability window

#### Player Destruction Sequence

When health reaches zero, the player sprite runs a destruction sequence using frames:
- `3`
- `4`
- `5`

Movement and firing are disabled while destruction is active.

The game now waits for this destruction sequence to finish before entering `GAME OVER` state.

Audio timing during destruction/game-over:
- Current gameplay music stops immediately when the player is destroyed
- Player explosion plays first
- `music/Gameover.vgm` starts when `GAME OVER` sprites begin their fly-in sequence
- After `GAME OVER` sprites fully assemble, there is a 2-second hold before fast title-style scroll transition starts

Player collision box tuning:
- Collision checks now use a centered `14x14` hitbox inside the `16x16` sprite (`(PLAYER_SPRITE_SIZE_PX - PLAYER_HITBOX_SIZE) / 2` offset)
- Post-hit invulnerability window is `108` frames (twice the previous `54`-frame value)

Title screen player effect:
- The player sprite engine glow palette animation now runs on the title screen as an ambient effect

### 9. Level System and Between-Level Bonus

The game now runs in levels. Each level is made of 7 subwaves (enemy types `0..6`).

At the end of the 7th subwave:
1. We enter a level bonus/intermission state.
2. Scroll transitions to fast warp style (without restoring HUD from ROM).
3. Music switches to `music/Bonus.vgm`.
4. Bonus tally is rendered.
5. Press and release START to begin the next level.

#### Level Compositions

Level 1:
- `0x5`, `1x5`, `2x5`, `3x5`, `4x5`, `5x5`, `6x5`

Level 2:
- `0x5+1x1`, `1x5+2x1`, `2x5+3x1`, `3x5+4x1`, `4x5+5x1`, `5x5+6x1`, `6x5+4x1`

Level 3:
- `0x5+1x3`, `1x5+2x3`, `2x5+3x3`, `3x5+4x3`, `4x5+5x3`, `5x5+6x2+5x1`, `6x5+4x3`

Level 4:
- `0x5+1x5`, `1x5+2x5`, `2x5+3x5`, `3x5+4x5`, `4x5+5x5`, `5x5+6x5`, `6x5+4x5`

Level 5+:
- Uses fixed scripted mixed tables (deterministic), up to 15 enemies in a subwave.

#### Bonus Tally

Bonus tally uses total kills by enemy type across the full previous level.

Display format is per row:
- enemy icon (sprite)
- 2-digit kill count
- tile `225` for `x`
- points-per-kill (scaled)
- tile `226` for `=`
- row subtotal

Score scaling by level:
- Level 1: base values (`10, 15, 20, ...`)
- Level 2: base x2 (`20, 30, 40, ...`)
- Level 3: base x3 (`30, 45, 60, ...`)
- Level 4+: base x(level)

Scoring note:
- Enemy index `6` is worth `100` points per kill before level multiplier scaling in bonus tally.

Health restoration:
- 1 HP restored per enemy kill from the previous level (clamped to max).

Bonus completion prompt:
- After bonus tally and health refill complete, HUD shows `PRESS START` near the bottom.

#### Music Flow

Gameplay tracks by level:
- Level 1: `music/Level_01.vgm`
- Level 2: `music/Level_02.vgm`
- Level 3: `music/Level_03.vgm`
- Level 4: `music/Level_04.vgm`
- Level 5: `music/Level_05.vgm`
- Level 6: `music/Level_06.vgm`
- Level 7+: `music/Level_07.vgm` (also the fallback for every level past 7 -- there's no higher-level track, so the last one just keeps playing)
- Boss battles (any level): `music/Boss.vgm`, overriding whichever level track was playing

Intermission track:
- Between levels: `music/Bonus.vgm`

#### Game Over State

A dedicated game-over state is now part of the state machine.

On game-over entry:
1. Enemy/projectile gameplay interactions are halted
2. Music switches to `music/Gameover.vgm`
3. Background scroll transitions back toward title-style fast scrolling
4. HUD tilemap is restored from ROM using:
    - `open("ROM:StarFields_HUD_map.bin", O_RDONLY)`

Game-over visuals:
- Enemy frames `42..49` are reused to spell `GAME OVER`
- Letters fly in from different off-screen origins and converge more slowly
- Final word placement is shifted downward to avoid overlapping `PRESS START`
- Only a brief delay is applied before the letter fly-in begins

Foreground tile behavior on game-over transition:
- Warp/foreground tiles are restored when transitioning back to fast title-style scrolling

Exit rules from game-over:
- Start press + release, or
- 60-second timeout

Both paths return to title and reset player, enemies, projectiles, score, HUD health, and title music.

#### Enemy Tuning Parameters

The quickest way to tune enemy behavior is in `src/enemy.c` and `src/enemy.h`.

Global wave pacing (`src/enemy.h`):
- `ENEMY_WAVE_SIZE`: number of enemies spawned per wave pattern.
- `ENEMY_SPAWN_DELAY_FRAMES`: delay before the first wave starts.
- `ENEMY_INTER_SPAWN_FRAMES`: spacing between enemy spawns inside one wave.
- `ENEMY_INTER_WAVE_FRAMES`: pause between completed waves.

Wave overlap pacing (`src/enemy.c`):
- `WAVE_CLEAR_TIMEOUT_FRAMES`: maximum wait after the last enemy of a wave is spawned before forcing the next wave to start. Current value is `240` frames (4 seconds at 60 FPS).
- Next-wave trigger condition: start the next wave when either all enemies from the current wave are disabled, or the timeout expires.

Shared movement/bullet speed (`src/enemy.c`):
- `ENEMY_SLOW_SPEED_Q8`, `ENEMY_MEDIUM_SPEED_Q8`, `ENEMY_FAST_SPEED_Q8`, `ENEMY_DIVE_SPEED_Q8`
- `BULLET_SLOW_SPEED_Q8`, `BULLET_MEDIUM_SPEED_Q8`, `BULLET_FAST_SPEED_Q8`

Shared predictive aiming (`src/enemy.c`):
- `AIM_LEAD_MIN_FRAMES` / `AIM_LEAD_MAX_FRAMES`: clamp range for dynamic lead time.
- `AIM_MAX_LEAD_PER_AXIS`: clamps per-frame tracked player velocity to avoid extreme lead jumps.
- `enemy_update_player_tracking()`: updates player velocity estimate once per frame.
- `enemy_get_aim_lead_frames()`: computes dynamic lead time using bullet speed and distance.
- `enemy_get_predicted_player_center()`: returns the clamped projected target used by aimed fire helpers.

Projectile pool sizing (`src/constants.h`):
- `MAX_PROJECTILES`: total shared projectile sprites (player + enemy bullets).
- `MAX_PLAYER_PROJECTILES`: reserved player slots; remaining slots are enemy bullets.

Index 0 (zig then dive):
- `ENEMY_ZIG_SPEED` (`src/enemy.h`): horizontal oscillation intensity.
- `TYPE0_INTER_SPAWN_FRAMES` (`src/enemy.c`): spacing between index-0 enemies as they follow each other in the same path.
- `wave_type0_shots_remaining` setup in `enemy_prepare_wave()`: number of aimed shots allowed during the zig phase.
- Dive trigger alignment threshold in `update_pattern0()` (`<= 8` pixels).

Index 1 (rise, hold, tight spread fire, exit):
- Spawn X and hold Y positions in `spawn_enemy()` case `1` define the X formation.
- `TYPE1_SHOTS_PER_ENEMY`: total bullets each enemy fires during attack.
- `timer` in `update_pattern1()`: how long each ship holds in attack phase.
- `aim_pattern_y_offsets[]` in `update_pattern1()`: sequence of vertical aim offsets (for patterns like at, above, at, below, at).
- `TYPE1_FIRE_INTERVAL_FRAMES`: cadence between each shot in the quick burst.
- Shot speed in `update_pattern1()` (`BULLET_MEDIUM_SPEED_Q8`) controls dodge difficulty.

Index 2 (edge path):
- Corner waypoints in `enemy_type2_waypoint()`.
- `safe_left` and `safe_right` in `enemy_type2_waypoint()`: left/right screen safety margins.
- Fire cadence (`fire_timer = 42`) in `update_pattern2()`.

Index 3 (move to attack points, wide player-aimed fan):
- Predetermined target arrays (`target_xs`, `target_ys`) in `spawn_enemy()` case `3` control where ships park.
- When multiple index-3 waves overlap, newer groups are offset so parked ships do not stack directly on top of older groups.
- `TYPE3_ATTACK_DURATION_FRAMES`: how long ships stay at attack points.
- `TYPE3_WAVE_FIRE_INTERVAL`: frames between shots in the ongoing fan-wave cadence.
- `enemy_fire_player_fan_step()`: emits one shot per cadence tick and sweeps through a large-angle offset list toward the player.

Index 4 (descend then aimed dive):
- Pre-dive countdown (`timer = 36 + slot*12`) in `spawn_enemy()` case `4` controls progression spacing.
- Dive speed `ENEMY_DIVE_SPEED_Q8` and aim call in `update_pattern4()`.
- Dive target currently uses `player_controller_get_position()` (player top-left) to match sprite corner coordinates.

Index 5 (formation sweep + step-down):
- `TYPE5_FORMATION_SPACING_X`: horizontal spacing between ships.
- `TYPE5_FORMATION_Y`: starting Y of the formation.
- Each overlapping index-5 wave has its own independent formation anchor/state, so multiple formations can coexist on screen without reusing the same movement controller.
- A type-5 wave group remains active until all of its members have spawned and all of those members are gone, which prevents newly prepared formations from becoming inert before their ships finish entering the screen.
- `TYPE5_STEP_DOWN_Q8`: total Y increment applied per wall bounce.
- `TYPE5_STEP_DOWN_SPEED_Q8`: smoothness/speed of the downward transition.
- Side bounds (`8` and `SCREEN_WIDTH - 8`) in `update_pattern5_anchor()`.

Index 6 (chase then detonate barrage):
- Chase speed (`ENEMY_MEDIUM_SPEED_Q8`) in `update_pattern6()`.
- `TYPE6_DETONATE_DIST_X` / `TYPE6_DETONATE_DIST_Y`: proximity needed to trigger radial burst.
- Vertical fail-safe in `update_pattern6()`: if enemy top goes below player top (`enemy_y >= player_top_y`), it detonates immediately.
- Explosion size is controlled by `enemy_fire_big_barrage()` (16-way medium ring + 8-way fast ring).

If you want balancing that feels predictable, tune in this order:
1. `ENEMY_INTER_SPAWN_FRAMES` and `ENEMY_INTER_WAVE_FRAMES`.
2. Per-pattern attack hold timers (`timer`, `TYPE3_ATTACK_DURATION_FRAMES`).
3. Bullet cadence values (`fire_timer` resets, `TYPE3_SPIRAL_FIRE_INTERVAL`).
4. Bullet and movement speeds.

## Gameplay Flow and Level Transitions

### Player Autopilot System

Smooth transitions between gameplay states are implemented using a player autopilot system that scripted movements.

Autopilot states:
- `PLAYER_SCRIPT_NONE` (0): Normal gameplay, player responds to input.
- `PLAYER_SCRIPT_TO_BONUS` (1): Player moves smoothly from current position to bonus screen waypoint (240, 120).
- `PLAYER_SCRIPT_FROM_BONUS` (2): After bonus completes, player returns to start position and resumes gameplay.

Autopilot parameters:
- Movement speed: `PLAYER_SCRIPT_STEP_PX` (2 px per frame on each axis).
- Bonus screen waypoint: (240, 120) — right of center to keep sprite fully visible.
- Start position: center-bottom of playfield (computed from screen dimensions).
- Interpolation: linear step on both X and Y toward target; triggers next phase on arrival.

**Level-end flow:**
1. Player destroys final enemy in wave → `level_cleared` flag set.
2. Input and enemy/projectile updates skip on next frame.
3. `update_player_script()` moves player toward (240, 120) at 2 px/frame.
4. On arrival, automatically enters bonus phase.
5. Bonus phase plays tally animation, music plays intermission track.
6. On bonus complete: `player_script` set to `PLAYER_SCRIPT_FROM_BONUS`.
7. `update_player_script()` moves player back to start position.
8. On arrival, `player_script = PLAYER_SCRIPT_NONE` and normal gameplay resumes.

Benefit: Eliminates abrupt transitions and gives player visual feedback during downtime.

### Game-Over State and Timing

#### Duration

Game-over screen lasts for `GAME_OVER_TIMEOUT_FRAMES` (6528 frames = 108.8 seconds at 60 FPS).

This duration matches the length of the game-over music track (`music/Gameover.vgm`), ensuring the music completes without the screen cutting off abruptly.

#### Animation Sequencing

On game-over entry:
1. Enemy and projectile gameplay halts.
2. Music switches to game-over track.
3. Background scroll transitions to title-style fast scrolling (within `GAME_OVER_SCROLL_START_DELAY_FRAMES = 120` frames = 2 seconds).
4. Player sprite is hidden after the 2-second hold so the `GAME OVER` letters are the focus.
5. Enemy sprite frames 7–14 are repurposed to spell out `GAME OVER`.
6. Letters fly in from various off-screen positions and converge on screen with slower motion than the background.

#### Exit Rules

- **START pressed and released:** Returns to title cleanly (game state reset before transition).
- **Timeout (108.8s):** Auto-returns to title.

Both paths fully reset player, enemies, projectiles, score, HUD health, and restore title music.

#### Player Autopilot Position Tuning

The bonus screen waypoint is set to X=240 (instead of right edge) to ensure the player sprite stays fully visible during the bonus phase without clipping the right screen edge.
