#!/usr/bin/env python3
"""Download the <image> smoke-test PNG from the Twemoji CDN.

Source:  https://github.com/twitter/twemoji  (Twemoji 14.0.2)
License: Twemoji graphics are CC-BY 4.0 — attribution: "Twemoji" by Twitter.
Re-run this script to refresh the asset; only Python stdlib is required.
"""
import urllib.request, pathlib

TWEMOJI_BASE = "https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72"

ASSETS = {
    "test.png": "1f5bc",  # 🖼️ framed picture — meta: an "image" of an image
}

if __name__ == "__main__":
    out_dir = pathlib.Path(__file__).parent
    for name, cp in ASSETS.items():
        url  = f"{TWEMOJI_BASE}/{cp}.png"
        path = out_dir / name
        with urllib.request.urlopen(url) as r:
            path.write_bytes(r.read())
        print(f"wrote {path} ({path.stat().st_size} bytes)  <- {url}")
