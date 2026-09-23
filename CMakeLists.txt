cmake_minimum_required(VERSION 3.21)

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(RPStarHopper C CXX ASM)

add_executable(RPStarHopper)
rp6502_map(RPStarHopper src/xram.h "XRAM_.*")

# XRAM Assets
rp6502_asset(RPStarHopper XRAM(XRAM_PLAYER_DATA) images/Player_4bpp.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_BG_DATA) images/StarFields_BG_map.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_FG_DATA) images/StarFields_FG_map.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_HUD_DATA) images/StarFields_HUD_map.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_STARFIELD_TILES_DATA) images/StarFields_tiles_4bpp.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_PROJECTILE_DATA) images/Projectiles_4bpp.bin)
rp6502_asset(RPStarHopper XRAM(XRAM_ENEMY_DATA) images/Enemies_4bpp.bin)
# SFX command streams, see src/sfx.c / tools/generate_sfx.py.
rp6502_asset(RPStarHopper XRAM(XRAM_SFX_DATA) music/sfx/sfx_xram.bin)
rp6502_asset(RPStarHopper help src/help.txt)

# ROM Assets
rp6502_asset(RPStarHopper StarFields_HUD_map.bin images/StarFields_HUD_map.bin)
rp6502_asset(RPStarHopper Title.vgm music/Title.vgm)
rp6502_asset(RPStarHopper Level_01.vgm music/Level_01.vgm)
rp6502_asset(RPStarHopper Level_02.vgm music/Level_02.vgm)
rp6502_asset(RPStarHopper Level_03.vgm music/Level_03.vgm)
rp6502_asset(RPStarHopper Level_04.vgm music/Level_04.vgm)
rp6502_asset(RPStarHopper Level_05.vgm music/Level_05.vgm)
rp6502_asset(RPStarHopper Level_06.vgm music/Level_06.vgm)
rp6502_asset(RPStarHopper Level_07.vgm music/Level_07.vgm)
rp6502_asset(RPStarHopper Boss.vgm music/Boss.vgm)
rp6502_asset(RPStarHopper BossFinal.vgm music/BossFinal.vgm)
rp6502_asset(RPStarHopper Bonus.vgm music/Bonus.vgm)
rp6502_asset(RPStarHopper Gameover.vgm music/Gameover.vgm)
rp6502_asset(RPStarHopper Victory.vgm music/Victory.vgm)



rp6502_executable(RPStarHopper DATA default RESET default)

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

# Size-focused flags, applied to both presets.
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

rp6502_executable(GamepadMapper DATA default RESET default)

target_sources(GamepadMapper PRIVATE
    src/gamepad_mapper.c
)
