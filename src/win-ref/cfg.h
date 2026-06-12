#pragma once
#include <SDL.h>

// difficulty: 0=EASY  1=NORMAL  2=HARD
typedef struct {
    SDL_Scancode k_left, k_right, k_up, k_down;
    SDL_Scancode k_jump, k_attack, k_turbo;
    SDL_Scancode k_coin, k_start1, k_reset;
    int joy_index;      // -1 = disabled
    int joy_btn_jump, joy_btn_attack, joy_btn_turbo;
    int joy_btn_left, joy_btn_right, joy_btn_up, joy_btn_down;
    int joy_btn_coin, joy_btn_start;
    int difficulty;
    unsigned cheat_flags;
} wbml_cfg;

// cheat_flags bitmask
#define CHEAT_TIMER  (1u << 0)  // freeze game timer (no hourglass)
#define CHEAT_ARMOUR (1u << 1)  // force Legend Armour
#define CHEAT_SHIELD (1u << 2)  // force Legend Shield
#define CHEAT_SWORD  (1u << 3)  // force Sword of Legend
#define CHEAT_BOOTS  (1u << 4)  // force Legend Boots
#define CHEAT_COIN   (1u << 5)  // coin pickup minimum 15~66

void cfg_defaults(wbml_cfg *c);
int  cfg_load(wbml_cfg *c, const char *path);
void cfg_save(const wbml_cfg *c, const char *path);
