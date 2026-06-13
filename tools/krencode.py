#!/usr/bin/env python3
"""Re-encode the in-game Korean strings from the user's translation file.

Reads  korean-patch/kr_strings.txt  (the translation, one line per
string id) and rewrites the dialog strings inside roms-kr/m-6.bin in place,
preserving each string's exact byte length and control structure:

    chunk = ctrl(3B)  [text span]  (7B xx | FE ctrl(3B))  [text span] ... FF

Because the emulator overlays Hangul glyphs from its own font (krtext.c),
tile codes are free to reassign.  Encoding scheme:
  - frequent syllables -> 1 byte (single code)
  - rare syllables     -> 2 bytes (cho-jamo code + body code)
  - digits/punctuation -> original tile codes (drawn as original tiles)
Emits src/win-ref/krtext.h so the overlay decodes exactly this encoding.

Run after tools/mkbootleg.py:
    python3 tools/mkbootleg.py
    python3 tools/krencode.py
"""
import sys
from collections import Counter
from pathlib import Path

from mkbootleg import apply_ips

STR_BASE, STR_END = 0x8F18, 0x9EA8
CHO = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
NJONG = 28

# cho-jamo codes (tile low bytes): the 14 plain jamo keep their original
# slots, double consonants reuse 주니's extra jamo tiles.
CHO_CODE = {'ㄱ': 0x00, 'ㄴ': 0x01, 'ㄷ': 0x02, 'ㄹ': 0x03, 'ㅁ': 0x04,
            'ㅂ': 0x05, 'ㅅ': 0x06, 'ㅇ': 0x07, 'ㅈ': 0x08, 'ㅊ': 0x09,
            'ㅋ': 0x0A, 'ㅌ': 0x0B, 'ㅍ': 0x0C, 'ㅎ': 0x0D,
            'ㄲ': 0x5F, 'ㄸ': 0x60, 'ㅃ': 0x61, 'ㅆ': 0x62, 'ㅉ': 0x63}

SPACE, CTRL_LINE, CTRL_DAKU, CTRL_PAGE, CTRL_END = 0x2E, 0x7B, 0x7C, 0xFE, 0xFF


def decompose(c):
    s = ord(c) - 0xAC00
    return CHO[s // (21 * 28)], (s % (21 * 28)) // 28, s % 28


def load_texts(path):
    texts = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#') or ':' not in line:
            continue
        idx, _, rest = line.partition(':')
        try:
            ci = int(idx, 16)
        except ValueError:
            continue
        texts[ci] = texts.get(ci, '')
        texts[ci] += (' ' if texts[ci] else '') + rest.strip()
    return texts


def load_nonhangul(mapping_path):
    """Original codes for digits/punctuation from the derived mapping."""
    out = {}
    if not Path(mapping_path).exists():
        return out
    for line in Path(mapping_path).read_text(encoding='utf-8').splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0] == 'single':
            ch = parts[2]
            if len(ch) == 1 and not (0xAC00 <= ord(ch) <= 0xD7A3):
                out.setdefault(ch, int(parts[1], 16))
    return out


def split_chunks_at(m6):
    """Return [(abs_offset, bytes)] for each FF-terminated string."""
    chunks = []
    cur, start = [], STR_BASE
    i = STR_BASE
    while i < STR_END:
        b = m6[i]
        if b == 0xFF:
            if cur:
                chunks.append((start, list(cur)))
            cur, start = [], i + 1
        else:
            cur.append(b)
        i += 1
    if cur:
        chunks.append((start, list(cur)))
    return chunks


def parse_skeleton(ch):
    """Split a chunk into (separators, span capacities).
    Returns list of items: ('ctrl', bytes) or ('span', length)."""
    items = [('ctrl', ch[:3])]
    i, span = 3, 0
    while i < len(ch):
        b = ch[i]
        if b == CTRL_PAGE:
            items.append(('span', span)); span = 0
            items.append(('ctrl', ch[i:i + 4]))
            i += 4
        elif b in (CTRL_LINE, CTRL_DAKU):
            items.append(('span', span)); span = 0
            items.append(('ctrl', ch[i:i + 2]))
            i += 2
        else:
            span += 1
            i += 1
    items.append(('span', span))
    return items


def main():
    root = Path(__file__).resolve().parent.parent
    texts = load_texts(root / 'korean-patch' / 'kr_strings.txt')
    nonhangul = load_nonhangul(root / 'build' / 'krmap' / 'mapping.txt')
    # user-typed variants of the same punctuation
    for a, b in (('.', '。'), ('。', '.')):
        if a not in nonhangul and b in nonhangul:
            nonhangul[a] = nonhangul[b]
    nonhangul.setdefault('♥', 0x2F)
    nonhangul.setdefault(',', SPACE)  # no comma tile — render as space

    # pristine patched m-6 (so the run is idempotent)
    # prefer roms/wbmljb/ (new layout), fall back to roms-wbmljb/
    m6_src = root / 'roms' / 'wbmljb' / 'm-6.bin'
    if not m6_src.exists():
        m6_src = root / 'roms-wbmljb' / 'm-6.bin'
    m6 = bytearray(apply_ips(
        m6_src.read_bytes(),
        (root / 'korean-patch' / 'wbmljb' / 'm-6k.ips').read_bytes()))
    chunks = split_chunks_at(m6)
    print(f'{len(chunks)} strings, {len(texts)} translations')

    # ---- code allocation ---------------------------------------------------
    freq = Counter()
    for t in texts.values():
        for c in t:
            if 0xAC00 <= ord(c) <= 0xD7A3:
                freq[c] += 1

    reserved = {SPACE, CTRL_LINE, CTRL_DAKU, CTRL_PAGE, CTRL_END}
    reserved |= set(nonhangul.values())
    reserved |= set(range(0xC8, 0xDC))          # cursor, digits, box borders
    reserved |= set(CHO_CODE.values())
    pool = [b for b in range(0xFD) if b not in reserved]

    def allocate(rank):
        """singles for the top of `rank`, pairs for the rest (fixed point)."""
        nsingle = len(pool)
        for _ in range(8):
            rest = rank[nsingle:]
            nb = len({decompose(c)[1] * NJONG + decompose(c)[2] for c in rest})
            if len(pool) - nb == nsingle:
                break
            nsingle = len(pool) - nb
        singles = rank[:nsingle]
        rest = rank[nsingle:]
        bodies = sorted({decompose(c)[1] * NJONG + decompose(c)[2]
                         for c in rest})
        body_code, syll_code = {}, {}
        it = iter(pool)
        for bv in bodies:
            body_code[bv] = next(it)
        for c in singles:
            syll_code[c] = next(it)
        return syll_code, body_code

    # iterate: syllables inside strings that overflow get allocation priority
    boost = Counter()
    syll_code, body_code = {}, {}
    for it_round in range(4):
        rank = [c for c, _ in (freq + boost).most_common()]
        syll_code, body_code = allocate(rank)
        over = set()
        for ci, (off, ch) in enumerate(chunks):
            text = texts.get(ci)
            if not text:
                continue
            cap = sum(v for k, v in parse_skeleton(ch) if k == 'span')
            need = 0
            for c in text:
                if c == ' ' or c in nonhangul:
                    need += 1
                elif c in syll_code:
                    need += 1
                elif 0xAC00 <= ord(c) <= 0xD7A3:
                    need += 2
            if need > cap:
                for c in text.rstrip(' .!?'):
                    if 0xAC00 <= ord(c) <= 0xD7A3 and c not in syll_code:
                        boost[c] += 1000
                        over.add(ci)
        if not over:
            break
    print(f'allocation: {len(syll_code)} singles, {len(body_code)} bodies '
          f'(round {it_round + 1})')

    def encode_char(c):
        if c == ' ':
            return [SPACE]
        if c in syll_code:
            return [syll_code[c]]
        if 0xAC00 <= ord(c) <= 0xD7A3:
            cho, jung, jong = decompose(c)
            return [CHO_CODE[cho], body_code[jung * NJONG + jong]]
        if c in nonhangul:
            return [nonhangul[c]]
        return None  # unencodable

    # ---- re-encode each chunk in place ------------------------------------
    warns = 0
    for ci, (off, ch) in enumerate(chunks):
        text = texts.get(ci)
        if not text:
            continue
        skel = parse_skeleton(ch)
        out = []
        # token stream
        toks = []
        for c in text:
            enc = encode_char(c)
            if enc is None:
                print(f'  [{ci:02x}] cannot encode {c!r} - skipped')
                continue
            toks.append(enc)
        ti = 0
        tchars = [c for c in text if encode_char(c) is not None]
        for kind, val in skel:
            if kind == 'ctrl':
                out.extend(val)
                continue
            cap = val
            filled = []
            while ti < len(toks) and len(filled) + len(toks[ti]) <= cap:
                filled.extend(toks[ti])
                ti += 1
            # drop a leading space at line start
            while ti < len(toks) and toks[ti] == [SPACE]:
                ti += 1
            filled.extend([SPACE] * (cap - len(filled)))
            out.extend(filled)
        if ti < len(toks):
            cut = ''.join(tchars[ti:])
            if cut.strip(' .!?'):  # cutting only punctuation is harmless
                print(f'  [{ci:02x}] 길이 초과, 잘림: "{cut}"')
                warns += 1
        assert len(out) == len(ch), (ci, len(out), len(ch))
        m6[off:off + len(ch)] = bytes(out)

    out_m6 = root / 'roms' / 'kr' / 'm-6.bin'
    out_m6.write_bytes(m6)
    print(f'wrote {out_m6} ({warns} overflow warnings)')

    # ---- emit krtext.h ------------------------------------------------------
    typ = [0] * 256
    val = [0] * 256
    for j, code in CHO_CODE.items():
        typ[code] = 1
        val[code] = CHO.index(j)
    for bv, code in body_code.items():
        typ[code] = 2
        val[code] = bv
    for c, code in syll_code.items():
        typ[code] = 3
        val[code] = ord(c)
    typ[SPACE] = 4
    hdr = root / 'src' / 'win-ref' / 'krtext.h'
    with open(hdr, 'w', encoding='utf-8') as f:
        f.write('// Generated by tools/krencode.py — do not edit.\n')
        f.write('// Tile semantics for the re-encoded Korean strings.\n')
        f.write('#pragma once\n#include <stdint.h>\n\n')
        f.write('#define KRT_NONE 0\n#define KRT_CHO 1\n#define KRT_BODY 2\n')
        f.write('#define KRT_SYLL 3\n#define KRT_SPACE 4\n\n')
        f.write('static const uint8_t kr_tile_type[256] = {\n')
        for i in range(0, 256, 16):
            f.write('    ' + ','.join(str(t) for t in typ[i:i+16]) + ',\n')
        f.write('};\n\n')
        f.write('// cho: cho index / body: jung*28+jong / syll: codepoint\n')
        f.write('static const uint16_t kr_tile_val[256] = {\n')
        for i in range(0, 256, 16):
            f.write('    ' + ','.join(f'0x{v:04X}' for v in val[i:i+16]) +
                    ',\n')
        f.write('};\n')
    print(f'wrote {hdr}')


if __name__ == '__main__':
    main()
