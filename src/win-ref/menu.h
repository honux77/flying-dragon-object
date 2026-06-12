#pragma once
#include <SDL.h>
#include "cfg.h"

// Show the config menu. Returns 1 = start game, 0 = quit.
// rom_ok: 0 = ROM files missing, START GAME is disabled.
int run_menu(SDL_Renderer *ren, wbml_cfg *cfg, const char *cfg_path, int rom_ok);

// Draw an OSD message over the current frame. Call after SDL_RenderCopy.
// timer counts down from max_timer to 0; fades out in the last 30 frames.
void osd_draw(SDL_Renderer *r, const char *msg, int timer, int max_timer);
