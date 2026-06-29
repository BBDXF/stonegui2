#!/usr/bin/env python3
"""Download 3 clock-face PNGs from the Twemoji CDN for the <animimg> loop.

Cycling 1 → 5 → 9 o'clock gives an unmistakably rotational animation feel
so the smoke test is visually obvious.

Source:  https://github.com/twitter/twemoji  (Twemoji 14.0.2)
License: Twemoji graphics are CC-BY 4.0 — attribution: "Twemoji" by Twitter.
Re-run this script to refresh the assets; only Python stdlib is required.
"""
import urllib.request, pathlib

TWEMOJI_BASE = "https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72"

ASSETS = {
    "frame1.png": "1f550",  # 🕐 one oclock
    "frame2.png": "1f554",  # 🕔 five oclock
    "frame3.png": "1f558",  # 🕘 nine oclock
}

if __name__ == "__main__":
    out_dir = pathlib.Path(__file__).parent
    for name, cp in ASSETS.items():
        url  = f"{TWEMOJI_BASE}/{cp}.png"
        path = out_dir / name
        with urllib.request.urlopen(url) as r:
            path.write_bytes(r.read())
        print(f"wrote {path} ({path.stat().st_size} bytes)  <- {url}")
