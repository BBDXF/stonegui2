/**
 * app.js — animation smoke test (onComplete fires via lv_timer_handler).
 *
 * Animates a view's width 100→250 over 120 ms. Within ~200 ms the LVGL
 * animation engine drives the property to `to`, then fires our completed_cb
 * which calls onComplete — proving the wire-through {C anim engine → JS
 * callback} works end-to-end.
 *
 * Run:   ./build/stonegui examples/anim/app.js   (kill with SIGTERM after ≥ 300 ms)
 * CI grep stdout for "ANIM_COMPLETED".
 */

import { render, h, createAnimation } from "../../js/framework.js";

console.log("== animation smoke test ==");

let ref = null;
render(() =>
    h("view", {
        ref: (n) => ref = n,
        style: {
            width: 100, height: 50,
            backgroundColor: "#3498db", borderRadius: 6,
            x: 20, y: 20,
        },
    })
);

createAnimation(ref, {
    property:   "width",
    from:       100,
    to:         250,
    duration:   120,
    easing:     "ease-out",
    onComplete: () => {
        console.log("ANIM_COMPLETED width(100→250)");
        console.log("ALL TESTS PASSED");
    },
});

console.log("animation started — onComplete expected within ~150 ms");
