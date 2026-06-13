#pragma once
#include <SDL.h>
#include "cfg.h"

// Show the config menu. Returns 1 = start game, 0 = quit.
// romdirs/nromdirs: list of candidate ROM directories to choose from.
// romdir_ok: 1 if corresponding dir has the required ROM files.
// sel_romdir: in/out — index of currently selected dir; updated on start.
int run_menu(SDL_Renderer *ren, wbml_cfg *cfg, const char *cfg_path,
             const char **romdirs, int nromdirs, const int *romdir_ok,
             int *sel_romdir);

// Draw an OSD message over the current frame. Call after SDL_RenderCopy.
// timer counts down from max_timer to 0; fades out in the last 30 frames.
void osd_draw(SDL_Renderer *r, const char *msg, int timer, int max_timer);
