# pokeemerald - native PC build (Windows/Linux/macOS), CMake + Clang + SDL2.
# The GBA ROM is still built with `make` (Makefile), unchanged and matching.
#
#   cmake -S src/platform_pc/tools -B build-tools -G Ninja && cmake --build build-tools
#   python src/platform_pc/scripts/gen_assets.py
#   cmake -B build-pc -G Ninja -DCMAKE_C_COMPILER=clang && cmake --build build-pc
cmake_minimum_required(VERSION 3.20)
project(pokeemerald_pc C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The PC port targets 64-bit hosts only.")
endif()

include(cmake/PemSDL2.cmake)

set(PEM_PC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/platform_pc)

# --- low-address executables (same rationale as the HeartGold port) -------
# The game keeps code/data addresses in 32-bit words (.4byte pointers in the
# script bytecode, u32 casts, task args): link at 0x10000000, without ASLR.
function(pem_low_address_link target)
    if(WIN32)
        target_link_options(${target} PRIVATE
            -Wl,--image-base=0x10000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va)
    elseif(APPLE)
        target_link_options(${target} PRIVATE -Wl,-pagezero_size,0x1000 -Wl,-image_base,0x10000000
            -Wl,-no_pie -Wl,-no_fixup_chains)
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
    else()
        include(CheckLinkerFlag)
        check_linker_flag(C "-Wl,-Ttext-segment=0x10000000" PEM_LD_TTEXT_SEGMENT)
        target_link_options(${target} PRIVATE -no-pie)
        if(PEM_LD_TTEXT_SEGMENT)
            target_link_options(${target} PRIVATE -Wl,-Ttext-segment=0x10000000)
        else()
            target_link_options(${target} PRIVATE -Wl,--image-base=0x10000000)
        endif()
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
    endif()
endfunction()

# --- HAL: GBA hardware reproduced on the host -----------------------------
add_library(pem_hal STATIC
    ${PEM_PC_DIR}/log_pc.c
    ${PEM_PC_DIR}/hw_pc.c
    ${PEM_PC_DIR}/bios_pc.c
    ${PEM_PC_DIR}/gpu_pc.c
    ${PEM_PC_DIR}/dma_pc.c
    ${PEM_PC_DIR}/irq_pc.c
    ${PEM_PC_DIR}/video_pc.c
    ${PEM_PC_DIR}/crash_pc.c
    ${PEM_PC_DIR}/timer_pc.c
    ${PEM_PC_DIR}/audio_pc.c
    ${PEM_PC_DIR}/nullread_pc.c
    ${PEM_PC_DIR}/config_pc.c)
target_include_directories(pem_hal PUBLIC ${PEM_PC_DIR})
target_compile_definitions(pem_hal PUBLIC PLATFORM_PC=1)
target_link_libraries(pem_hal PUBLIC pem::SDL2)
if(WIN32)
    target_link_libraries(pem_hal PUBLIC psapi dbghelp)
else()
    target_link_libraries(pem_hal PUBLIC m)
endif()

# Flags for code that includes the game's headers.
add_library(pem_game_flags INTERFACE)
# -iquote, not -I: include/ has malloc.h, strings.h... that would shadow the
# system headers.
target_compile_options(pem_game_flags INTERFACE "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(pem_game_flags INTERFACE MODERN=1 PLATFORM_PC=1)
target_compile_options(pem_game_flags INTERFACE
    -fno-strict-aliasing -fwrapv -funsigned-char
    -fms-extensions                           # PC_PTR32 = __ptr32 __uptr
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
    -Wno-gnu-alignof-expression   # MODERN alignment asserts in gba/macro.h
    -Wno-macro-redefined)         # include/config.h sets NDEBUG itself

# HAL pieces that implement game APIs (same symbols as the files they
# replace in the PC build: agb_flash*.c, siirtc.c), built with game headers.
add_library(pem_hal_game STATIC
    ${PEM_PC_DIR}/flash_pc.c
    ${PEM_PC_DIR}/rtc_pc.c)
target_link_libraries(pem_hal_game PUBLIC pem_hal PRIVATE pem_game_flags)
# pem_hal calls back into it (pc_flash_flush on crash, at VBlank): a cycle of
# static libraries, which CMake repeats on the link line for GNU ld.
target_link_libraries(pem_hal INTERFACE pem_hal_game)

# --- the game ---------------------------------------------------------
include(cmake/PemGame.cmake)

# --- M1: title screen graphics through the HAL ---------------------------
add_executable(pem_m1_view ${PEM_PC_DIR}/m1_view.c)
target_link_libraries(pem_m1_view PRIVATE pem_hal_game pem_hal pem_game_flags pem::SDL2main pem::SDL2)
pem_low_address_link(pem_m1_view)

# --- M2: hardware layer autotest ----------------------------------------
enable_testing()
add_executable(pem_autotest ${PEM_PC_DIR}/tests/autotest_pc.c)
target_link_libraries(pem_autotest PRIVATE pem_hal_game pem_hal pem_game_flags)
pem_low_address_link(pem_autotest)
add_test(NAME hal_autotest
         COMMAND pem_autotest ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
