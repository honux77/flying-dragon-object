// Korean in-game text overlay: when running the Korean-patched ROM set,
// dialog text is drawn by the game as 8x8 tiles (codes 0x800-0x8FF on the
// fixed fg layer).  Rare syllables are split across two cells (cho-jamo
// tile + body tile), which hurts readability.  Since we own the renderer,
// we re-draw those cells with the Galmuri9 10x12 Hangul font instead:
// scan the fg tilemap, decode runs of Korean tiles, paint over them.
#include "krtext.h"
#include "font_kr.h"
#include "machine.h"

#define DISP_W 256
#define DISP_H 224

// Background and glyph colors for the repainted text cells.
#define KR_BG 0xFF000000u
#define KR_FG 0xFFFFFFFFu

static int kfont_idx(int cp) {
    int lo = 0, hi = KFONT_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (g_kfont_cp[mid] == (uint16_t)cp) return mid;
        if (g_kfont_cp[mid] < cp) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

// Draw one 10x12 glyph centred in a cell span starting at (px, py) of
// width w_px.  py is the 8px text row top; the glyph cell extends 2px
// above and below it.
static void draw_glyph(uint32_t *disp, int px, int py, int w_px, int cp) {
    int idx = kfont_idx(cp);
    if (idx < 0) return;
    int x0 = px + (w_px - 10) / 2;
    for (int row = 0; row < KFONT_ROWS; row++) {
        int y = py - 2 + row;
        if (y < 0 || y >= DISP_H) continue;
        uint16_t bits = g_kfont[idx][row];
        for (int b = 0; b < 10; b++) {
            int x = x0 + b;
            if (x < 0 || x >= DISP_W) continue;
            if (bits & (1 << b))
                disp[y * DISP_W + x] = KR_FG;
        }
    }
}

static void fill_cells(uint32_t *disp, int tx, int ty, int ncells) {
    for (int y = ty * 8 - 2; y < ty * 8 + 10; y++) {
        if (y < 0 || y >= DISP_H) continue;
        for (int x = tx * 8; x < (tx + ncells) * 8 && x < DISP_W; x++)
            disp[y * DISP_W + x] = KR_BG;
    }
}

#define KR_CHO   "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
#define NJUNG 21

void krtext_overlay(uint32_t *disp, const system2 *m) {
    if (m->flip) return;
    const uint8_t *vr = m->videoram;  // page 0 = fixed fg layer

    for (int ty = 0; ty < 28; ty++) {
        int tx = 0;
        while (tx < 32) {
            int idx = ty * 32 + tx;
            uint16_t td = vr[idx * 2] | (vr[idx * 2 + 1] << 8);
            uint16_t code = ((td >> 4) & 0x800) | (td & 0x7ff);
            // text font occupies tile codes 0x800-0x8FF only
            if ((code & 0xf00) != 0x800) { tx++; continue; }
            int b = code & 0xff;
            int t = kr_tile_type[b];

            if (t == KRT_CHO && tx + 1 < 32) {
                // cho jamo + body = one syllable over two cells
                int idx2 = ty * 32 + tx + 1;
                uint16_t td2 = vr[idx2 * 2] | (vr[idx2 * 2 + 1] << 8);
                uint16_t code2 = ((td2 >> 4) & 0x800) | (td2 & 0x7ff);
                if ((code2 & 0xf00) == 0x800 &&
                    kr_tile_type[code2 & 0xff] == KRT_BODY) {
                    int cho = kr_tile_val[b];
                    int body = kr_tile_val[code2 & 0xff];
                    int cp = 0xAC00 + cho * NJUNG * 28 + body;
                    fill_cells(disp, tx, ty, 2);
                    draw_glyph(disp, tx * 8, ty * 8, 16, cp);
                    tx += 2;
                    continue;
                }
            }
            if (t == KRT_SYLL) {
                int cp = kr_tile_val[b];
                // syllable + vowel-extension body (e.g. 무+ㅓ = 뭐):
                // merge when the pair forms a valid compound vowel
                int span = 1;
                if (tx + 1 < 32) {
                    int idx2 = ty * 32 + tx + 1;
                    uint16_t td2 = vr[idx2 * 2] | (vr[idx2 * 2 + 1] << 8);
                    uint16_t code2 = ((td2 >> 4) & 0x800) | (td2 & 0x7ff);
                    if ((code2 & 0xf00) == 0x800 &&
                        kr_tile_type[code2 & 0xff] == KRT_BODY) {
                        int body = kr_tile_val[code2 & 0xff];
                        int ext_jung = body / 28, ext_jong = body % 28;
                        int s = cp - 0xAC00;
                        int jung = (s % (NJUNG * 28)) / 28;
                        int jong = s % 28;
                        // merge table: (base jung, ext jung) -> compound
                        // ㅗ(8)+ㅏ(0)=ㅘ(9)  ㅗ+ㅣ(20)=ㅚ(11)  ㅗ+ㅐ(1)=ㅙ(10)
                        // ㅜ(13)+ㅓ(4)=ㅝ(14) ㅜ+ㅣ=ㅟ(16) ㅜ+ㅔ(5)=ㅞ(15)
                        // ㅡ(18)+ㅣ=ㅢ(19)
                        int merged = -1;
                        if (jong == 0 && ext_jong == 0) {
                            if (jung == 8 && ext_jung == 0)   merged = 9;
                            if (jung == 8 && ext_jung == 20)  merged = 11;
                            if (jung == 8 && ext_jung == 1)   merged = 10;
                            if (jung == 13 && ext_jung == 4)  merged = 14;
                            if (jung == 13 && ext_jung == 20) merged = 16;
                            if (jung == 13 && ext_jung == 5)  merged = 15;
                            if (jung == 18 && ext_jung == 20) merged = 19;
                        }
                        if (merged >= 0) {
                            int cho = s / (NJUNG * 28);
                            cp = 0xAC00 + (cho * NJUNG + merged) * 28;
                            span = 2;
                        }
                    }
                }
                fill_cells(disp, tx, ty, span);
                draw_glyph(disp, tx * 8, ty * 8, span * 8, cp);
                tx += span;
                continue;
            }
            tx++;
        }
    }
}
