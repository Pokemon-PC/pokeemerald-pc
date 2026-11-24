/*
 * pc_hal.h - what the game (under PLATFORM_PC) calls into the PC HAL.
 *
 * Only plain C types here: this header is included from the game's own
 * headers (include/gba/macro.h) and from HAL files that do not see them.
 */
#ifndef PEM_PC_HAL_H
#define PEM_PC_HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DMA (dma_pc.c) -------------------------------------------------------
 * The DmaSet/DmaStop macros call these instead of writing REG_DMAxSAD/DAD/CNT:
 * a store to plain memory cannot start a transfer on PC. Full 64-bit
 * pointers are kept; the registers still receive their (32-bit) values for
 * code that reads them back. control = DMAxCNT_H << 16 | count. */
void pc_dma_set(int ch, const volatile void *src, volatile void *dest, uint32_t control);
void pc_dma_stop(int ch);
void pc_dma_on_hblank(void);   /* visible lines only */
void pc_dma_on_vblank(void);

/* ---- interrupts and frame timing (irq_pc.c) ------------------------------ */
typedef void (*PcIntrFunc)(void);
/* Table in the game's order (gIntrTableTemplate: VCount, Serial, Timer3,
 * HBlank, VBlank, Timer0-2, DMA0-3, Keypad, GamePak), as IntrMain uses it. */
void pc_irq_set_table(PcIntrFunc *table, int count);
/* Raises an IF flag (INTR_FLAG_*) and runs the handler if IME/IE allow it. */
void pc_irq_raise(uint16_t flag);
/* Runs the display until the start of the next VBlank: renders the
 * remaining lines (VCount/HBlank IRQs, HBlank DMA), presents the frame,
 * paces to 59.73 Hz, reads input, then raises the VBlank IRQ.
 * This is what WaitForVBlank()/VBlankIntrWait() do on PC. */
void pc_wait_vblank(void);
uint64_t pc_frame_count(void);

/* ---- video and input (video_pc.c) ---------------------------------------- */
bool pc_video_init(void);          /* PEM_HEADLESS=1: no window */
void pc_video_present(const uint32_t *argb240x160);
/* Polls host events; returns the GBA keys held (active high, KEY_* bits). */
uint16_t pc_input_poll(void);
bool pc_quit_requested(void);
void pc_video_shutdown(void);
/* Test hook: when set, replaces keyboard/gamepad input for every frame. */
void pc_input_set_override(uint16_t (*fn)(uint64_t frame));
/* Test hook: called with every finished frame (240x160 ARGB). */
void pc_video_set_frame_hook(void (*fn)(uint64_t frame, const uint32_t *argb));

/* ---- backup memory (flash_pc.c) ------------------------------------------ */
/* Path of the 128 KiB flash image; default "save.bin" (env PEM_SAVE). */
void pc_flash_set_path(const char *path);
const uint8_t *pc_flash_data(void);
/* Writes pending changes to disk (called at each VBlank and at exit). */
void pc_flash_flush(void);

/* ---- real-time clock (rtc_pc.c) ------------------------------------------ */
/* Host local time + an offset kept next to the save (set when the game
 * resets or sets the clock), like a cartridge battery. */
void pc_rtc_set_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PEM_PC_HAL_H */
