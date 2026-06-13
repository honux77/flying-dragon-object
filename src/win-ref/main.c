// Windows reference host for wbml: SDL2 window, config menu, input, 60Hz loop.
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "machine.h"
#include "video.h"
#include "cfg.h"
#include "menu.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

// ---------------------------------------------------------------------------
// ROM directory scanning
// ---------------------------------------------------------------------------
#define ROMDIR_LEN  512
#define MAX_ROMDIRS 10

// A directory is valid if it contains the sound ROM, which is common to all sets.
static int romdir_is_valid(const char *dir) {
    char path[ROMDIR_LEN];
    snprintf(path, sizeof(path), "%s/epr-11037.126", dir);
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static int scan_romdirs(const char *base, char romdirs[][ROMDIR_LEN], int romdir_ok[], int max) {
    int n = 0;

    // Include the base dir only if it actually contains ROM files.
    if (romdir_is_valid(base)) {
        SDL_strlcpy(romdirs[n], base, ROMDIR_LEN);
        romdir_ok[n] = 1;
        n++;
    }

#ifdef _WIN32
    char pattern[ROMDIR_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*", base);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            if (n >= max) break;
            snprintf(romdirs[n], ROMDIR_LEN, "%s/%s", base, fd.cFileName);
            romdir_ok[n] = romdir_is_valid(romdirs[n]);
            n++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(base);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && n < max) {
            if (ent->d_name[0] == '.') continue;
            char path[ROMDIR_LEN];
            snprintf(path, sizeof(path), "%s/%s", base, ent->d_name);
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            SDL_strlcpy(romdirs[n], path, ROMDIR_LEN);
            romdir_ok[n] = romdir_is_valid(path);
            n++;
        }
        closedir(d);
    }
#endif
    // Fallback: if nothing found at all, show the base dir anyway.
    if (n == 0) {
        SDL_strlcpy(romdirs[0], base, ROMDIR_LEN);
        romdir_ok[0] = 0;
        n = 1;
    }
    return n;
}

// Korean dialog overlay (krtext.c): repaints Hangul tile text with the
// Galmuri9 font.  Active only when the Korean-patched ROM set is loaded.
void krtext_overlay(uint32_t *disp, const system2 *m);

// Read a direction-capable input: >= 0 = button, -2...-5 = hat (L/R/U/D), -1 = off.
static int joy_dir(SDL_Joystick *joy, int v) {
    if (v == -1) return 0;
    if (v >= 0)  return SDL_JoystickGetButton(joy, v);
    Uint8 hat = SDL_JoystickGetHat(joy, 0);
    switch (v) {
    case -2: return (hat & SDL_HAT_LEFT)  != 0;
    case -3: return (hat & SDL_HAT_RIGHT) != 0;
    case -4: return (hat & SDL_HAT_UP)    != 0;
    case -5: return (hat & SDL_HAT_DOWN)  != 0;
    default: return 0;
    }
}

static void poll_input(system2 *m, const wbml_cfg *cfg, SDL_Joystick *joy) {
    static unsigned turbo_tick = 0;
    turbo_tick++;

    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint8_t p1 = 0, sys = 0;

    int turbo_active = k[cfg->k_turbo] ||
                       (joy && cfg->joy_btn_turbo >= 0 &&
                        SDL_JoystickGetButton(joy, cfg->joy_btn_turbo));

    if (!turbo_active) {
        if (k[cfg->k_left])  p1 |= 0x80;
        if (k[cfg->k_right]) p1 |= 0x40;
        if (k[cfg->k_up])    p1 |= 0x20;
        if (k[cfg->k_down])  p1 |= 0x10;
    }
    if (k[cfg->k_jump])   p1 |= 0x02;
    if (k[cfg->k_attack]) p1 |= 0x04;
    if (k[cfg->k_coin])   sys |= 0x01;
    if (k[cfg->k_start1]) sys |= 0x10;

    if (joy) {
        if (!turbo_active) {
            if (joy_dir(joy, cfg->joy_btn_left))  p1 |= 0x80;
            if (joy_dir(joy, cfg->joy_btn_right)) p1 |= 0x40;
            if (joy_dir(joy, cfg->joy_btn_up))    p1 |= 0x20;
            if (joy_dir(joy, cfg->joy_btn_down))  p1 |= 0x10;
        }
        if (cfg->joy_btn_jump   >= 0 && SDL_JoystickGetButton(joy, cfg->joy_btn_jump))   p1 |= 0x02;
        if (cfg->joy_btn_attack >= 0 && SDL_JoystickGetButton(joy, cfg->joy_btn_attack)) p1 |= 0x04;
        if (cfg->joy_btn_coin  >= 0 && SDL_JoystickGetButton(joy, cfg->joy_btn_coin))  sys |= 0x01;
        if (cfg->joy_btn_start >= 0 && SDL_JoystickGetButton(joy, cfg->joy_btn_start)) sys |= 0x10;
    }

    if (turbo_active) {
        if ((turbo_tick >> 1) & 1) p1 |= 0x80; else p1 |= 0x40;
    }

    m->in.p1  = p1;
    m->in.system = sys;
}

int main(int argc, char **argv) {
    const char *rombase = (argc > 1) ? argv[1] : "roms";

    // Scan for ROM directories (base dir + subdirectories).
    static char romdir_store[MAX_ROMDIRS][ROMDIR_LEN];
    static int romdir_ok_arr[MAX_ROMDIRS];
    int nromdirs = scan_romdirs(rombase, romdir_store, romdir_ok_arr, MAX_ROMDIRS);

    // Build pointer array for run_menu (2D array != char**, must use pointers).
    const char *romdirs[MAX_ROMDIRS];
    for (int i = 0; i < nromdirs; i++) romdirs[i] = romdir_store[i];

    // Default to first valid dir; fall back to index 0.
    int sel_romdir = 0;
    for (int i = 0; i < nromdirs; i++) {
        if (romdir_ok_arr[i]) { sel_romdir = i; break; }
    }

    // optional sound-register logging for diagnostics: --snlog or SN_LOG=1
    {
        extern int g_sn_log; extern FILE *g_sn_logfile;
        int want_log = (SDL_getenv("SN_LOG") != NULL);
        for (int i = 1; i < argc; i++) if (SDL_strcmp(argv[i], "--snlog") == 0) want_log = 1;
        if (want_log) {
            g_sn_logfile = fopen("build/snlog_live.txt", "w");
            if (g_sn_logfile) { g_sn_log = 1; printf("SN write log -> build/snlog_live.txt\n"); }
        }
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // Load config (SDL_GetPrefPath gives a writable per-user directory)
    static char cfg_path[512];
    {
        char *pref = SDL_GetPrefPath("wbml", "wbml");
        if (pref) {
            snprintf(cfg_path, sizeof(cfg_path), "%swbml.cfg", pref);
            SDL_free(pref);
        } else {
            SDL_strlcpy(cfg_path, "wbml.cfg", sizeof(cfg_path));
        }
    }
    wbml_cfg cfg;
    cfg_defaults(&cfg);
    cfg_load(&cfg, cfg_path);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    SDL_AudioDeviceID adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (adev) SDL_PauseAudioDevice(adev, 0);

    const int DISP_W = SCREEN_W / 2;  // 256
    const int DISP_H = SCREEN_H;      // 224

    int scale = 3;
    int win_x = SDL_WINDOWPOS_CENTERED, win_y = SDL_WINDOWPOS_CENTERED;
    {
        FILE *wf = fopen("build/winpos.txt", "r");
        if (wf) { fscanf(wf, "%d %d", &win_x, &win_y); fclose(wf); }
    }
    SDL_Window *win = SDL_CreateWindow(
        "Flying Dragon Object",
        win_x, win_y,
        DISP_W * scale, DISP_H * scale, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, DISP_W, DISP_H);
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);
    SDL_Texture *tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        DISP_W, DISP_H);

    // Show config menu before game
    int headless_frames = 0;
    for (int i = 1; i < argc; i++)
        if (SDL_strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            headless_frames = atoi(argv[i + 1]);

    if (!headless_frames) {
        if (!run_menu(ren, &cfg, cfg_path,
                      (const char **)romdirs, nromdirs, romdir_ok_arr, &sel_romdir)) {
            SDL_DestroyTexture(tex);
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 0;
        }
    }

    // Initialize machine with the selected ROM directory.
    static system2 m;
    int rom_ok = !machine_init(&m, romdir_store[sel_romdir]);
    if (!rom_ok)
        fprintf(stderr, "ROM load failed from '%s/'\n", romdir_store[sel_romdir]);

    // Korean-patched tile ROM? (signature: redrawn ㄱ jamo at tile 0x800)
    static const uint8_t kr_sig[8] = {0x7c,0x06,0x06,0x06,0x06,0x00,0x00,0x00};
    int kr_mode = rom_ok && memcmp(m.tilerom + 0x4000, kr_sig, 8) == 0;
    if (kr_mode) printf("Korean patch detected: Hangul overlay enabled\n");

    // Apply difficulty (DIP switches + easy-mode state)
    machine_set_difficulty(&m, cfg.difficulty);

    // Open joystick if configured
    SDL_Joystick *joy = NULL;
    if (cfg.joy_index >= 0 && cfg.joy_index < SDL_NumJoysticks())
        joy = SDL_JoystickOpen(cfg.joy_index);

    static uint32_t fb[SCREEN_W * SCREEN_H];
    static uint32_t disp[256 * 224];
    int running = 1;
    int paused = 0;
#define NUM_SLOTS 5
    static savestate save_slots[NUM_SLOTS];
    for (int i = 0; i < NUM_SLOTS; i++) save_slots[i].valid = 0;
    int cur_slot = 0;
    char osd_msg[64] = "";
    int  osd_timer   = 0;
#define OSD_SHOW(s) do { SDL_strlcpy(osd_msg, (s), sizeof(osd_msg)); osd_timer = 150; } while(0)


    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 prev = SDL_GetPerformanceCounter();
    const double frame_s = 1.0 / 60.06;
    long frame = 0;

    printf("Wonder Boy: Monster Land (reference)\n");
    printf("  Z=jump  X=attack  C=turbo-LR\n");
    printf("  5=coin  1=start   P=pause  F8/F9=slot  F6=save  F7=load  F12=screenshot  ESC=quit\n");
    fflush(stdout);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_p) {
                paused = !paused;
                OSD_SHOW(paused ? "일시정지" : "재개");
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F8) {
                cur_slot = (cur_slot + NUM_SLOTS - 1) % NUM_SLOTS;
                snprintf(osd_msg, sizeof(osd_msg), "슬롯 %d %s", cur_slot + 1,
                         save_slots[cur_slot].valid ? "(저장됨)" : "(비어있음)");
                osd_timer = 150;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F9) {
                cur_slot = (cur_slot + 1) % NUM_SLOTS;
                snprintf(osd_msg, sizeof(osd_msg), "슬롯 %d %s", cur_slot + 1,
                         save_slots[cur_slot].valid ? "(저장됨)" : "(비어있음)");
                osd_timer = 150;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F6) {
                machine_save(&m, &save_slots[cur_slot]);
                snprintf(osd_msg, sizeof(osd_msg), "슬롯 %d 저장됨", cur_slot + 1);
                osd_timer = 150;
                printf("[save] slot %d saved at frame %ld\n", cur_slot + 1, frame);
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F7) {
                if (save_slots[cur_slot].valid) {
                    machine_load(&m, &save_slots[cur_slot]);
                    snprintf(osd_msg, sizeof(osd_msg), "슬롯 %d 불러옴", cur_slot + 1);
                    osd_timer = 150;
                    printf("[load] slot %d restored\n", cur_slot + 1);
                } else {
                    snprintf(osd_msg, sizeof(osd_msg), "슬롯 %d 비어있음", cur_slot + 1);
                    osd_timer = 150;
                }
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == cfg.k_reset) {
                machine_reset(&m);
                machine_set_difficulty(&m, cfg.difficulty);
                frame = 0;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12) {
                char name[64];
                snprintf(name, sizeof(name), "build/shot_%ld.bmp", frame);
                SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(
                    disp, DISP_W, DISP_H, 32, DISP_W * 4,
                    0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                if (surf) {
                    SDL_SaveBMP(surf, name);
                    SDL_FreeSurface(surf);
                    OSD_SHOW("스크린샷 저장됨");
                    printf("screenshot: %s\n", name);
                }
            }
            // RAM snapshot for address hunting: press 0 to dump C200-C2FF
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_0) {
                char name[64];
                snprintf(name, sizeof(name), "build/ram_%ld.bin", frame);
                FILE *f = fopen(name, "wb");
                if (f) { fwrite(m.ram, 1, sizeof(m.ram), f); fclose(f); }
                printf("[ram] frame=%ld", frame);
                // C000-C0FF: system/timer area
                for (int i = 0; i < 0x100; i++) {
                    if (i % 16 == 0) printf("\n  C%03X:", 0x000 + i);
                    printf(" %02X", m.ram[0x000 + i]);
                }
                // C200-C2FF: player state area
                for (int i = 0; i < 0x100; i++) {
                    if (i % 16 == 0) printf("\n  C%03X:", 0x200 + i);
                    printf(" %02X", m.ram[0x200 + i]);
                }
                printf("\n");
                fflush(stdout);
            }
        }

        if (!paused) {
            poll_input(&m, &cfg, joy);
            static int16_t audio[AUDIO_MAX_FRAME];
            int nsamp = machine_run_frame(&m, fb, audio);
            machine_cheat_tick(&m, cfg.cheat_flags);
            if (adev) {
                if (SDL_GetQueuedAudioSize(adev) < (Uint32)(AUDIO_SAMPLE_RATE / 4 * sizeof(int16_t)))
                    SDL_QueueAudio(adev, audio, nsamp * sizeof(int16_t));
            }
            frame++;

            for (int y = 0; y < DISP_H; y++) {
                const uint32_t *src = &fb[y * SCREEN_W];
                uint32_t *dst = &disp[y * DISP_W];
                for (int x = 0; x < DISP_W; x++) dst[x] = src[x * 2];
            }
            if (kr_mode) krtext_overlay(disp, &m);
            SDL_UpdateTexture(tex, NULL, disp, DISP_W * sizeof(uint32_t));
        }

        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        if (osd_timer > 0) { osd_draw(ren, osd_msg, osd_timer, 150); osd_timer--; }
        SDL_RenderPresent(ren);
        if (paused) { SDL_Delay(16); continue; }

        if (headless_frames && frame >= headless_frames) {
            FILE *f = fopen("build/headless.ppm", "wb");
            if (f) {
                fprintf(f, "P6\n%d %d\n255\n", DISP_W, DISP_H);
                for (int p = 0; p < DISP_W * DISP_H; p++) {
                    uint32_t c = disp[p];
                    fputc((c >> 16) & 0xff, f);
                    fputc((c >>  8) & 0xff, f);
                    fputc( c        & 0xff, f);
                }
                fclose(f);
                printf("headless: wrote build/headless.ppm after %ld frames\n", frame);
            }
            f = fopen("build/vram.bin", "wb");
            if (f) {
                fwrite(m.videoram, 1, VIDEORAM_SIZE, f);
                fclose(f);
                printf("headless: wrote build/vram.bin\n");
            }
            running = 0;
        }

        if (headless_frames) continue;
        double target_ticks = frame_s * (double)freq;
        for (;;) {
            Uint64 now = SDL_GetPerformanceCounter();
            double elapsed = (double)(now - prev);
            if (elapsed >= target_ticks) { prev = now; break; }
            double remain_ms = (target_ticks - elapsed) * 1000.0 / (double)freq;
            if (remain_ms > 2.0) SDL_Delay((Uint32)(remain_ms - 1.0));
        }
    }

    if (joy) SDL_JoystickClose(joy);
    {
        int wx, wy;
        SDL_GetWindowPosition(win, &wx, &wy);
        FILE *wf = fopen("build/winpos.txt", "w");
        if (wf) { fprintf(wf, "%d %d\n", wx, wy); fclose(wf); }
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
