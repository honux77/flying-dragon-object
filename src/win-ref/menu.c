// Config menu with embedded 8x8 bitmap font (public domain font8x8_basic)
// Hangul rendered from font_kr.h (Galmuri9, OFL-1.1) via UTF-8 decoding.
#include "menu.h"
#include "font_kr.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// 8x8 bitmap font — printable ASCII 0x20-0x7E (95 glyphs)
// Each byte is one row; LSB = leftmost pixel (font8x8_basic convention).
// Source: font8x8_basic (public domain)
// ---------------------------------------------------------------------------
static const uint8_t g_font[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x20 space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 0x21 !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x22 "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // 0x23 #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // 0x24 $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // 0x25 %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // 0x26 &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // 0x27 '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // 0x28 (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // 0x29 )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 0x2A *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // 0x2B +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // 0x2C ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // 0x2D -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // 0x2E .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // 0x2F /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0x30 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 0x31 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 0x32 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 0x33 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 0x34 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 0x35 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 0x36 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 0x37 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 0x38 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 0x39 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // 0x3A :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // 0x3B ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // 0x3C <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // 0x3D =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // 0x3E >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // 0x3F ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // 0x40 @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 0x41 A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 0x42 B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 0x43 C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 0x44 D
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 0x45 E
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 0x46 F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 0x47 G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 0x48 H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 0x49 I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 0x4A J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 0x4B K
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 0x4C L
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 0x4D M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 0x4E N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 0x4F O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 0x50 P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 0x51 Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 0x52 R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 0x53 S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 0x54 T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 0x55 U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 0x56 V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 0x57 W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 0x58 X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 0x59 Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 0x5A Z
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // 0x5B [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // 0x5C backslash
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // 0x5D ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // 0x5E ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 0x5F _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // 0x60 `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 0x61 a
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 0x62 b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 0x63 c
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, // 0x64 d
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // 0x65 e
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // 0x66 f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 0x67 g
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 0x68 h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 0x69 i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 0x6A j
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 0x6B k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 0x6C l
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 0x6D m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 0x6E n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 0x6F o
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 0x70 p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 0x71 q
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 0x72 r
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 0x73 s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 0x74 t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 0x75 u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 0x76 v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 0x77 w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 0x78 x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 0x79 y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 0x7A z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // 0x7B {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 0x7C |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // 0x7D }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x7E ~
};

// ---------------------------------------------------------------------------
// Rendering helpers (logical 256x224 coordinates)
// ---------------------------------------------------------------------------
typedef struct { uint8_t r, g, b; } Col;
static const Col WHITE  = {255,255,255};
static const Col YELLOW = {255,220,  0};
static const Col GRAY   = {128,128,128};
static const Col CYAN   = {  0,220,255};
static const Col RED    = {255, 80, 80};

// Global text alpha — osd_draw sets this for fade-out, everything else uses 255.
static Uint8 g_text_alpha = 255;

static void set_color(SDL_Renderer *r, Col c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, g_text_alpha);
}

static void draw_char(SDL_Renderer *r, int x, int y, unsigned char c, Col col) {
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *g = g_font[c - 0x20];
    set_color(r, col);
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col2 = 0; col2 < 8; col2++)
            if (bits & (1 << col2))
                SDL_RenderDrawPoint(r, x + col2, y + row);
    }
}

// Decode one UTF-8 sequence, advancing *ps. Invalid bytes yield '?'.
static int utf8_next(const char **ps) {
    const unsigned char *s = (const unsigned char *)*ps;
    if (s[0] < 0x80)                         { *ps += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && s[1])       { *ps += 2; return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((s[0] & 0xF0) == 0xE0 && s[1] && s[2]) {
        *ps += 3;
        return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    *ps += 1; return '?';
}

static int kfont_find(int cp) {
    int lo = 0, hi = KFONT_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (g_kfont_cp[mid] == (uint16_t)cp) return mid;
        if (g_kfont_cp[mid] < cp) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

// Hangul glyph: 10px advance, 12 rows. y is the ASCII 8x8 cell top;
// the Hangul cell starts 2px above so baselines line up.
static void draw_kglyph(SDL_Renderer *r, int x, int y, int idx, Col col) {
    set_color(r, col);
    for (int row = 0; row < KFONT_ROWS; row++) {
        uint16_t bits = g_kfont[idx][row];
        for (int b = 0; b < 10; b++)
            if (bits & (1 << b))
                SDL_RenderDrawPoint(r, x + b, y - 2 + row);
    }
}

static void draw_str(SDL_Renderer *r, int x, int y, const char *s, Col col) {
    while (*s) {
        int cp = utf8_next(&s);
        if (cp < 0x80) { draw_char(r, x, y, (unsigned char)cp, col); x += 8; continue; }
        int idx = kfont_find(cp);
        if (idx >= 0) { draw_kglyph(r, x, y, idx, col); x += 10; }
        else          { draw_char(r, x, y, '?', col);   x += 8;  }
    }
}

// Pixel width of a UTF-8 string as draw_str will render it.
static int str_pxwidth(const char *s) {
    int w = 0;
    while (*s) {
        int cp = utf8_next(&s);
        w += (cp < 0x80 || kfont_find(cp) < 0) ? 8 : 10;
    }
    return w;
}

// Draw text centred at cx (pixel coordinate)
static void draw_str_c(SDL_Renderer *r, int cx, int y, const char *s, Col col) {
    draw_str(r, cx - str_pxwidth(s) / 2, y, s, col);
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Col col) {
    SDL_Rect rc = {x, y, w, h};
    set_color(r, col);
    SDL_RenderFillRect(r, &rc);
}

// Character grid helpers: each cell is 8x8 px on the 256x224 logical surface
#define CX(col)  ((col) * 8)
#define CY(row)  ((row) * 8)
#define COLS 32
#define ROWS 28

// ---------------------------------------------------------------------------
// Menu state machine
// ---------------------------------------------------------------------------
typedef enum { MS_MAIN, MS_KEYS, MS_JOY, MS_CHEAT, MS_HELP } MenuScreen;

// Action names for keyboard settings
static const char *k_labels[] = {
    "왼쪽", "오른쪽", "위", "아래",
    "점프", "공격", "터보", "코인", "시작", "리셋"
};
static SDL_Scancode *k_fields(wbml_cfg *c, int i) {
    SDL_Scancode *arr[] = {
        &c->k_left, &c->k_right, &c->k_up, &c->k_down,
        &c->k_jump, &c->k_attack, &c->k_turbo,
        &c->k_coin, &c->k_start1, &c->k_reset
    };
    return arr[i];
}
#define NUM_KEYS 10

// Joystick names for display
static const char *joy_row_labels[] = {
    "장치", "점프 버튼", "공격 버튼", "터보 버튼",
    "왼쪽 버튼", "오른쪽 버튼", "위 버튼", "아래 버튼",
    "코인 버튼", "시작 버튼"
};
#define NUM_JOY_ROWS 10

// Open all connected joysticks for event detection
static SDL_Joystick *g_joys[16];
static int g_njoys = 0;

static void open_all_joysticks(void) {
    g_njoys = SDL_NumJoysticks();
    if (g_njoys > 16) g_njoys = 16;
    for (int i = 0; i < g_njoys; i++)
        g_joys[i] = SDL_JoystickOpen(i);
}

static void close_all_joysticks(void) {
    for (int i = 0; i < g_njoys; i++)
        if (g_joys[i]) { SDL_JoystickClose(g_joys[i]); g_joys[i] = NULL; }
    g_njoys = 0;
}

// Present the current frame
static void present(SDL_Renderer *r) { SDL_RenderPresent(r); }

// Draw a horizontal separator line
static void draw_sep(SDL_Renderer *r, int row) {
    set_color(r, GRAY);
    SDL_RenderDrawLine(r, 0, CY(row), 255, CY(row));
}

// Draw header + footer chrome shared by all screens
static void draw_chrome_hint(SDL_Renderer *r, const char *title, const char *hint) {
    SDL_SetRenderDrawColor(r, 8, 8, 48, 255);
    SDL_RenderClear(r);
    fill_rect(r, 0, 0, 256, 13, (Col){0,0,80});
    draw_str_c(r, 128, 2, title, YELLOW);
    draw_sep(r, 2);
    draw_sep(r, ROWS - 3);
    draw_str_c(r, 128, CY(ROWS-2), hint, GRAY);
}

static void draw_chrome(SDL_Renderer *r, const char *title) {
    draw_chrome_hint(r, title, "상하:선택  ENTER:확인  ESC:뒤로");
}

// ---------------------------------------------------------------------------
// MAIN MENU
// ---------------------------------------------------------------------------
static const char *main_items[] = {
    "키보드 설정",
    "조이스틱 설정",
    "난이도",
    "치트 설정",
    "도움말",
    "게임 시작",
    "종료"
};
#define MAIN_N         7
#define MAIN_IDX_DIFF  2
#define MAIN_IDX_CHEAT 3
#define MAIN_IDX_HELP  4
#define MAIN_IDX_START 5
#define MAIN_IDX_QUIT  6

static const char *diff_names[] = { "쉬움", "보통", "어려움" };
#define DIFF_N 3

static void draw_main(SDL_Renderer *r, int sel, int difficulty, unsigned cheat_flags, int rom_ok) {
    draw_chrome(r, "드래곤은 UFO다!!");
    if (!rom_ok)
        draw_str_c(r, 128, CY(3), "롬 파일이 없습니다", RED);
    int y0 = CY(4);
    for (int i = 0; i < MAIN_N; i++) {
        int y = y0 + i * 17;
        int is_sel = (i == sel);
        int disabled = (!rom_ok && i == MAIN_IDX_START);
        Col c = disabled ? GRAY : (is_sel ? YELLOW : WHITE);
        if (is_sel && !disabled) draw_str(r, CX(2), y, ">", YELLOW);
        draw_str(r, CX(4), y, main_items[i], c);
        if (i == MAIN_IDX_DIFF) {
            char buf[24];
            snprintf(buf, sizeof(buf), "< %s >", diff_names[difficulty]);
            draw_str(r, CX(16), y, buf, is_sel ? CYAN : GRAY);
        }
        if (i == MAIN_IDX_CHEAT) {
            draw_str(r, CX(20), y, cheat_flags ? "켜짐" : "꺼짐", cheat_flags ? CYAN : GRAY);
        }
    }
}

// ---------------------------------------------------------------------------
// CHEAT SETTINGS
// ---------------------------------------------------------------------------
static const struct { const char *label; unsigned flag; } cheat_items[] = {
    { "타이머 정지",   CHEAT_TIMER  },
    { "전설의 갑옷",   CHEAT_ARMOUR },
    { "전설의 방패",   CHEAT_SHIELD },
    { "전설의 검",     CHEAT_SWORD  },
    { "전설의 부츠",   CHEAT_BOOTS  },
    { "부자 게임",     CHEAT_COIN   },
};
#define CHEAT_N ((int)(sizeof(cheat_items)/sizeof(cheat_items[0])))

static void draw_cheat(SDL_Renderer *r, int sel, unsigned flags) {
    draw_chrome_hint(r, "치트 설정", "ENTER/SPACE:전환  ESC:뒤로");
    int y0 = CY(3);
    for (int i = 0; i < CHEAT_N; i++) {
        int y = y0 + i * 17;
        int is_sel = (i == sel);
        int on = (flags & cheat_items[i].flag) != 0;
        Col c = is_sel ? YELLOW : WHITE;
        if (is_sel) draw_str(r, CX(1), y, ">", YELLOW);
        draw_str(r, CX(3), y, on ? "[X]" : "[ ]", on ? CYAN : GRAY);
        draw_str(r, CX(7), y, cheat_items[i].label, c);
    }
    int y_back = y0 + CHEAT_N * 17;
    Col bc = (sel == CHEAT_N) ? YELLOW : WHITE;
    if (sel == CHEAT_N) draw_str(r, CX(1), y_back, ">", YELLOW);
    draw_str(r, CX(3), y_back, "뒤로", bc);
}

// ---------------------------------------------------------------------------
// KEYBOARD SETTINGS
// ---------------------------------------------------------------------------
static void draw_keys(SDL_Renderer *r, wbml_cfg *cfg, int sel, int rebinding) {
    draw_chrome(r, "키보드 설정");
    int y0 = CY(3);
    for (int i = 0; i < NUM_KEYS; i++) {
        int y = y0 + i * 17;
        int is_sel = (i == sel);
        Col lc = is_sel ? YELLOW : WHITE;
        Col vc = is_sel ? CYAN   : GRAY;
        if (is_sel) draw_str(r, CX(1), y, ">", YELLOW);
        draw_str(r, CX(2), y, k_labels[i], lc);
        const char *kn = SDL_GetScancodeName(*k_fields(cfg, i));
        if (rebinding && is_sel)
            draw_str(r, CX(13), y, "키를 누르세요...", RED);
        else
            draw_str(r, CX(13), y, kn, vc);
    }
    // BACK item
    int y_back = y0 + NUM_KEYS * 17;
    Col bc = (sel == NUM_KEYS) ? YELLOW : WHITE;
    if (sel == NUM_KEYS) draw_str(r, CX(1), y_back, ">", YELLOW);
    draw_str(r, CX(2), y_back, "뒤로", bc);
}

// ---------------------------------------------------------------------------
// JOYSTICK SETTINGS
// ---------------------------------------------------------------------------
static void fmt_btn(char *buf, size_t n, int v) {
    switch (v) {
    case -1: snprintf(buf, n, "---");        break;
    case -2: snprintf(buf, n, "햇 왼쪽");    break;
    case -3: snprintf(buf, n, "햇 오른쪽");  break;
    case -4: snprintf(buf, n, "햇 위");      break;
    case -5: snprintf(buf, n, "햇 아래");    break;
    default: snprintf(buf, n, "버튼 %d", v); break;
    }
}

static void draw_joy_row(SDL_Renderer *r, wbml_cfg *cfg, int i, int y,
                         int is_sel, int detecting) {
    Col lc = is_sel ? YELLOW : WHITE;
    Col vc = is_sel ? CYAN   : GRAY;
    if (is_sel) draw_str(r, CX(1), y, ">", YELLOW);
    draw_str(r, CX(2), y, joy_row_labels[i], lc);

    if (detecting && is_sel) {
        const char *msg = (i >= 4) ? "버튼/햇 입력" : "버튼을 누르세요";
        draw_str(r, CX(13), y, msg, RED);
        return;
    }

    char vbuf[32];
    switch (i) {
    case 0:
        if (cfg->joy_index < 0) {
            snprintf(vbuf, sizeof(vbuf), "사용 안 함");
        } else {
            const char *name = (cfg->joy_index < g_njoys && g_joys[cfg->joy_index])
                ? SDL_JoystickName(g_joys[cfg->joy_index]) : "?";
            snprintf(vbuf, sizeof(vbuf), "[%d] %.14s", cfg->joy_index, name);
        }
        break;
    case 1: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_jump);   break;
    case 2: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_attack); break;
    case 3: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_turbo);  break;
    case 4: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_left);   break;
    case 5: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_right);  break;
    case 6: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_up);     break;
    case 7: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_down);   break;
    case 8: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_coin);   break;
    case 9: fmt_btn(vbuf, sizeof(vbuf), cfg->joy_btn_start);  break;
    default: snprintf(vbuf, sizeof(vbuf), "?"); break;
    }
    draw_str(r, CX(13), y, vbuf, vc);
}

static void draw_joy(SDL_Renderer *r, wbml_cfg *cfg, int sel, int detecting) {
    draw_chrome(r, "조이스틱 설정");
    if (g_njoys == 0)
        draw_str_c(r, 128, CY(3), "조이스틱이 없습니다", RED);
    else {
        char info[40];
        snprintf(info, sizeof(info), "조이스틱 %d개 발견", g_njoys);
        draw_str_c(r, 128, CY(3), info, GRAY);
    }

    int y0 = CY(3);
    for (int i = 0; i < NUM_JOY_ROWS; i++)
        draw_joy_row(r, cfg, i, y0 + i * 15, i == sel, detecting);

    // BACK
    int y_back = y0 + NUM_JOY_ROWS * 15;
    Col bc = (sel == NUM_JOY_ROWS) ? YELLOW : WHITE;
    if (sel == NUM_JOY_ROWS) draw_str(r, CX(1), y_back, ">", YELLOW);
    draw_str(r, CX(2), y_back, "뒤로", bc);
}

// Returns a pointer to the cfg field for the given joy row (rows 1-7 only).
static int *joy_btn_field(wbml_cfg *cfg, int sel) {
    switch (sel) {
    case 1: return &cfg->joy_btn_jump;
    case 2: return &cfg->joy_btn_attack;
    case 3: return &cfg->joy_btn_turbo;
    case 4: return &cfg->joy_btn_left;
    case 5: return &cfg->joy_btn_right;
    case 6: return &cfg->joy_btn_up;
    case 7: return &cfg->joy_btn_down;
    case 8: return &cfg->joy_btn_coin;
    case 9: return &cfg->joy_btn_start;
    default: return NULL;
    }
}

// Hint for joystick rows
static void draw_joy_hint(SDL_Renderer *r, int sel, int detecting) {
    // Erase the generic chrome hint drawn by draw_chrome before painting ours.
    fill_rect(r, 0, CY(ROWS-2) - 2, 256, 12, (Col){8, 8, 48});
    const char *s;
    if (detecting)
        s = (sel >= 4) ? "버튼/햇 입력  DEL:해제  ESC:취소"
                       : "버튼 입력  DEL:해제  ESC:취소";
    else if (sel == 0)
        s = "좌우:장치 변경";
    else
        s = "ENTER:버튼 지정  DEL:해제";
    draw_str_c(r, 128, CY(ROWS-2), s, GRAY);
}

// ---------------------------------------------------------------------------
// OSD (on-screen display) overlay — call after SDL_RenderCopy of game texture
// ---------------------------------------------------------------------------
void osd_draw(SDL_Renderer *r, const char *msg, int timer, int max_timer) {
    (void)max_timer;
    if (timer <= 0 || !msg || !msg[0]) return;
    // Fade out in the last 30 frames
    int alpha = (timer < 30) ? (timer * 255 / 30) : 255;

    int text_w = str_pxwidth(msg);
    int box_w  = text_w + 8;
    int box_x  = (256 - box_w) / 2;
    int box_y  = 224 - 20;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(alpha * 3 / 4));
    SDL_Rect bg = {box_x, box_y, box_w, 16};
    SDL_RenderFillRect(r, &bg);

    g_text_alpha = (Uint8)alpha;
    draw_str(r, box_x + 4, box_y + 4, msg, (Col){255, 220, 0});
    g_text_alpha = 255;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ---------------------------------------------------------------------------
// HELP screen
// ---------------------------------------------------------------------------
static void draw_help(SDL_Renderer *r, const wbml_cfg *cfg) {
    (void)cfg;
    draw_chrome_hint(r, "도움말", "ESC:뒤로");

    int y = CY(3);
    const int S = 15;  // row step

    char buf[32];
#define HELP_ROW(label, val) \
    draw_str(r, CX(2), y, (label), WHITE); \
    draw_str(r, CX(16), y, (val), CYAN); y += S;

    snprintf(buf, sizeof(buf), "%s", SDL_GetScancodeName(cfg->k_reset));
    HELP_ROW("리셋",           buf);
    HELP_ROW("일시정지",       "P");
    HELP_ROW("상태 저장",      "F6");
    HELP_ROW("상태 불러오기",  "F7");
    HELP_ROW("이전 슬롯",      "F8");
    HELP_ROW("다음 슬롯",      "F9");
    HELP_ROW("스크린샷",       "F12");
    HELP_ROW("종료",           "ESC");
#undef HELP_ROW
}

// ---------------------------------------------------------------------------
// Main menu entry point
// ---------------------------------------------------------------------------
int run_menu(SDL_Renderer *ren, wbml_cfg *cfg, const char *cfg_path, int rom_ok) {
    open_all_joysticks();

    MenuScreen screen = MS_MAIN;
    int sel = 0;
    int result = 0;  // 0=quit, 1=start
    int rebinding = 0;
    int detecting = 0;
    int running = 1;

    while (running) {
        // Render
        switch (screen) {
        case MS_MAIN:  draw_main(ren, sel, cfg->difficulty, cfg->cheat_flags, rom_ok); break;
        case MS_KEYS: draw_keys(ren, cfg, sel, rebinding); break;
        case MS_JOY:   draw_joy (ren, cfg, sel, detecting);
                       draw_joy_hint(ren, sel, detecting);
                       break;
        case MS_CHEAT: draw_cheat(ren, sel, cfg->cheat_flags); break;
        case MS_HELP:  draw_help(ren, cfg); break;
        }
        present(ren);

        // Handle events
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; result = 0; break; }

            // --- Keyboard rebind mode ---
            if (rebinding && screen == MS_KEYS && e.type == SDL_KEYDOWN) {
                SDL_Scancode sc = e.key.keysym.scancode;
                if (sc == SDL_SCANCODE_ESCAPE) {
                    // cancel
                } else if (sc != SDL_SCANCODE_RETURN) {
                    *k_fields(cfg, sel) = sc;
                    cfg_save(cfg, cfg_path);
                }
                rebinding = 0;
                continue;
            }

            // --- Joystick detect mode ---
            if (detecting && screen == MS_JOY) {
                if (e.type == SDL_KEYDOWN) {
                    SDL_Scancode sc = e.key.keysym.scancode;
                    if (sc == SDL_SCANCODE_ESCAPE) { detecting = 0; continue; }
                    if (sc == SDL_SCANCODE_DELETE || sc == SDL_SCANCODE_BACKSPACE) {
                        int *f = joy_btn_field(cfg, sel);
                        if (f) { *f = -1; cfg_save(cfg, cfg_path); }
                        detecting = 0; continue;
                    }
                }
                if (e.type == SDL_JOYBUTTONDOWN) {
                    int *f = joy_btn_field(cfg, sel);
                    if (f) { *f = e.jbutton.button; cfg_save(cfg, cfg_path); }
                    detecting = 0; continue;
                }
                if (sel >= 4 && e.type == SDL_JOYHATMOTION &&
                    e.jhat.value != SDL_HAT_CENTERED) {
                    int enc = -1;
                    if      (e.jhat.value & SDL_HAT_LEFT)  enc = -2;
                    else if (e.jhat.value & SDL_HAT_RIGHT) enc = -3;
                    else if (e.jhat.value & SDL_HAT_UP)    enc = -4;
                    else if (e.jhat.value & SDL_HAT_DOWN)  enc = -5;
                    if (enc != -1) {
                        int *f = joy_btn_field(cfg, sel);
                        if (f) { *f = enc; cfg_save(cfg, cfg_path); }
                        detecting = 0; continue;
                    }
                }
                continue; // swallow other events while detecting
            }

            // --- Normal navigation ---
            if (e.type != SDL_KEYDOWN) continue;
            SDL_Scancode sc = e.key.keysym.scancode;

            int max_sel = (screen == MS_MAIN)  ? MAIN_N - 1
                        : (screen == MS_KEYS)  ? NUM_KEYS
                        : (screen == MS_CHEAT) ? CHEAT_N
                        : (screen == MS_HELP)  ? 0
                        :                        NUM_JOY_ROWS;

            if (sc == SDL_SCANCODE_UP)   { sel = (sel > 0) ? sel - 1 : max_sel; continue; }
            if (sc == SDL_SCANCODE_DOWN) { sel = (sel < max_sel) ? sel + 1 : 0; continue; }

            if (sc == SDL_SCANCODE_ESCAPE) {
                if (screen == MS_CHEAT) { screen = MS_MAIN; sel = MAIN_IDX_CHEAT; }
                else if (screen == MS_HELP)  { screen = MS_MAIN; sel = MAIN_IDX_HELP; }
                else if (screen != MS_MAIN)  { screen = MS_MAIN; sel = 0; }
                continue;
            }

            // Difficulty row: L/R to change
            if (screen == MS_MAIN && sel == MAIN_IDX_DIFF) {
                if (sc == SDL_SCANCODE_LEFT) {
                    cfg->difficulty = (cfg->difficulty + DIFF_N - 1) % DIFF_N;
                    cfg_save(cfg, cfg_path);
                    continue;
                }
                if (sc == SDL_SCANCODE_RIGHT) {
                    cfg->difficulty = (cfg->difficulty + 1) % DIFF_N;
                    cfg_save(cfg, cfg_path);
                    continue;
                }
            }

            if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER ||
                sc == SDL_SCANCODE_SPACE) {
                if (screen == MS_MAIN) {
                    switch (sel) {
                    case 0:              screen = MS_KEYS;  sel = 0; break;
                    case 1:              screen = MS_JOY;   sel = 0; break;
                    case MAIN_IDX_CHEAT: screen = MS_CHEAT; sel = 0; break;
                    case MAIN_IDX_HELP:  screen = MS_HELP;  sel = 0; break;
                    case MAIN_IDX_DIFF:
                        cfg->difficulty = (cfg->difficulty + 1) % DIFF_N;
                        cfg_save(cfg, cfg_path);
                        break;
                    case MAIN_IDX_START: if (rom_ok) { running = 0; result = 1; } break;
                    case MAIN_IDX_QUIT:  running = 0; result = 0; break;
                    }
                } else if (screen == MS_KEYS) {
                    if (sel == NUM_KEYS) { screen = MS_MAIN; sel = 0; }
                    else                 { rebinding = 1; }
                } else if (screen == MS_JOY) {
                    if (sel == NUM_JOY_ROWS) { screen = MS_MAIN; sel = 0; }
                    else if (sel == 0) { /* handled by L/R below */ }
                    else               { detecting = 1; }
                } else if (screen == MS_HELP) {
                    screen = MS_MAIN; sel = MAIN_IDX_HELP;
                } else if (screen == MS_CHEAT) {
                    if (sel == CHEAT_N) { screen = MS_MAIN; sel = MAIN_IDX_CHEAT; }
                    else {
                        cfg->cheat_flags ^= cheat_items[sel].flag;
                        cfg_save(cfg, cfg_path);
                    }
                }
                continue;
            }

            // Joystick device selection with left/right
            if (screen == MS_JOY && sel == 0) {
                if (sc == SDL_SCANCODE_LEFT) {
                    cfg->joy_index--;
                    if (cfg->joy_index < -1) cfg->joy_index = g_njoys - 1;
                    cfg_save(cfg, cfg_path);
                }
                if (sc == SDL_SCANCODE_RIGHT) {
                    cfg->joy_index++;
                    if (cfg->joy_index >= g_njoys) cfg->joy_index = -1;
                    cfg_save(cfg, cfg_path);
                }
            }
        }
    }

    close_all_joysticks();
    return result;
}
