cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

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
rp6502_asset(RPStarHopper RESOURCE.001.vgm music/RESOURCE.001.vgm)
rp6502_asset(RPStarHopper RESOURCE.002.vgm music/RESOURCE.002.vgm)
rp6502_asset(RPStarHopper RESOURCE.003.vgm music/RESOURCE.003.vgm)
rp6502_asset(RPStarHopper RESOURCE.005.vgm music/RESOURCE.005.vgm)
rp6502_asset(RPStarHopper RESOURCE.006.vgm music/RESOURCE.006.vgm)
rp6502_asset(RPStarHopper RESOURCE.008.vgm music/RESOURCE.008.vgm)
rp6502_asset(RPStarHopper RESOURCE.009.vgm music/RESOURCE.009.vgm)
rp6502_asset(RPStarHopper RESOURCE.010.vgm music/RESOURCE.010.vgm)
rp6502_asset(RPStarHopper RESOURCE.011.vgm music/RESOURCE.011.vgm)



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
