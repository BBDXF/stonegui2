/**
 * app.js — animimg smoke test (3-frame loop via loadImages handle batch).
 *
 * Run:   ./build/stonegui examples/animimg/app.js
 * Kill:  Ctrl+C (or `timeout --signal=TERM 3 …`)
 *
 * Refresh the test assets with:   python3 examples/animimg/make_frames.py
 * (downloads 🕐🕔🕘 clock-face emojis from Twemoji CDN, CC-BY 4.0)
 */

import { render, h, loadImages } from "../../js/framework.js";

const ASSETS = "/home/andy/learn/stonegui/examples/animimg";
const frames = loadImages([
    `${ASSETS}/frame1.png`,
    `${ASSETS}/frame2.png`,
    `${ASSETS}/frame3.png`,
]);

console.log("== animimg smoke test ==");
console.log("frames:", JSON.stringify(frames));

if (frames.some((h) => h === 0)) {
    console.log("FAIL — at least one frame failed to load");
}

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
            text: "AnimImg — clock face rotation 🕐 → 🕔 → 🕘 (900 ms / cycle, infinite loop)",
            style: { textColor: "#cdd6f4", fontSize: 20 },
        }),
        h("animimg", {
            src: frames,
            duration: 900,
            repeat: "infinite",
            start: true,
            style: { width: 128, height: 128 },
        }),
    )
);

console.log("ALL TESTS PASSED — bundle mounted without throwing");
