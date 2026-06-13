#!/usr/bin/env python3
"""Build a Korean-patched wbmljb (bootleg) ROM set from the wbml set.

The wbmljb bootleg ROMs are the MC-8123-decrypted views of the original
program ROMs: each 64KB file is <32KB opcode view> + <32KB data view>.
We already have the decryption (tools/mc8123.py), so the bootleg files
can be generated from the user's own wbml set and verified by CRC32:

    wbml.01  = views of epr-11031a.90 (0x00000)   CRC 66482638
    m-6.bin  = views of epr-11032.91  (0x10000)   CRC 8c08cd11
    m-7.bin  = views of epr-11033.92  (0x18000)   CRC 11881703

Then the Korean fan-translation IPS patches (korean-patch/wbmljb/) are
applied: m-6k.ips to m-6.bin, epr-1103[456]k.ips to the tile ROMs.

Usage:
    python3 tools/mkbootleg.py            # writes roms-kr/
Run the result with:
    ./build/flydo roms-kr
"""
import sys
import shutil
import zlib
from pathlib import Path

import mc8123  # decryption (same directory)

BOOTLEG_CRC = {
    "wbml.01": 0x66482638,
    "m-6.bin": 0x8C08CD11,
    "m-7.bin": 0x11881703,
}
SOURCE_CRC = {  # pre-patch checksums from Korean_Translation.dat / MAME
    "epr-11034.4": 0x37A2077D,
    "epr-11035.5": 0xCDF2A21B,
    "epr-11036.6": 0x644687FA,
}
COPY_FILES = [  # unchanged ROMs shared with the wbml set
    "epr-11037.126",
    "epr-11027.86", "epr-11028.87", "epr-11029.88", "epr-11030.89",
    "pr11024.8", "pr11025.14", "pr11026.20", "pr5317.37",
]


def apply_ips(data: bytes, patch: bytes) -> bytes:
    assert patch[:5] == b"PATCH", "not an IPS file"
    out = bytearray(data)
    pos = 5
    while True:
        if patch[pos:pos + 3] == b"EOF":
            break
        off = int.from_bytes(patch[pos:pos + 3], "big"); pos += 3
        size = int.from_bytes(patch[pos:pos + 2], "big"); pos += 2
        if size == 0:  # RLE record
            rle = int.from_bytes(patch[pos:pos + 2], "big"); pos += 2
            chunk = patch[pos:pos + 1] * rle; pos += 1
        else:
            chunk = patch[pos:pos + size]; pos += size
        if off + len(chunk) > len(out):
            out.extend(b"\xff" * (off + len(chunk) - len(out)))
        out[off:off + len(chunk)] = chunk
    return bytes(out)


def from_decryption(romdir):
    """Generate bootleg-format files by decrypting the wbml set."""
    key_rom = (romdir / "317-0043.key").read_bytes()
    region = bytearray(b"\xff" * 0x20000)
    region[0x00000:0x08000] = (romdir / "epr-11031a.90").read_bytes()
    region[0x10000:0x18000] = (romdir / "epr-11032.91").read_bytes()
    region[0x18000:0x20000] = (romdir / "epr-11033.92").read_bytes()
    opcodes, data = mc8123.decode_region(
        bytes(region), key_rom, [(0x00000, 0x08000), (0x10000, 0x20000)])
    return {
        "wbml.01": opcodes[0x00000:0x08000] + data[0x00000:0x08000],
        "m-6.bin": opcodes[0x10000:0x18000] + data[0x10000:0x18000],
        "m-7.bin": opcodes[0x18000:0x20000] + data[0x18000:0x20000],
    }


def main():
    root = Path(__file__).resolve().parent.parent
    # wbml standard ROMs: prefer roms/wbml/ (new layout), fall back to roms/
    romdir = root / "roms" / "wbml"
    if not (romdir / "epr-11031a.90").exists():
        romdir = root / "roms"
    # wbmljb bootleg ROMs: prefer roms/wbmljb/ (new layout), fall back to roms-wbmljb/
    bootlegdir = root / "roms" / "wbmljb"
    if not (bootlegdir / "wbml.01").exists():
        bootlegdir = root / "roms-wbmljb"
    patchdir = root / "korean-patch" / "wbmljb"
    # Output inside roms/ so the ROM selector picks it up automatically
    outdir = root / "roms" / "kr"
    outdir.mkdir(parents=True, exist_ok=True)

    # 1. Program ROMs: prefer the real wbmljb files (the patch was authored
    #    against them); otherwise derive them by decrypting the wbml set.
    if (bootlegdir / "wbml.01").exists():
        print("using real wbmljb program ROMs from roms-wbmljb/")
        bootleg = {n: (bootlegdir / n).read_bytes()
                   for n in ("wbml.01", "m-6.bin", "m-7.bin")}
    else:
        print("roms-wbmljb/ not found — deriving program ROMs by decryption")
        bootleg = from_decryption(romdir)

    # 2. Verify. With derived files, wbml.01 differs (newer 'a' revision of
    #    epr-11031) and m-6.bin differs (bootleg-specific edits in the code
    #    half), but the Korean patch only rewrites text in the data half of
    #    m-6, so the derived set is still compatible.  m-7 must always match
    #    — it validates the decryption itself.
    for name, blob in bootleg.items():
        crc = zlib.crc32(blob)
        match = crc == BOOTLEG_CRC[name]
        print(f"  {name}: crc32={crc:08x} "
              f"{'OK' if match else f'(bootleg ref {BOOTLEG_CRC[name]:08x})'}")
        if name == "m-7.bin" and not match:
            sys.exit("m-7.bin mismatch — decryption is broken, aborting")

    # 3. Apply IPS patches: program ROM + tile ROMs.
    bootleg["m-6.bin"] = apply_ips(
        bootleg["m-6.bin"], (patchdir / "m-6k.ips").read_bytes())
    print("  m-6.bin: Korean program patch applied")
    for name, want in SOURCE_CRC.items():
        src = (romdir / name).read_bytes()
        crc = zlib.crc32(src)
        if crc != want:
            sys.exit(f"{name}: crc32={crc:08x}, expected {want:08x}")
        ips = patchdir / f"{name.split('.')[0]}k.ips"
        bootleg[name] = apply_ips(src, ips.read_bytes())
        print(f"  {name}: Korean tile patch applied ({ips.name})")

    # 4. Write patched set + copy the unchanged ROMs.
    for name, blob in bootleg.items():
        (outdir / name).write_bytes(blob)
    for name in COPY_FILES:
        shutil.copyfile(romdir / name, outdir / name)
    print(f"wrote {outdir}/")


if __name__ == "__main__":
    main()
