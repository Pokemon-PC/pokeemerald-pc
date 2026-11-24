/*
 * debug_pc.c - game-state dumps for debugging (built with the game headers).
 * Called by main_pc.c for PEM_REGS frames: main callbacks and active tasks,
 * as addresses that src/platform_pc/scripts/symbolize.py turns into names
 * (lines formatted like backtrace frames: "#NN 0x...").
 */
#include "platform_pc.h"

#include "global.h"
#include "main.h"
#include "task.h"

void pc_debug_dump_game(unsigned long long frame) {
    pc_log("game@%llu state=%u", frame, gMain.state);
    pc_log("game@%llu   #00 0x%016llX callback1", frame, (unsigned long long)(uintptr_t)gMain.callback1);
    pc_log("game@%llu   #00 0x%016llX callback2", frame, (unsigned long long)(uintptr_t)gMain.callback2);
    pc_log("game@%llu   #00 0x%016llX vblankCallback", frame, (unsigned long long)(uintptr_t)gMain.vblankCallback);
    for (int i = 0; i < NUM_TASKS; i++) {
        if (!gTasks[i].isActive) {
            continue;
        }
        pc_log("game@%llu   #00 0x%016llX task %d data=%d,%d,%d,%d,%d,%d,%d,%d", frame,
               (unsigned long long)(uintptr_t)gTasks[i].func, i, gTasks[i].data[0], gTasks[i].data[1],
               gTasks[i].data[2], gTasks[i].data[3], gTasks[i].data[4], gTasks[i].data[5],
               gTasks[i].data[6], gTasks[i].data[7]);
    }
}
