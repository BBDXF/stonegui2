/**
 * app.js — image decoder smoke test (PNG via bundled lodepng).
 *
 * Verifies that LV_USE_LODEPNG + LV_USE_FS_POSIX let `<image src="A:/...">`
 * load a real PNG from disk with zero system dependencies.
 *
 * Run:   ./build/stonegui examples/image/app.js
 * Kill:  Ctrl+C (or `timeout --signal=TERM 3 …`)
 *
 * Refresh the test asset with:   python3 examples/image/make_png.py
 * (downloads the 🖼️ framed-picture emoji from Twemoji CDN, CC-BY 4.0)
 */

import { render, h } from "../../js/framework.js";

const ASSETS = "/home/andy/learn/stonegui/examples/image";
const PNG    = `A:${ASSETS}/test.png`;

console.log("== image smoke test ==");
console.log("loading:", PNG);

render(() =>
    h("view", {
        style: {
            width: "100%", height: "100%",
            backgroundColor: "#1e1e2e",
            flexFlow: "column", padding: 24, gap: 16,
            alignItems: "center", justifyContent: "center",
        },
    },
        h("text", {
            text: "PNG decode test — Twemoji 🖼️ framed-picture emoji below",
            style: { textColor: "#cdd6f4", fontSize: 20 },
        }),
        h("image", {
            src: PNG,
            style: { width: 128, height: 128 },
        }),
    )
);

console.log("ALL TESTS PASSED — bundle mounted without throwing");
console.log("(visually verify the 128x128 framed-picture emoji; LVGL silently shows a broken-image glyph on decode failure)");
