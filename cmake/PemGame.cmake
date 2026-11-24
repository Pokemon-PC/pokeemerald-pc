# The game's own sources (src/*.c), built for the host.
#
# Each file goes through the GBA Makefile's pipeline (cpp | preproc), run by
# src/platform_pc/scripts/pp_c.py, then is compiled as plain C. Files whose
# job is done by the PC HAL instead are left out (PEM_GAME_EXCLUDE).

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PEM_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PEM_EXE_SUFFIX "")
if(WIN32)
    set(PEM_EXE_SUFFIX ".exe")
endif()
set(PEM_PREPROC ${PEM_ROOT}/tools/preproc/preproc${PEM_EXE_SUFFIX})
if(NOT EXISTS ${PEM_PREPROC})
    message(FATAL_ERROR "tools not built: cmake -S src/platform_pc/tools -B build-tools && cmake --build build-tools")
endif()
if(NOT EXISTS ${PEM_ROOT}/build/assets)
    message(WARNING "build/assets missing: run python src/platform_pc/scripts/gen_assets.py")
endif()

# Replaced by the HAL (same symbols, see src/platform_pc/*_pc.c).
set(PEM_GAME_EXCLUDE
    src/agb_flash.c src/agb_flash_1m.c src/agb_flash_mx.c   # flash_pc.c
    src/siirtc.c                                            # rtc_pc.c
    # ARM-only link cable / wireless adapter code: no link on PC ("cable
    # unplugged"), the symbols they export are stubbed in link_pc.c.
    src/librfu_intr.c src/multiboot.c
)

file(GLOB PEM_GAME_SOURCES RELATIVE ${PEM_ROOT} ${PEM_ROOT}/src/*.c)
list(REMOVE_ITEM PEM_GAME_SOURCES ${PEM_GAME_EXCLUDE})

# Preprocessor flags, as CPPFLAGS in the Makefile (MODERN=1).
set(PEM_CPP_FLAGS
    -iquote ${PEM_ROOT}/include
    -iquote ${PEM_ROOT}/src/platform_pc
    -DMODERN=1 -DPLATFORM_PC=1
    -fms-extensions -funsigned-char    # same as the compile step (system headers test them)
    -Wno-trigraphs)

# All writable game variables go to one section (snapshot for soft reset,
# see src/platform_pc/main_pc.c). COFF: grouped section between markers.
if(WIN32)
    set(PEM_GAME_DATA_SECTION ".data$pem1")
elseif(APPLE)
    set(PEM_GAME_DATA_SECTION "__DATA,__pemgame")
else()
    set(PEM_GAME_DATA_SECTION "pem_game")
endif()

# French text (src/platform_pc/lang_pc.c): room and registry for each string
# of the map, filled at run time from the player's French ROM.
set(PEM_LANG_MAP ${PEM_ROOT}/src/platform_pc/lang/fr_bpef.tsv)
set(PEM_LANG_DEPS ${PEM_LANG_MAP} ${PEM_ROOT}/src/platform_pc/scripts/lang/lang_build.py)
if(WIN32)
    set(PEM_LANG_SECTION ".rdata$pemlang1")
elseif(APPLE)
    set(PEM_LANG_SECTION "__DATA,__pemlang")
else()
    set(PEM_LANG_SECTION "pem_lang")
endif()

set(PEM_PP_DIR ${CMAKE_CURRENT_BINARY_DIR}/pp)
set(PEM_GAME_PP_SOURCES)
foreach(src ${PEM_GAME_SOURCES})
    string(REPLACE "/" "__" flat ${src})
    set(out ${PEM_PP_DIR}/${flat})
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/pp_c.py
                --clang ${CMAKE_C_COMPILER} --preproc ${PEM_PREPROC} --root ${PEM_ROOT}
                --out ${out} --dep ${out}.d --data-section ${PEM_GAME_DATA_SECTION}
                --lang-map ${PEM_LANG_MAP} --lang-section ${PEM_LANG_SECTION}
                -- ${PEM_CPP_FLAGS} ${PEM_ROOT}/${src}
        DEPENDS ${PEM_ROOT}/${src} ${PEM_ROOT}/src/platform_pc/scripts/pp_c.py ${PEM_LANG_DEPS}
        DEPFILE ${out}.d
        COMMENT "preproc ${src}"
        VERBATIM)
    list(APPEND PEM_GAME_PP_SOURCES ${out})
endforeach()

add_library(pem_game STATIC ${PEM_GAME_PP_SOURCES})
target_compile_options(pem_game PRIVATE
    -fno-strict-aliasing -fwrapv -funsigned-char
    -fms-extensions                           # PC_PTR32 = __ptr32 __uptr
    -fno-zero-initialized-in-bss              # zeros too go to the game data section
    -w                                        # decompiled code: thousands of noise warnings
    -Werror=implicit-function-declaration
    -Werror=int-conversion
    -Werror=incompatible-pointer-types
    -Werror=return-type)
target_link_libraries(pem_game PUBLIC pem_hal_game pem_hal)

# --- data written in assembly (scripts, maps, sound) -----------------------
# Same pipeline as the Makefile's DATA_ASM rule, assembled for the host by
# GNU as (pp_s.py explains why not Clang's assembler).
# llvm-mingw names its LLVM assembler "x86_64-w64-mingw32-as" too: look for
# every "as" on the PATH and keep the first one that is GNU as.
if(NOT PEM_GNU_AS)
    execute_process(COMMAND where as OUTPUT_VARIABLE _as_list ERROR_QUIET)
    if(NOT WIN32)
        execute_process(COMMAND sh -c "which -a as" OUTPUT_VARIABLE _as_list ERROR_QUIET)
    endif()
    string(REPLACE "
" ";" _as_list "${_as_list}")
    foreach(_as ${_as_list})
        string(STRIP "${_as}" _as)
        if(_as)
            execute_process(COMMAND "${_as}" --version OUTPUT_VARIABLE _v ERROR_QUIET)
            if(_v MATCHES "GNU assembler")
                set(PEM_GNU_AS "${_as}" CACHE FILEPATH "GNU as for the host")
                break()
            endif()
        endif()
    endforeach()
endif()
if(NOT PEM_GNU_AS)
    message(FATAL_ERROR "GNU as (binutils) not found: install binutils / WinLibs, or set PEM_GNU_AS")
endif()
message(STATUS "GNU as: ${PEM_GNU_AS}")
file(GLOB PEM_DATA_ASM RELATIVE ${PEM_ROOT} ${PEM_ROOT}/data/*.s)
file(GLOB PEM_SONG_ASM RELATIVE ${PEM_ROOT} ${PEM_ROOT}/sound/songs/midi/*.s)
set(PEM_PP_S_FLAGS)
if(WIN32)
    set(PEM_PP_S_FLAGS --coff)
endif()
set(PEM_GAME_DATA_OBJS)
foreach(src ${PEM_DATA_ASM})
    string(REPLACE "/" "__" flat ${src})
    set(pps ${PEM_PP_DIR}/${flat})
    set(obj ${PEM_PP_DIR}/${flat}.obj)
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/pp_s.py ${PEM_PP_S_FLAGS}
                --clang ${CMAKE_C_COMPILER} --preproc ${PEM_PREPROC} --root ${PEM_ROOT}
                --out ${pps} --dep ${pps}.d --lang-map ${PEM_LANG_MAP}
                -- -iquote ${PEM_ROOT}/include -DMODERN=1 -DPLATFORM_PC=1 ${PEM_ROOT}/${src}
        COMMAND ${PEM_GNU_AS} -I ${PEM_ROOT} -I ${PEM_ROOT}/include -o ${obj} ${pps}
        DEPENDS ${PEM_ROOT}/${src} ${PEM_ROOT}/src/platform_pc/scripts/pp_s.py ${PEM_LANG_DEPS}
        DEPFILE ${pps}.d
        WORKING_DIRECTORY ${PEM_ROOT}
        COMMENT "asm ${src}"
        VERBATIM)
    list(APPEND PEM_GAME_DATA_OBJS ${obj})
endforeach()
# Songs (mid2agb output): assembled directly with -I sound, as in
# audio_rules.mk, after the ARM -> x86 syntax fixes (pp_s.py --raw). Their
# common include, sound/MPlayDef.s, gets the same treatment.
set(PEM_SOUND_INC ${PEM_PP_DIR}/sound_inc)
add_custom_command(
    OUTPUT ${PEM_SOUND_INC}/MPlayDef.s
    COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/pp_s.py ${PEM_PP_S_FLAGS} --raw
            --root ${PEM_ROOT} --out ${PEM_SOUND_INC}/MPlayDef.s --dep ${PEM_SOUND_INC}/MPlayDef.s.d
            -- ${PEM_ROOT}/sound/MPlayDef.s
    DEPENDS ${PEM_ROOT}/sound/MPlayDef.s ${PEM_ROOT}/src/platform_pc/scripts/pp_s.py
    VERBATIM)
foreach(src ${PEM_SONG_ASM})
    string(REPLACE "/" "__" flat ${src})
    set(pps ${PEM_PP_DIR}/${flat})
    set(obj ${pps}.obj)
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/pp_s.py ${PEM_PP_S_FLAGS} --raw
                --root ${PEM_ROOT} --out ${pps} --dep ${pps}.d -- ${PEM_ROOT}/${src}
        COMMAND ${PEM_GNU_AS} -I ${PEM_SOUND_INC} -I ${PEM_ROOT}/sound -o ${obj} ${pps}
        DEPENDS ${PEM_ROOT}/${src} ${PEM_SOUND_INC}/MPlayDef.s ${PEM_ROOT}/src/platform_pc/scripts/pp_s.py
        WORKING_DIRECTORY ${PEM_ROOT}
        COMMENT "asm ${src}"
        VERBATIM)
    list(APPEND PEM_GAME_DATA_OBJS ${obj})
endforeach()
set_source_files_properties(${PEM_GAME_DATA_OBJS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

# --- the executable -----------------------------------------------------------
add_executable(pokeemerald
    ${PEM_ROOT}/src/platform_pc/main_pc.c
    ${PEM_ROOT}/src/platform_pc/link_pc.c
    ${PEM_ROOT}/src/platform_pc/sound_pc.c
    ${PEM_ROOT}/src/platform_pc/layout_check_pc.c
    ${PEM_ROOT}/src/platform_pc/debug_pc.c
    ${PEM_ROOT}/src/platform_pc/lang_pc.c
    ${PEM_ROOT}/src/platform_pc/lang_tables_pc.c
    ${PEM_GAME_DATA_OBJS})
target_link_libraries(pokeemerald PRIVATE pem_game pem_hal_game pem_hal pem_game_flags pem::SDL2main pem::SDL2)
pem_low_address_link(pokeemerald)
if(WIN32 AND CMAKE_C_COMPILER_LINKER_ID STREQUAL "LLD")   # lld-only option (all undefined symbols)
    target_link_options(pokeemerald PRIVATE -Wl,--error-limit=0)
endif()

# End-to-end: boot to a new game, sound recorded (needs the generated assets).
add_test(NAME game_smoke
         COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/smoke_test.py
                 $<TARGET_FILE:pokeemerald> ${CMAKE_CURRENT_BINARY_DIR}/smoke)
set_tests_properties(game_smoke PROPERTIES TIMEOUT 600)

# Longer: new game -> truck -> Littleroot Town -> inside the house.
add_test(NAME game_world
         COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/smoke_test.py --world
                 $<TARGET_FILE:pokeemerald> ${CMAKE_CURRENT_BINARY_DIR}/world)
set_tests_properties(game_world PROPERTIES TIMEOUT 900)

# Menus (bag, trainer card, party, summary), save, then continue the save.
add_test(NAME game_save
         COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/smoke_test.py --save
                 $<TARGET_FILE:pokeemerald> ${CMAKE_CURRENT_BINARY_DIR}/savetest)
set_tests_properties(game_save PROPERTIES TIMEOUT 900)

# First act of the story from that save: wall clock, rival, Route 101,
# starter, first battle, Prof. Birch's lab.
add_test(NAME game_story
         COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/smoke_test.py --story
                 $<TARGET_FILE:pokeemerald> ${CMAKE_CURRENT_BINARY_DIR}/storytest)
set_tests_properties(game_story PROPERTIES TIMEOUT 1500)

# French: only with the player's own French ROM (never part of the repo).
#   cmake -B build-pc -DPEM_FR_ROM="path/to/Pokemon - Version Emeraude (France).gba"
set(PEM_FR_ROM "" CACHE FILEPATH "Your French Emerald ROM, for the game_french test")
if(PEM_FR_ROM)
    add_test(NAME game_french
             COMMAND ${Python3_EXECUTABLE} ${PEM_ROOT}/src/platform_pc/scripts/smoke_test.py --french
                     $<TARGET_FILE:pokeemerald> ${CMAKE_CURRENT_BINARY_DIR}/frenchtest ${PEM_FR_ROM})
    set_tests_properties(game_french PROPERTIES TIMEOUT 900)
endif()
