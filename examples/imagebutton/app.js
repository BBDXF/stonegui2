/**
 * app.js — imagebutton smoke test (released/pressed state PNGs).
 *
 * Run:   ./build/stonegui examples/imagebutton/app.js
 * Kill:  Ctrl+C (or `timeout --signal=TERM 3 …`)
 *
 * Refresh the test assets with:   python3 examples/imagebutton/make_states.py
 * (downloads 🤍 / ❤️ heart emojis from Twemoji CDN, CC-BY 4.0)
 */

import { render, h, loadImage, createSignal } from "../../js/framework.js";

const ASSETS = "/home/andy/learn/stonegui/examples/imagebutton";
const released = loadImage(`${ASSETS}/released.png`);
const pressed  = loadImage(`${ASSETS}/pressed.png`);

console.log("== imagebutton smoke test ==");
console.log("released handle:", released, " pressed handle:", pressed);

if (!released || !pressed) {
    console.log("FAIL — one of the state PNGs failed to load");
}

const [clicks, setClicks] = createSignal(0);

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
            text: "ImageButton — press to swap 🤍 → ❤️ (like / love toggle)",
            style: { textColor: "#cdd6f4", fontSize: 20 },
        }),
        h("imagebutton", {
            released: released,
            pressed: pressed,
            style: { width: 128, height: 128 },
            onClick: () => setClicks(clicks() + 1),
        }),
        h("text", {
            text: () => `clicks: ${clicks()}`,
            style: { textColor: "#cdd6f4", fontSize: 18 },
        }),
    )
);

console.log("ALL TESTS PASSED — bundle mounted without throwing");
