/*
 * main_pc.c - entry point of the PC build: hardware, then AgbMain().
 *
 * Soft reset (A+B+Start+Select, end of the credits, ...): on the GBA the
 * BIOS clears the RAM and restarts the ROM, so every game variable starts
 * over. Here the game's variables all live in one section (pp_c.py places
 * them there); it is saved before the first AgbMain() and restored by
 * SoftReset(), which then longjmps back to the start of the game.
 *
 * Test hooks (environment):
 *   PEM_EXIT_AFTER=N        quit after N frames
 *   PEM_SHOT=F:path.bmp     save frame F (repeatable: F1:a.bmp,F2:b.bmp)
 *   PEM_INPUT=F1-F2:A+UP,.. scripted keys (see scripted_input)
 */
#include <SDL.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw_pc.h"
#include "pc_hal.h"
#include "platform_pc.h"

void AgbMain(void);
void RegisterRamReset(uint32_t flags);

/* ---- the game's variables ------------------------------------------------ */
#if defined(_WIN32)
/* lld sorts grouped COFF sections by name: $pem0 < $pem1 (game) < $pem2. */
__attribute__((section(".data$pem0"), used)) static char s_game_begin[16];
__attribute__((section(".data$pem2"), used)) static char s_game_end[16];
#define GAME_DATA_BEGIN ((char *)(s_game_begin + sizeof s_game_begin))
#define GAME_DATA_END   ((char *)s_game_end)
#elif defined(__APPLE__)
#error "macOS: section bounds not implemented yet (see the HGSS port, 02 section 7)"
#else
/* The linker defines __start_/__stop_ for sections named like C identifiers. */
extern char __start_pem_game[], __stop_pem_game[];
__attribute__((section("pem_game"), used)) static char s_game_anchor = 1;
#define GAME_DATA_BEGIN __start_pem_game
#define GAME_DATA_END   __stop_pem_game
#endif

static void *s_snapshot;
static size_t s_snapshot_size;
static jmp_buf s_reset_jump;

static void snapshot_game_data(void) {
    s_snapshot_size = (size_t)(GAME_DATA_END - GAME_DATA_BEGIN);
    s_snapshot = malloc(s_snapshot_size);
    if (!s_snapshot) {
        pc_fatal("out of memory for the reset snapshot (%zu bytes)", s_snapshot_size);
    }
    memcpy(s_snapshot, GAME_DATA_BEGIN, s_snapshot_size);
    pc_log("game variables: %zu KiB at %p", s_snapshot_size / 1024, (void *)GAME_DATA_BEGIN);
}

/* BIOS SoftReset, as libagbsyscall calls it: RegisterRamReset(flags), then
 * restart from the ROM entry point (crt0 clears all RAM again). */
void SoftReset(uint32_t flags) {
    pc_log("soft reset");
    pc_flash_flush();
    RegisterRamReset(flags | 0xFF);
    longjmp(s_reset_jump, 1);
}

/* Same entry the ROM header jumps to when SoftResetRom is used. */
void SoftResetRom(uint32_t flags) {
    SoftReset(flags);
}

/* ---- test hooks ---------------------------------------------------------- */
static long s_exit_after = -1;
static char s_shots[512];

static void save_bmp(const char *path, const uint32_t *argb) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom((void *)argb, PC_GBA_WIDTH, PC_GBA_HEIGHT, 32,
                                                        PC_GBA_WIDTH * 4, SDL_PIXELFORMAT_ARGB8888);
    if (!s || SDL_SaveBMP(s, path) != 0) {
        pc_warn("cannot save %s: %s", path, SDL_GetError());
    } else {
        pc_log("saved %s", path);
    }
    if (s) {
        SDL_FreeSurface(s);
    }
}

/* PEM_REGS=F1,F2,...: log the display registers and palette at those frames. */
static char s_regs[256];

static void dump_regs(uint64_t frame) {
    const volatile uint16_t *io = (const volatile uint16_t *)(uintptr_t)PC_IO_BASE;
    const volatile uint16_t *pal = (const volatile uint16_t *)(uintptr_t)PC_PLTT_BASE;
    pc_log("regs@%llu DISPCNT=%04X BG0CNT=%04X BG1CNT=%04X BG2CNT=%04X BG3CNT=%04X "
           "BLDCNT=%04X BLDALPHA=%04X BLDY=%04X WININ=%04X WINOUT=%04X IE=%04X IME=%04X DISPSTAT=%04X",
           (unsigned long long)frame, io[0], io[4], io[5], io[6], io[7], io[0x28], io[0x29], io[0x2A],
           io[0x24], io[0x25], io[0x100], io[0x104], io[2]);
    char line[512];
    int n = 0;
    for (int i = 0; i < 32; i++) {
        n += snprintf(line + n, sizeof line - (size_t)n, "%04X ", pal[i]);
    }
    pc_log("regs@%llu BGPAL[0..31] %s", (unsigned long long)frame, line);
    n = 0;
    for (int i = 256; i < 272; i++) {
        n += snprintf(line + n, sizeof line - (size_t)n, "%04X ", pal[i]);
    }
    pc_log("regs@%llu OBJPAL[0..15] %s", (unsigned long long)frame, line);
}

static void frame_hook(uint64_t frame, const uint32_t *argb) {
    if (s_shots[0]) {
        char list[512];
        snprintf(list, sizeof list, "%s", s_shots);
        for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ",")) {
            char *colon = strchr(tok, ':');
            if (colon && strtoull(tok, NULL, 10) == frame) {
                save_bmp(colon + 1, argb);
            }
        }
    }
    if (s_regs[0]) {
        char list[256];
        snprintf(list, sizeof list, "%s", s_regs);
        for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ",")) {
            if (strtoull(tok, NULL, 10) == frame) {
                dump_regs(frame);
            }
        }
    }
    if (s_exit_after >= 0 && (long)frame >= s_exit_after) {
        pc_log("PEM_EXIT_AFTER reached (%ld frames)", s_exit_after);
        pc_flash_flush();
        exit(0);
    }
}

/* PEM_INPUT="F1-F2:KEYS,F3:KEYS,..." holds KEYS (A,B,START,SELECT,UP,DOWN,
 * LEFT,RIGHT,L,R joined by '+') during frames F1..F2 (or frame F3 only).
 * Replaces keyboard/gamepad input for the whole run. */
static char s_input[1024];

static uint16_t parse_keys(const char *s) {
    static const struct { const char *name; uint16_t bit; } kKeys[] = {
        { "A", 0x001 }, { "B", 0x002 }, { "SELECT", 0x004 }, { "START", 0x008 },
        { "RIGHT", 0x010 }, { "LEFT", 0x020 }, { "UP", 0x040 }, { "DOWN", 0x080 },
        { "R", 0x100 }, { "L", 0x200 },
    };
    uint16_t keys = 0;
    char buf[128];
    snprintf(buf, sizeof buf, "%s", s);
    for (char *tok = strtok(buf, "+"); tok; tok = strtok(NULL, "+")) {
        for (unsigned i = 0; i < sizeof kKeys / sizeof kKeys[0]; i++) {
            if (strcmp(tok, kKeys[i].name) == 0) {
                keys |= kKeys[i].bit;
            }
        }
    }
    return keys;
}

static uint16_t scripted_input(uint64_t frame) {
    char list[1024];
    uint16_t keys = 0;
    snprintf(list, sizeof list, "%s", s_input);
    char *save = NULL;
    for (char *tok = strtok_r(list, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char *colon = strchr(tok, ':');
        if (!colon) {
            continue;
        }
        *colon = '\0';
        char *dash = strchr(tok, '-');
        uint64_t f1 = strtoull(tok, NULL, 10);
        uint64_t f2 = dash ? strtoull(dash + 1, NULL, 10) : f1;
        if (frame >= f1 && frame <= f2) {
            keys |= parse_keys(colon + 1);
        }
    }
    return keys;
}

/* ---- entry ----------------------------------------------------------------- */
static void run_game(void *arg) {
    (void)arg;
    pc_crash_install();
    snapshot_game_data();
    if (setjmp(s_reset_jump) != 0) {
        memcpy(GAME_DATA_BEGIN, s_snapshot, s_snapshot_size);
        for (int ch = 0; ch < 4; ch++) {
            pc_dma_stop(ch);
        }
        pc_timers_reset();
    }
    RegisterRamReset(0xFF);   /* crt0: clear RAM and registers before AgbMain */
    AgbMain();
    pc_warn("AgbMain returned");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const char *e = getenv("PEM_EXIT_AFTER");
    if (e && *e) {
        s_exit_after = strtol(e, NULL, 10);
    }
    e = getenv("PEM_SHOT");
    if (e && *e) {
        snprintf(s_shots, sizeof s_shots, "%s", e);
    }
    if (!pc_hw_init()) {
        pc_fatal("cannot reserve the GBA memory map");
    }
    if (!pc_video_init()) {
        pc_fatal("cannot open the window");
    }
    pc_audio_init();
    pc_video_set_frame_hook(frame_hook);
    e = getenv("PEM_REGS");
    if (e && *e) {
        snprintf(s_regs, sizeof s_regs, "%s", e);
    }
    e = getenv("PEM_INPUT");
    if (e && *e) {
        snprintf(s_input, sizeof s_input, "%s", e);
        pc_input_set_override(scripted_input);
    }
    pc_run_on_low_stack(run_game, NULL);
    pc_flash_flush();
    pc_audio_shutdown();
    pc_video_shutdown();
    SDL_Quit();
    return 0;
}
