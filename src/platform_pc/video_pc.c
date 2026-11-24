/*
 * video_pc.c - SDL window, keyboard and gamepad.
 *
 * Default keys (same as mGBA): arrows, X = A, Z = B, A = L, S = R,
 * Enter = Start, Backspace = Select. F11 or Alt+Enter = fullscreen.
 * Any SDL game controller works too (Nintendo layout).
 * PEM_HEADLESS=1: no window (tests). PEM_SCALE=n: window size.
 */
#include "pc_hal.h"

#include "hw_pc.h"
#include "platform_pc.h"

#include <SDL.h>
#include <stdlib.h>

enum {
    KEY_A = 0x001, KEY_B = 0x002, KEY_SELECT = 0x004, KEY_START = 0x008,
    KEY_RIGHT = 0x010, KEY_LEFT = 0x020, KEY_UP = 0x040, KEY_DOWN = 0x080,
    KEY_R = 0x100, KEY_L = 0x200,
};

static SDL_Window *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture *s_tex;
static SDL_GameController *s_pad;
static bool s_headless, s_quit;
static uint16_t (*s_override)(uint64_t);
static void (*s_frame_hook)(uint64_t, const uint32_t *);

void pc_input_set_override(uint16_t (*fn)(uint64_t)) { s_override = fn; }
void pc_video_set_frame_hook(void (*fn)(uint64_t, const uint32_t *)) { s_frame_hook = fn; }
bool pc_quit_requested(void) { return s_quit; }

bool pc_video_init(void) {
    const char *h = getenv("PEM_HEADLESS");
    s_headless = h && *h && *h != '0';
    if (s_headless) {
        return true;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        pc_warn("SDL_Init: %s", SDL_GetError());
        return false;
    }
    int scale = 3;
    const char *s = getenv("PEM_SCALE");
    if (s && atoi(s) > 0) {
        scale = atoi(s);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    s_win = SDL_CreateWindow("Pokemon Emerald", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             PC_GBA_WIDTH * scale, PC_GBA_HEIGHT * scale, SDL_WINDOW_RESIZABLE);
    if (!s_win) {
        pc_warn("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }
    s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
    if (!s_ren) {
        s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_SOFTWARE);
    }
    SDL_RenderSetLogicalSize(s_ren, PC_GBA_WIDTH, PC_GBA_HEIGHT);
    SDL_RenderSetIntegerScale(s_ren, SDL_FALSE);
    s_tex = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                              PC_GBA_WIDTH, PC_GBA_HEIGHT);
    return s_tex != NULL;
}

void pc_video_present(const uint32_t *argb) {
    if (s_frame_hook) {
        s_frame_hook(pc_frame_count(), argb);
    }
    if (s_headless || !s_tex) {
        return;
    }
    SDL_UpdateTexture(s_tex, NULL, argb, PC_GBA_WIDTH * 4);
    SDL_RenderClear(s_ren);
    SDL_RenderCopy(s_ren, s_tex, NULL, NULL);
    SDL_RenderPresent(s_ren);
}

static uint16_t keyboard_keys(void) {
    const uint8_t *k = SDL_GetKeyboardState(NULL);
    uint16_t keys = 0;
    if (k[SDL_SCANCODE_X]) keys |= KEY_A;
    if (k[SDL_SCANCODE_Z]) keys |= KEY_B;
    if (k[SDL_SCANCODE_BACKSPACE]) keys |= KEY_SELECT;
    if (k[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & KMOD_ALT)) keys |= KEY_START;
    if (k[SDL_SCANCODE_RIGHT]) keys |= KEY_RIGHT;
    if (k[SDL_SCANCODE_LEFT]) keys |= KEY_LEFT;
    if (k[SDL_SCANCODE_UP]) keys |= KEY_UP;
    if (k[SDL_SCANCODE_DOWN]) keys |= KEY_DOWN;
    if (k[SDL_SCANCODE_S]) keys |= KEY_R;
    if (k[SDL_SCANCODE_A]) keys |= KEY_L;
    return keys;
}

static uint16_t pad_keys(void) {
    if (!s_pad) {
        return 0;
    }
    uint16_t keys = 0;
    /* Nintendo layout: east = A, south = B. */
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_B)) keys |= KEY_A;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_A)) keys |= KEY_B;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_BACK)) keys |= KEY_SELECT;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_START)) keys |= KEY_START;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) keys |= KEY_RIGHT;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) keys |= KEY_LEFT;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) keys |= KEY_UP;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) keys |= KEY_DOWN;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) keys |= KEY_R;
    if (SDL_GameControllerGetButton(s_pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) keys |= KEY_L;
    int lx = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTX);
    int ly = SDL_GameControllerGetAxis(s_pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (lx > 16000) keys |= KEY_RIGHT;
    if (lx < -16000) keys |= KEY_LEFT;
    if (ly > 16000) keys |= KEY_DOWN;
    if (ly < -16000) keys |= KEY_UP;
    return keys;
}

uint16_t pc_input_poll(void) {
    if (!s_headless) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                s_quit = true;
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_F11 ||
                    (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) {
                    bool fs = SDL_GetWindowFlags(s_win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                    SDL_SetWindowFullscreen(s_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (!s_pad) {
                    s_pad = SDL_GameControllerOpen(e.cdevice.which);
                }
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (s_pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s_pad))) {
                    SDL_GameControllerClose(s_pad);
                    s_pad = NULL;
                }
                break;
            default:
                break;
            }
        }
    }
    if (s_override) {
        return s_override(pc_frame_count());
    }
    if (s_headless) {
        return 0;
    }
    uint16_t keys = keyboard_keys() | pad_keys();
    /* The GBA d-pad cannot report opposite directions at once. */
    if ((keys & (KEY_LEFT | KEY_RIGHT)) == (KEY_LEFT | KEY_RIGHT)) keys &= (uint16_t)~(KEY_LEFT | KEY_RIGHT);
    if ((keys & (KEY_UP | KEY_DOWN)) == (KEY_UP | KEY_DOWN)) keys &= (uint16_t)~(KEY_UP | KEY_DOWN);
    return keys;
}

void pc_video_shutdown(void) {
    if (s_pad) SDL_GameControllerClose(s_pad);
    if (s_tex) SDL_DestroyTexture(s_tex);
    if (s_ren) SDL_DestroyRenderer(s_ren);
    if (s_win) SDL_DestroyWindow(s_win);
    s_pad = NULL;
    s_tex = NULL;
    s_ren = NULL;
    s_win = NULL;
    if (!s_headless) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    }
}
