#!/usr/bin/env python3
"""Render the Korean-patched dialog strings as labeled tile sheets.

Reads roms-kr/ (built by mkbootleg.py) and writes build/krmap/page_N.png:
each dialog string rendered with the patched 8x8 tiles, with the string
byte shown in hex under every cell.  Used to transcribe the
tile-code -> Hangul-syllable mapping for emulator-side font rendering.

String encoding (reverse-engineered):
  - strings live in the m-6.bin data view, 0x8F1B-0x9EA5, 0xFF-terminated
  - first ~3 bytes of each chunk are window control codes
  - byte b -> fg tile 0x800+b
  - common syllables: one byte (precomposed tile)
  - rare syllables: two bytes = cho-seong tile (0x00-0x0D = TILE jamo) +
    body tile (jung-seong+jong-seong), occupying two adjacent cells
"""
import struct
import zlib
from pathlib import Path

HEX = {
    '0': '111 101 101 101 111', '1': '010 110 010 010 111',
    '2': '111 001 111 100 111', '3': '111 001 111 001 111',
    '4': '101 101 111 001 001', '5': '111 100 111 001 111',
    '6': '111 100 111 101 111', '7': '111 001 001 010 010',
    '8': '111 101 111 101 111', '9': '111 101 111 001 111',
    'a': '010 101 111 101 101', 'b': '110 101 110 101 110',
    'c': '111 100 100 100 111', 'd': '110 101 101 101 110',
    'e': '111 100 111 100 111', 'f': '111 100 111 100 100',
}

STR_BASE, STR_END = 0x8F18, 0x9EA8


def load_tiles(d):
    return (Path(d, 'epr-11034.4').read_bytes(),
            Path(d, 'epr-11035.5').read_bytes(),
            Path(d, 'epr-11036.6').read_bytes())


def tilepx(planes, code):
    a, b, c = planes
    out = []
    for row in range(8):
        b2, b1, b0 = a[code * 8 + row], b[code * 8 + row], c[code * 8 + row]
        out.append([(((b2 >> (7 - x)) & 1) << 2 | ((b1 >> (7 - x)) & 1) << 1 |
                     ((b0 >> (7 - x)) & 1)) for x in range(8)])
    return out


def write_png(path, img):
    h, w = len(img), len(img[0])
    rows = []
    for y in range(h):
        row = bytearray([0])
        for x in range(w):
            r, g, b = img[y][x]
            row += bytes([r, g, b, 255])
        rows.append(bytes(row))
    data = zlib.compress(b''.join(rows), 9)

    def chunk(n, d):
        return (struct.pack('>I', len(d)) + n + d +
                struct.pack('>I', zlib.crc32(n + d) & 0xffffffff))

    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) +
                     chunk(b'IDAT', data) + chunk(b'IEND', b''))


def draw_hex(img, x0, y0, text, col):
    for k, ch in enumerate(text):
        patt = HEX[ch].split()
        for y in range(5):
            for x in range(3):
                if patt[y][x] == '1':
                    img[y0 + y][x0 + k * 5 + x] = col


def split_chunks(m6):
    chunks, cur = [], []
    for b in m6[STR_BASE:STR_END]:
        if b == 0xFF:
            if cur:
                chunks.append(cur)
                cur = []
        else:
            cur.append(b)
    if cur:
        chunks.append(cur)
    return chunks


def main():
    root = Path(__file__).resolve().parent.parent
    kr_tiles = load_tiles(root / 'roms-kr')
    jp_tiles = load_tiles(root / 'roms-wbmljb')
    kr_chunks = split_chunks((root / 'roms-kr' / 'm-6.bin').read_bytes())
    jp_chunks = split_chunks((root / 'roms-wbmljb' / 'm-6.bin').read_bytes())
    print(f'JP strings: {len(jp_chunks)}, KR strings: {len(kr_chunks)}')
    outdir = root / 'build' / 'krmap'
    outdir.mkdir(parents=True, exist_ok=True)

    S = 3
    CW = 9 * S
    CH = 8 * S + 12
    MAXC = 30
    # Interleave: JP rows then KR rows per string, keeping a string's rows
    # together on one page where possible.
    rows = []
    n = max(len(jp_chunks), len(kr_chunks))
    for ci in range(n):
        for tag, tiles, chunks in (('jp', jp_tiles, jp_chunks),
                                   ('kr', kr_tiles, kr_chunks)):
            if ci >= len(chunks):
                continue
            ch = chunks[ci]
            for start in range(0, len(ch), MAXC):
                rows.append((ci, tag, tiles, ch[start:start + MAXC]))

    PER_PAGE = 18
    npages = 0
    for pg in range(0, len(rows), PER_PAGE):
        sel = rows[pg:pg + PER_PAGE]
        W = MAXC * CW + 40
        H = len(sel) * (CH + 6) + 6
        img = [[(25, 25, 55)] * W for _ in range(H)]
        for ri, (ci, tag, tiles, bs) in enumerate(sel):
            oy = ri * (CH + 6) + 4
            label_col = (255, 140, 140) if tag == 'jp' else (120, 255, 120)
            for k, ch_ in enumerate(f'{ci:02x}'):
                patt = HEX[ch_].split()
                for y in range(5):
                    for x in range(3):
                        if patt[y][x] == '1':
                            for dy in range(2):
                                for dx in range(2):
                                    img[oy + 8 + y * 2 + dy][4 + k * 9 + x * 2 + dx] = label_col
            for i, b in enumerate(bs):
                ox = 40 + i * CW
                px = tilepx(tiles, 0x800 + b)
                for y in range(8):
                    for x in range(8):
                        if px[y][x] == 7:
                            for dy in range(S):
                                for dx in range(S):
                                    img[oy + y * S + dy][ox + x * S + dx] = (255, 255, 255)
                draw_hex(img, ox, oy + 8 * S + 2, f'{b:02x}', (255, 200, 80))
        write_png(outdir / f'page_{pg // PER_PAGE:02d}.png', img)
        npages += 1
    print(f'-> {npages} pages in {outdir}/ '
          '(string number: red = Japanese, green = Korean)')

    # transcription template, one line per Korean string
    tpl = outdir / 'kr_strings_template.txt'
    with open(tpl, 'w', encoding='utf-8') as f:
        f.write('# 한국어 받아쓰기: 각 줄의 "번호:" 뒤에 시트의 한글 텍스트를\n'
                '# 보이는 그대로 입력하세요 (띄어쓰기 포함, 제어문자 무시).\n'
                '# 잘 안 보이면 ?로 표시. 빈 줄/주석(#)은 무시됩니다.\n')
        for ci in range(len(kr_chunks)):
            f.write(f'{ci:02x}: \n')
    print(f'template: {tpl}')


if __name__ == '__main__':
    main()
