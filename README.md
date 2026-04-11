# RP6502 Game Demo for LLVM-MOS

Get started by using the vscode-llvm-mos template from https://github.com/picocomputer/vscode-llvm-mos.  Find the "Use this template" button and follow the instructions to create a new repository.  

Once you have your respository set up, we are going to update CMakeLists.txt to build the demo game.  Update the contents of CMakeLists.txt to have the name of the game you want to make.  In this example, we are going to make a game called RPDemo.  The CMakeLists.txt file should look something like this:

```cmake
cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPDemo C CXX ASM)

add_executable(RPDemo)

rp6502_asset(RPDemo help src/help.txt)

rp6502_executable(RPDemo 
    DATA file 
    RESET file
)

target_sources(RPDemo PRIVATE
    src/main.c
)
```

At this point, you should be able to build the project in VScode with the build button and run it on your Picocomputer via F5 or the run button.  You can also run it from the command line with the following command:
```
python3 ./tools/rp6502.py run build/RPDemo.rp6502
```
[Note: If you are on a Mac, you can use rp6502_mac.py found in the tools directory of this repository.]

## Setting up Graphics

The documentation for the Picocomputer is excellent:
https://picocomputer.github.io

For this demo we are going to work with a 320x240 canvas.   Let's start by initializing the graphics system.  We can do this by calling the xreg_vga_canvas function with a parameter of 1.  Add the following to your main.c file, as well as ```#include <stdbool.h>``` at the top of the file:


```c
static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    if (rc < 0) {
        puts("Error: xreg_vga_canvas(1) failed");
        return false;
    }
    return true;
}
```

and then we will call this function from our main, and also set up a vsync loop to keep the program running.  Update your main function to look like this:

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

If you build and run this code, you should see a blank screen on your Picocomputer.  This means that we have successfully initialized the graphics system and are running a main loop that is synced to the vertical refresh of the display.  In the next section, we will start drawing some pixels to the screen!  To exit the program hit "CTRL + ALT + DEL" on the keyboard connected to your Picocomputer.

## Adding a Sprite

We are going to use Mode 5 Sprite system for the demo.  It's very flexible and powerful.  We are going to add a 4-bpp (16-colour) sprite with a custom palette and start to get a feel for the XRAM system.   The images folder contains "Player_4bpp.bin" which is a 16x16 pixel sprite in the 4-bpp format.  We will learn later how to make our own sprites and convert them to the correct format.  

We are going to load this sprite into XRAM and then draw it to the screen.  First, we need to add the sprite as an asset in our CMakeLists.txt file.  Update your CMakeLists.txt to add a new rp6502_asset for the sprite, and make sure to include the correct path to the image file.  Your CMakeLists.txt should now look like this:

```cmake
cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPDemo C CXX ASM)

add_executable(RPDemo)

rp6502_asset(RPDemo 0x10000 images/Player_4bpp.bin)
rp6502_asset(RPDemo help src/help.txt)

rp6502_executable(RPDemo 
    DATA file 
    RESET file
)

target_sources(RPDemo PRIVATE
    src/main.c
)
```

This will place the sprite in XRAM at address 0x10000.  I strongly recommend using a simple spreadsheet to track XRAM memory usage.  You can use my template if you like:
https://docs.google.com/spreadsheets/d/1FLckfGkOWBRM4_hf3JohAPjYF4lZ37s38hei422bUt4/edit?usp=sharing

I personally like to track assets using a ```constants.h``` file.  

```c
#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U            // Starting address in XRAM for sprite data

#define PLAYER_DATA            (SPRITE_DATA_START) // Address for main tile bitmap data
#define PLAYER_DATA_SIZE        0x0080U            // 128 bytes (16x16 at 4bpp)
#define PLAYER_SPRITE_SIZE_PX   16                 // Player sprite is 16x16 pixels

#define SPRITE_DATA_END        (PLAYER_DATA + PLAYER_DATA_SIZE) // End address for sprite data

// Palette configurations
#define PLAYER_PALETTE_ADDR    0xFC00  // 16-color palette (32 bytes, 0xFC00-0xFC1F)
#define PLAYER_PALETTE_SIZE    0x0020

#endif // CONSTANTS_H
```

This file defines some constants for our game, including the screen dimensions and the memory layout for our sprite data.  We have defined a section of XRAM starting at 0x0000 for our sprite data, and we have allocated 128 bytes for our player sprite, which is a 16x16 pixel sprite at 4 bits per pixel (4bpp).  This means that each pixel takes up 4 bits, so we can fit two pixels in one byte.  Therefore, a 16x16 sprite will require 128 bytes of memory (16 * 16 * 4 bits / 8 bits per byte = 128 bytes).  If you use the Spreadsheet all the Hex math is done for you.  

Now let's create ```sprite_mode5.c``` and ````sprite_mode5.h```` files to handle the sprite drawing logic.  In these files, we will write functions to initialize the sprite system, load our sprite data into XRAM, and draw the sprite to the screen.  This will help us keep our main.c file clean and organized.  Here is an example of what the contents of these files might look like:

```c
#include <rp6502.h>
#include <stdio.h>
#include <stdint.h>
#include "constants.h"
#include "sprite_mode5.h"

// Store the player config address for updates
unsigned PLAYER_CONFIG;

void sprite_mode5_init(void) {
    int rc;
    int16_t center_x = (int16_t)((SCREEN_WIDTH - PLAYER_SPRITE_SIZE_PX) / 2);
    int16_t center_y = (int16_t)((SCREEN_HEIGHT - PLAYER_SPRITE_SIZE_PX) * 2 / 3); // Start slightly lower than center for better composition

    PLAYER_CONFIG = SPRITE_DATA_END; // Just after the end of sprite data

    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, center_x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, center_y);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, xram_sprite_ptr, PLAYER_DATA);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, palette_ptr, PLAYER_PALETTE_ADDR);


    // Mode 5 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(5, 0x0A, PLAYER_CONFIG, 1, 1, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }


    RIA.addr0 = PLAYER_PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = player_palette[i] & 0xFF;
        RIA.rw0 = player_palette[i] >> 8;
    }


    puts("Mode5 player sprite ready");
}
```

```c
#ifndef SPRITE_MODE5_H
#define SPRITE_MODE5_H

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
    uint16_t xram_sprite_ptr;
    uint16_t palette_ptr;
} vga_mode5_sprite_t;

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

Update CMakeLists.txt to include the new source file:

```cmake
target_sources(RPDemo PRIVATE
    src/main.c
    src/sprite_mode5.c
)
```

and update main.c to look like this:

```c
#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "sprite_mode5.h"



static bool init_graphics(void)
{
    // 320×240 canvas
    int rc;
    rc = xreg_vga_canvas(1);
    if (rc < 0) {
        puts("Error: xreg_vga_canvas(1) failed");
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

At this point, if you build and run the code, you should see your player sprite displayed in the center of the screen!  Congratulations, you have successfully loaded a sprite into XRAM and drawn it to the screen using Mode 5!  In the next section, we will start adding some interactivity and movement to our sprite.

## Input System.

We going to use ```input.c```, ```input.h```, ```player_controller.c```, and ```player_controller.h``` which have been designed to make handling inputs bit easier and also allow for custom key mappings for any gamepad you want to use.  I strongly recommend reading the Picocomputer documentation.  To get started add ```input.c``` and ```player_controller.c``` to CMakeLists.txt and include the headers in main.c.  

We need to allocate XRAM to fetch the current state of the inputs.  You can choose any location in XRAM that is not being used by other assets.  In our constants.h file, we have allocated the beginning of XRAM for sprite data, so we can start our input state right after the sprite data.  Update your constants.h file to include the following:

```c
// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield
```

and then update your main function look like:

```c
int main(void)
{

    // Initialize input
    xreg(0, 0, 0, KEYBOARD_INPUT);
    xreg(0, 0, 2, GAMEPAD_INPUT);

    // Initialise graphics
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
    xreg(0, 0, 0, KEYBOARD_INPUT);
    xreg(0, 0, 2, GAMEPAD_INPUT);
```
This sets up the XRAM addresses for the keyboard and gamepad inputs and enables the devices.  

```c
    init_input_system();
    player_controller_init();
```
This initializes our input handling system and our player controller.  The input system will also look for ``JOYSTICK_SH.DAT``` and if it exists, it will load custom key mappings from that file.  This allows you to set up custom key mappings for any gamepad you want to use with your Picocomputer.  We will cover how to set up custom key mappings in a later section.

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
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, y);
}
```

and add an extern to the header file:

```c
void sprite_mode5_set_position(int16_t x, int16_t y);
```

The key here is,

```c
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, x_pos_px, x);
    xram0_struct_set(PLAYER_CONFIG, vga_mode5_sprite_t, y_pos_px, y);
```
We are directly updating the XRAM values for the sprite's position.  This is a powerful feature of the Picocomputer, as it allows us to update sprite properties directly from our game logic without needing to make expensive system calls.  By writing directly to XRAM, we can achieve very fast updates to our sprites, which is essential for smooth gameplay.

At this point, you should be able to move your player sprite around the screen using the inputs you have set up.  You can customize the input handling logic in ```player_controller_update()```.  

## Tilemaps and Backgrounds

Each layer can have a fill and sprite component.  So far we have added a sprite to layer 2 (top).  Now we are going to add tiles to layers 0, 1 and 2 to add a slowly moving background, with a faster foreground to create a parallax effect.  The will we add a top layer for a HUD to show a score.  

This will be done with ```tile_mode2.c``` and ```tile_mode2.h```.  You can added this to your CMakeLists.txt, add the header to main.c, and add ```tile_mode2_init();``` to ```init_graphics()``` just like we did with the sprite system.  In ```main.c``` we add ```tile_mode2_update_scroll();``` which will update the scroll position of the tilemaps to create a parallax scrolling effect.  The tile data is stored in XRAM and we can update it directly from our game logic just like we did with the sprites.  This allows us to create dynamic backgrounds that can change based on the player's actions or the game's state.





