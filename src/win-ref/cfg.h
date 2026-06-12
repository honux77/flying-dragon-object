#pragma once
#include <SDL.h>

// difficulty: 0=EASY  1=NORMAL  2=HARD  3=CHEAT
typedef struct {
    SDL_Scancode k_left, k_right, k_up, k_down;
    SDL_Scancode k_jump, k_attack, k_turbo;
    SDL_Scancode k_coin, k_start1, k_reset;
    int joy_index;      // -1 = disabled
    int joy_btn_jump, joy_btn_attack, joy_btn_turbo;
    int joy_btn_left, joy_btn_right, joy_btn_up, joy_btn_down;
    int difficulty;
} wbml_cfg;

void cfg_defaults(wbml_cfg *c);
int  cfg_load(wbml_cfg *c, const char *path);
void cfg_save(const wbml_cfg *c, const char *path);
