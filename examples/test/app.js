/**
 * app.js — framework reactive primitives smoke test
 *
 * Run:  ./build/stonegui examples/test/app.js   (kill with SIGTERM)
 *
 * Prints PASS / FAIL for each primitive, then a final summary. CI greps for
 * the summary line; humans read the per-test log.
 */

import {
    createSignal, createEffect, createMemo, createRoot,
    onCleanup, untrack, batch, render, h, Show, For,
} from "../../js/framework.js";

let pass = 0, fail = 0;
function check(cond, name) {
    if (cond) { pass++; console.log("  PASS", name); }
    else      { fail++; console.log("  FAIL", name); }
}

console.log("\n== createMemo ==");
{
    const [a, setA] = createSignal(10);
    const doubled = createMemo(() => a() * 2);
    check(doubled() === 20, "initial value");
    setA(5);
    check(doubled() === 10, "updates on signal change");
}

console.log("\n== batch ==");
{
    const [x, setX] = createSignal(1);
    const [y, setY] = createSignal(2);
    let runs = 0;
    createEffect(() => { runs++; x(); y(); });
    check(runs === 1, "effect runs once at creation");
    batch(() => { setX(10); setY(20); });
    check(runs === 2, "two sets in batch → one effect run");

    setX(11);
    setY(21);
    check(runs === 4, "two sets without batch → two effect runs");
}

console.log("\n== untrack ==");
{
    const [p, setP] = createSignal(0);
    const [q, setQ] = createSignal(0);
    let runs = 0;
    createEffect(() => { runs++; p(); untrack(() => q()); });
    check(runs === 1, "initial run");
    setQ(100);
    check(runs === 1, "untracked signal does not retrigger");
    setP(100);
    check(runs === 2, "tracked signal does retrigger");
}

console.log("\n== onCleanup + createRoot ==");
{
    let cleanups = 0;
    const dispose = createRoot((d) => {
        onCleanup(() => cleanups++);
        return d;
    });
    check(cleanups === 0, "cleanup not run before dispose");
    dispose();
    check(cleanups === 1, "cleanup runs on dispose");
    dispose();
    check(cleanups === 1, "dispose is idempotent (cleanup runs once)");
}

console.log("\n== nested effect disposal via root ==");
{
    const [s, setS] = createSignal(0);
    let effectRuns = 0;
    let cleaned = 0;
    const dispose = createRoot((d) => {
        createEffect(() => {
            effectRuns++;
            s();
            onCleanup(() => cleaned++);
        });
        return d;
    });
    check(effectRuns === 1, "effect created inside root runs");
    setS(1);
    check(effectRuns === 2 && cleaned === 1, "effect re-runs, prior cleanup fires");
    dispose();
    check(cleaned === 2, "root dispose runs the latest cleanup");
    setS(2);
    check(effectRuns === 2, "disposed effect stops tracking");
}

console.log("\n== render returns a working dispose ==");
{
    let mountedEffects = 0;
    let cleaned = 0;
    const [n, setN] = createSignal(0);
    const dispose = render(() => {
        createEffect(() => { mountedEffects++; n(); onCleanup(() => cleaned++); });
    });
    check(mountedEffects === 1, "effect inside render runs");
    setN(1);
    check(mountedEffects === 2 && cleaned === 1, "effect tracks signal");
    dispose();
    check(cleaned === 2, "render dispose cleans subtree");
    setN(2);
    check(mountedEffects === 2, "after dispose, no further re-runs");
}

console.log("\n== Show ==");
{
    const [vis, setVis] = createSignal(true);
    let mounted = 0, unmounted = 0;
    function Tracker() {
        mounted++;
        onCleanup(() => unmounted++);
        return h("view", {});
    }
    const dispose = render(() =>
        h("view", {}, h(Show, { when: vis }, () => h(Tracker)))
    );
    check(mounted === 1 && unmounted === 0, "child mounts when when=true");
    setVis(false);
    check(mounted === 1 && unmounted === 1, "child unmounts when when=false");
    setVis(true);
    check(mounted === 2 && unmounted === 1, "child re-mounts when when toggles back");
    dispose();
    check(unmounted === 2, "render dispose cleans Show subtree");
}

console.log("\n== Show with fallback ==");
{
    const [vis, setVis] = createSignal(true);
    let mainMounted = 0, mainCleaned = 0;
    let fbMounted = 0,   fbCleaned = 0;
    function Main()     { mainMounted++; onCleanup(() => mainCleaned++); return h("view", {}); }
    function Fallback() { fbMounted++;   onCleanup(() => fbCleaned++);   return h("view", {}); }
    const dispose = render(() =>
        h("view", {},
          h(Show, { when: vis, fallback: () => h(Fallback) }, () => h(Main))
        )
    );
    check(mainMounted === 1 && fbMounted === 0, "when=true → main, no fallback");
    setVis(false);
    check(mainCleaned === 1 && fbMounted === 1, "when=false → fallback, main cleaned");
    setVis(true);
    check(mainMounted === 2 && fbCleaned === 1, "back to main, fallback cleaned");
    dispose();
}

console.log("\n== For (keyed) ==");
{
    const [items, setItems] = createSignal([1, 2, 3]);
    let creates = 0, destroys = 0;
    function Item({ value }) {
        creates++;
        onCleanup(() => destroys++);
        return h("view", {});
    }
    const dispose = render(() =>
        h("view", {},
          h(For, { each: items, key: (it) => it }, (it) => h(Item, { value: it }))
        )
    );
    check(creates === 3 && destroys === 0, "3 items created initially");
    setItems([1, 2, 3, 4]);
    check(creates === 4 && destroys === 0, "append: only new item created (existing reused)");
    setItems([1, 2]);
    check(creates === 4 && destroys === 2, "shrink: 2 destroyed, none re-created");
    setItems([1, 2, 5, 6]);
    check(creates === 6 && destroys === 2, "extend with new keys: keep existing");
    setItems([6, 5]);
    check(creates === 6 && destroys === 4, "reorder + drop: no fresh creation for kept keys");
    dispose();
    check(destroys === 6, "render dispose cleans every remaining For row");
}

console.log("\n== pseudo-state styles (smoke) ==");
{
    const dispose = render(() => h("view", {},
        h("button", {
            style: {
                width: 100, height: 40,
                backgroundColor: "#3498db",
                borderRadius: 8,
                hover:    { backgroundColor: "#5dade2" },
                pressed:  { backgroundColor: "#2980b9" },
                focus:    { borderWidth: 2, borderColor: "#ffffff" },
                disabled: { backgroundColor: "#7f8c8d" },
            },
            text: "Hover me",
        })
    ));
    check(true, "mount button with hover/focus/pressed/disabled — no JS exception");
    dispose();
    check(true, "dispose pseudo-state button — no JS exception");
}

console.log("\n== Line / Table / Menu (smoke) ==");
{
    const dispose = render(() => h("view", {},
        h("line", {
            points: [[0, 0], [50, 30], [100, 10], [150, 40]],
            style: { width: 200, height: 60, borderColor: "#3498db", borderWidth: 2 },
        }),
        h("table", {
            rows: 2, cols: 2,
            cells: [["A1", "B1"], ["A2", "B2"]],
            style: { width: 200, height: 80 },
        }),
        h("menu", { style: { width: 300, height: 200 } },
            h("menuPage", { title: "Main" },
                h("text", { text: "Welcome" }),
            ),
            h("menuPage", { title: "Settings" },
                h("text", { text: "Configuration" }),
            ),
        ),
    ));
    check(true, "mount line/table/menu/menuPage — no JS exception");
    dispose();
    check(true, "dispose line/table/menu — no JS exception");
}

console.log(`\n${pass} passed, ${fail} failed`);
console.log(fail === 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
