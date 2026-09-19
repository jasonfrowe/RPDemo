cmake_minimum_required(VERSION 3.21)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(RPStarHopper C CXX ASM)

add_executable(RPStarHopper)

# XRAM Assets
rp6502_asset(RPStarHopper 0x10000 images/Player_4bpp.bin)
rp6502_asset(RPStarHopper 0x10300 images/StarFields_BG_map.bin)
rp6502_asset(RPStarHopper 0x10C60 images/StarFields_FG_map.bin)
rp6502_asset(RPStarHopper 0x115C0 images/StarFields_HUD_map.bin)
rp6502_asset(RPStarHopper 0x11A70 images/StarFields_tiles_4bpp.bin)
rp6502_asset(RPStarHopper 0x13A70 images/Projectiles_4bpp.bin)
rp6502_asset(RPStarHopper 0x13C10 images/Enemies_4bpp.bin)
rp6502_asset(RPStarHopper help src/help.txt)

# ROM Assets
rp6502_asset(RPStarHopper StarFields_HUD_map.bin images/StarFields_HUD_map1.bin)
rp6502_asset(RPStarHopper Title.vgm music/Title.vgm)
rp6502_asset(RPStarHopper Level_01.vgm music/Level_01.vgm)
rp6502_asset(RPStarHopper Level_02.vgm music/Level_02.vgm)
rp6502_asset(RPStarHopper Level_03.vgm music/Level_03.vgm)
rp6502_asset(RPStarHopper Level_04.vgm music/Level_04.vgm)
rp6502_asset(RPStarHopper Level_05.vgm music/Level_05.vgm)
rp6502_asset(RPStarHopper Level_06.vgm music/Level_06.vgm)
rp6502_asset(RPStarHopper Level_07.vgm music/Level_07.vgm)
rp6502_asset(RPStarHopper Boss.vgm music/Boss.vgm)
rp6502_asset(RPStarHopper Bonus.vgm music/Bonus.vgm)
rp6502_asset(RPStarHopper Gameover.vgm music/Gameover.vgm)

# SFX (channels 7/8 only -- see src/sfx.c / tools/generate_sfx.py)
rp6502_asset(RPStarHopper PlyrFire.vgm music/sfx/PlyrFire.vgm)
rp6502_asset(RPStarHopper EnmyFire.vgm music/sfx/EnmyFire.vgm)
rp6502_asset(RPStarHopper EnmyDie.vgm music/sfx/EnmyDie.vgm)
rp6502_asset(RPStarHopper PlyrHit.vgm music/sfx/PlyrHit.vgm)
rp6502_asset(RPStarHopper PlyrDie.vgm music/sfx/PlyrDie.vgm)
rp6502_asset(RPStarHopper PickUp.vgm music/sfx/PickUp.vgm)
rp6502_asset(RPStarHopper LvlClear.vgm music/sfx/LvlClear.vgm)
rp6502_asset(RPStarHopper LowEnrgy.vgm music/sfx/LowEnrgy.vgm)



rp6502_executable(RPStarHopper 
    DATA file 
    RESET file
)

target_sources(RPStarHopper PRIVATE
    src/main.c
    src/sprite_mode5.c
    src/tile_mode2.c
    src/input.c
    src/player_controller.c
    src/game_state.c
    src/gameplay.c
    src/gameplay_playing.c
    src/gameplay_bonus.c
    src/gameplay_game_over.c
    src/gameplay_boss.c
    src/projectile.c
    src/rng.c
    src/enemy.c
    src/score.c
    src/level_bonus.c
    src/music.c
    src/sfx.c
    src/opl.c
    src/vgm.c
)

# Additional size-focused flags on top of MinSizeRel.
target_compile_options(RPStarHopper PRIVATE
    -Oz
    -ffunction-sections
    -fdata-sections
    -flto
)

target_link_options(RPStarHopper PRIVATE
    -Wl,--gc-sections
    -flto
)

# Gamepad mapper utility
add_executable(GamepadMapper)

rp6502_executable(GamepadMapper
    DATA file
    RESET file
)

target_sources(GamepadMapper PRIVATE
    src/gamepad_mapper.c
    src/input.c
)
