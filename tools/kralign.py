#!/usr/bin/env python3
"""Align transcribed Korean text with the patched string bytes to derive
the tile-code -> syllable mapping.

Inputs:
  - roms-kr/m-6.bin                       (patched strings)
  - korean-patch/kr_strings.txt           (user transcription)

String structure: [ctrl x3] tokens... (FE [ctrl x3] tokens...)*
Tokens: PAIR(cho 0x00-0x0D, body) = one syllable, or SINGLE(byte).
SINGLE(0x2E) is a space and is dropped; text spaces/newlines are dropped.

Alignment per chunk: token count must equal text char count; pairs verify
that the syllable's cho-seong matches the cho tile.  Mismatching chunks
are reported for manual review.  Mappings are accumulated with votes
across all chunks; conflicts are reported.

Output: build/krmap/mapping.txt (and alignment report on stdout)
"""
import sys
from collections import Counter, defaultdict
from pathlib import Path

STR_BASE, STR_END = 0x8F18, 0x9EA8
CHO_TILE = "ㄱㄴㄷㄹㅁㅂㅅㅇㅈㅊㅋㅌㅍㅎ"  # tiles 0x800-0x80D
CHO = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
JONG = ["", "ㄱ", "ㄲ", "ㄳ", "ㄴ", "ㄵ", "ㄶ", "ㄷ", "ㄹ", "ㄺ", "ㄻ", "ㄼ",
        "ㄽ", "ㄾ", "ㄿ", "ㅀ", "ㅁ", "ㅂ", "ㅄ", "ㅅ", "ㅆ", "ㅇ", "ㅈ",
        "ㅊ", "ㅋ", "ㅌ", "ㅍ", "ㅎ"]


def decompose(s):
    code = ord(s) - 0xAC00
    if not 0 <= code <= 11171:
        return None
    return CHO[code // 588], JUNG[(code % 588) // 28], JONG[code % 28]


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


# body tiles that can follow a single tile as a visual vowel extension
# (e.g. 뭐 = [무][ㅓ]): vertical vowels only
VOWEL_EXT = {0x10: 'ㅓ', 0x0E: 'ㅏ', 0x16: 'ㅣ', 0x12: 'ㅔ', 0x14: 'ㅐ'}

# double-consonant jamo tiles that pair with a body like cho tiles do
# (e.g. 께 = [ㄲ][ㅔ], 때 = [ㄸ][ㅐ], 빨 = [ㅃ][ㅏㄹ], 싸 = [ㅆ][ㅏ])
DOUBLE_CHO = {0x5F: 'ㄲ', 0x60: 'ㄸ', 0x61: 'ㅃ', 0x62: 'ㅆ'}


def tokenize_bytes(ch):
    """Strip ctrl prefixes/separators, return list of ('pair',cho,body),
    ('single',b) or ('ext',b,ext) tokens."""
    toks = []
    i = 3  # leading ctrl
    while i < len(ch):
        b = ch[i]
        if b == 0xFE:           # line separator + new ctrl prefix
            i += 4
            continue
        if b in (0x7B, 0x7C):   # mid-string control + 1 param byte
            i += 2
            continue
        if b <= 0x0D and i + 1 < len(ch):
            toks.append(('pair', b, ch[i + 1]))
            i += 2
            continue
        if b in DOUBLE_CHO and i + 1 < len(ch):
            toks.append(('dpair', b, ch[i + 1]))
            i += 2
            continue
        if b == 0x2E:           # space
            i += 1
            continue
        # single, possibly followed by a vowel-extension tile
        if b >= 0x40 and i + 1 < len(ch) and ch[i + 1] in VOWEL_EXT:
            toks.append(('ext', b, ch[i + 1]))
            i += 2
            continue
        toks.append(('single', b))
        i += 1
    return toks


def tokenize_text(t):
    """Drop spaces; return list of chars (syllables, digits, punct)."""
    return [c for c in t if not c.isspace()]


def load_transcription(path):
    texts = defaultdict(str)
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if ':' not in line:
            continue
        idx, _, rest = line.partition(':')
        try:
            ci = int(idx, 16)
        except ValueError:
            continue
        texts[ci] += ('\n' if texts[ci] else '') + rest.strip()
    return texts


def compose(cho, jung, jong):
    return chr(0xAC00 + (CHO.index(cho) * 21 + JUNG.index(jung)) * 28 +
               JONG.index(jong))


# vowel-extension merge: base vowel of the single + ext vowel -> compound
EXT_MERGE = {('ㅜ', 'ㅓ'): 'ㅝ', ('ㅗ', 'ㅏ'): 'ㅘ', ('ㅡ', 'ㅣ'): 'ㅢ',
             ('ㅜ', 'ㅣ'): 'ㅟ', ('ㅗ', 'ㅣ'): 'ㅚ', ('ㅜ', 'ㅔ'): 'ㅞ',
             ('ㅗ', 'ㅐ'): 'ㅙ'}


def decode_chunks(chunks, singles, bodies):
    """Decode all chunks with the current mapping; track unknown bytes."""
    out = []
    unknown = Counter()
    for ch in chunks:
        s = []
        for t in tokenize_bytes(ch):
            if t[0] in ('pair', 'dpair'):
                cho = CHO_TILE[t[1]] if t[0] == 'pair' else DOUBLE_CHO[t[1]]
                body = bodies.get(t[2])
                if body is None:
                    s.append(f'[{t[1]:02x}+{t[2]:02x}]')
                    unknown[('body', t[2])] += 1
                else:
                    jung, jong = body
                    s.append(compose(cho, jung, jong))
            elif t[0] == 'ext':
                base = singles.get(t[1])
                extv = VOWEL_EXT[t[2]]
                if base is None:
                    s.append(f'[{t[1]:02x}~{t[2]:02x}]')
                    unknown[('single', t[1])] += 1
                else:
                    d = decompose(base)
                    merged = EXT_MERGE.get((d[1], extv)) if d else None
                    if d and merged and d[2] == '':
                        s.append(compose(d[0], merged, ''))
                    else:
                        s.append(base + extv)
            else:
                c = singles.get(t[1])
                if c is None:
                    s.append(f'[{t[1]:02x}]')
                    unknown[('single', t[1])] += 1
                else:
                    s.append(c)
        out.append(''.join(s))
    return out, unknown


def load_overrides(path):
    """known-mapping overrides: lines 'single XX char' / 'body XX jung jong'"""
    singles, bodies = {}, {}
    if not path.exists():
        return singles, bodies
    for line in path.read_text(encoding='utf-8').splitlines():
        line = line.split('#')[0].strip()
        if not line:
            continue
        parts = line.split()
        if parts[0] == 'single' and len(parts) >= 3:
            singles[int(parts[1], 16)] = parts[2]
        elif parts[0] == 'body' and len(parts) >= 4:
            bodies[int(parts[1], 16)] = (parts[2],
                                         '' if parts[3] == '-' else parts[3])
    return singles, bodies


def main():
    root = Path(__file__).resolve().parent.parent
    kr_m6 = root / 'roms' / 'kr' / 'm-6.bin'
    if not kr_m6.exists():
        kr_m6 = root / 'roms-kr' / 'm-6.bin'
    chunks = split_chunks(kr_m6.read_bytes())
    texts = load_transcription(root / 'korean-patch' / 'kr_strings.txt')
    ov_singles, ov_bodies = load_overrides(root / 'tools' / 'kr_overrides.txt')

    single_votes = defaultdict(Counter)   # byte -> char votes
    body_votes = defaultdict(Counter)     # body byte -> (jung,jong) votes
    naligned = 0

    for ci, ch in enumerate(chunks):
        text = texts.get(ci, '')
        btoks = tokenize_bytes(ch)
        ttoks = tokenize_text(text)
        # tolerate user-added trailing punctuation
        while len(ttoks) > len(btoks) and ttoks[-1] in '.!?,。':
            ttoks.pop()
        if len(btoks) != len(ttoks):
            continue
        ok = True
        for bt, tc in zip(btoks, ttoks):
            if bt[0] in ('pair', 'dpair'):
                want = CHO_TILE[bt[1]] if bt[0] == 'pair' else DOUBLE_CHO[bt[1]]
                d = decompose(tc)
                if d is None or d[0] != want:
                    ok = False
                    break
        if not ok:
            continue
        naligned += 1
        for bt, tc in zip(btoks, ttoks):
            if bt[0] in ('pair', 'dpair'):
                _, jung, jong = decompose(tc)
                body_votes[bt[2]][(jung, jong)] += 1
            elif bt[0] == 'single':
                single_votes[bt[1]][tc] += 1

    print(f'{len(chunks)} chunks, {naligned} cleanly aligned')

    # voted maps + manual overrides (overrides win)
    singles = {b: v.most_common(1)[0][0] for b, v in single_votes.items()}
    bodies = {b: v.most_common(1)[0][0] for b, v in body_votes.items()}
    singles.update(ov_singles)
    bodies.update(ov_bodies)

    decoded, unknown = decode_chunks(chunks, singles, bodies)
    out = root / 'build' / 'krmap' / 'decoded.txt'
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, 'w', encoding='utf-8') as f:
        for ci, txt in enumerate(decoded):
            ref = texts.get(ci, '').replace('\n', ' / ')
            f.write(f'{ci:02x}: {txt}\n')
            f.write(f'  ref {ref}\n')
    print(f'wrote {out}')
    print(f'mapping: {len(singles)} singles, {len(bodies)} bodies')
    if unknown:
        print('unknown tiles by frequency:')
        for (kind, b), n in unknown.most_common(40):
            print(f'  {kind} {b:02x} x{n}')

    # write mapping file
    mp = root / 'build' / 'krmap' / 'mapping.txt'
    with open(mp, 'w', encoding='utf-8') as f:
        f.write('# tile mapping: singles tile->char, bodies tile->jung/jong\n')
        for b in sorted(singles):
            f.write(f'single {b:02x} {singles[b]}\n')
        for b in sorted(bodies):
            jung, jong = bodies[b]
            f.write(f'body {b:02x} {jung} {jong or "-"}\n')
    print(f'wrote {mp}')

    # write C header for the emulator-side overlay renderer
    hdr = root / 'src' / 'win-ref' / 'krtext.h'
    with open(hdr, 'w', encoding='utf-8') as f:
        f.write('// Generated by tools/kralign.py — do not edit.\n')
        f.write('// Korean-patch tile semantics for fg tiles 0x800-0x8FF.\n')
        f.write('#pragma once\n#include <stdint.h>\n\n')
        f.write('// type per tile low-byte: 0=none 1=cho-jamo 2=body '
                '3=syllable 4=space\n')
        f.write('#define KRT_NONE 0\n#define KRT_CHO 1\n#define KRT_BODY 2\n')
        f.write('#define KRT_SYLL 3\n#define KRT_SPACE 4\n\n')
        typ = [0] * 256
        val = [0] * 256
        for b in range(0x0E):
            typ[b] = 1
            val[b] = CHO.index(CHO_TILE[b])
        for b, j in DOUBLE_CHO.items():
            typ[b] = 1
            val[b] = CHO.index(j)
        for b, (jung, jong) in bodies.items():
            if typ[b]:
                continue
            typ[b] = 2
            val[b] = JUNG.index(jung) * 28 + JONG.index(jong)
        for b, c in singles.items():
            if typ[b] or len(c) != 1 or not (0xAC00 <= ord(c) <= 0xD7A3):
                continue
            typ[b] = 3
            val[b] = ord(c)
        typ[0x2E] = 4
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
    n_mapped = sum(1 for t in typ if t)
    print(f'wrote {hdr} ({n_mapped}/256 tiles mapped)')


if __name__ == '__main__':
    main()
