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
    src/sprite_mode5.c
    src/tile_mode2.c
    src/input.c
    src/player_controller.c
)
