#!/usr/bin/env python3
"""Generate a tiny 16x16 RGB PNG using only stdlib (no Pillow).

Pattern: red/green checkerboard so any working decoder produces an
unmistakably non-blank image. Used by examples/image/app.js to verify
LV_USE_LODEPNG decode wired correctly through the lv_image binding.
"""
import struct, zlib, sys, pathlib

W, H = 16, 16

def chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

def make_png() -> bytes:
    sig  = b"\x89PNG\r\n\x1a\n"
    ihdr = chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
    raw  = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            raw.extend((255, 0, 0) if (x // 4 + y // 4) % 2 == 0 else (0, 200, 0))
    idat = chunk(b"IDAT", zlib.compress(bytes(raw)))
    iend = chunk(b"IEND", b"")
    return sig + ihdr + idat + iend

if __name__ == "__main__":
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "test.png")
    out.write_bytes(make_png())
    print(f"wrote {out} ({out.stat().st_size} bytes, {W}x{H} RGB)")
