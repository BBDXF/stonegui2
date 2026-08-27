/**
 * app.js — framework reactive primitives smoke test
 *
 * Run:  ./build/stonegui --no-watch examples/test/app.js
 *
 * Prints PASS / FAIL for each primitive, then a final summary. The bundle
 * exits 0 when every assertion passes and 1 when any fails (no SIGTERM
 * needed anymore). CI greps for the summary line and the exit code; humans
 * read the per-test log. Set STONEGUI_TEST_FORCE_FAIL=1 to force a failing
 * exit for negative-path CI coverage without touching any real assertion.
 */

import {
    createSignal, createEffect, createMemo, createRoot,
    onCleanup, untrack, batch, render, h, Show, For,
    clipboard, findCjkFont, getProperty, loadFont, loadImage, loadImages,
    moduleDir, focus, sendKey, setDefaultFont, setTheme, setThemeToken,
    showMsgbox, updateLayout,
} from "../../js/framework.js";

/* The msgbox builds sub-objects LVGL never hands back, so reaching its
 * backdrop / header / content / footer needs the native tree getters. */
import * as lv from "lvgl";

/* QuickJS native std module (registered by main.c via js_init_module_std):
 * std.exit(status) terminates with a truthful exit code; std.getenv(name)
 * returns the env value string or undefined. */
import * as std from "std";

const EXAMPLES = moduleDir(import.meta.url) + "/..";

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

console.log("\n== clipboard ==");
{
    const dispose = render(() => h("view", {}));

    clipboard.write("test-stonegui-clip");
    check(clipboard.read() === "test-stonegui-clip", "clipboard write+read round-trip");
    clipboard.write("");  /* clear */
    dispose();
}

console.log("\n== cursorPos via getProperty ==");
{
    let inputRef = null;
    const dispose = render(() =>
        h("view", {},
          h("input", {
              ref: (n) => inputRef = n,
              style: { width: 200, height: 40 },
          })
        )
    );
    const pos = getProperty(inputRef, "cursorPos");
    check(typeof pos === "number", "cursorPos getProperty returns a number");
    dispose();
}

console.log("\n== D-E smoke (arrowHeader / arcAngle / keyboard) ==");
{
    let kbInputRef = null;
    let kbRef = null;
    const dispose = render(() =>
        h("view", { style: { width: "100%", height: "100%" } },
            h("calendar", { arrowHeader: false, style: { width: 300, height: 280 } }),
            h("spinner", { arcAngle: 270, style: { width: 60, height: 60 } }),
            h("input", { ref: (n) => kbInputRef = n, style: { width: 200, height: 40 } }),
            h("keyboard", {
                ref: (n) => kbRef = n,
                /* Thunk, not `kbInputRef`: props are built before render, when
                 * the input's ref has not fired yet. A reactive accessor defers
                 * the read to the keyboard's own mount effect. */
                target: () => kbInputRef,
                mode: "number",
                style: { width: "100%", height: 200 },
            }),
        )
    );
    check(true, "calendar without arrowHeader mounts without crash");
    check(true, "spinner with custom arcAngle=270 mounts without crash");
    check(getProperty(kbRef, "target") === kbInputRef,
          "keyboard target is bound to the input textarea");
    dispose();
    check(true, "D-E dispose cleans up without crash");
}

console.log("\n== theme scheme + token swap (A phase) ==");
{
    let btn = null, led = null, slider = null, spanInButton = null, label = null, list = null;
    let inlineFont = null, textInButton = null;
    let input = null, dropdown = null, table = null, chart = null;
    let sw = null, progress = null;
    const dispose = render(() =>
        h("view", { style: { width: "100%", height: "100%" } },
            h("button", { ref: (n) => btn = n, text: "themed" }),
            h("led", { ref: (n) => led = n, style: { width: 20, height: 20 } }),
            h("slider", { ref: (n) => slider = n, style: { width: 100 } }),
            h("switch", { ref: (n) => sw = n }),
            h("progress", { ref: (n) => progress = n, value: 50 }),
            h("input", { ref: (n) => input = n }),
            h("dropdown", { ref: (n) => dropdown = n, options: "one\ntwo" }),
            h("table", { ref: (n) => table = n, rows: 1, cols: 1 }),
            h("chart", { ref: (n) => chart = n }),
            h("text", { ref: (n) => label = n, text: "plain label" }),
            h("text", { ref: (n) => inlineFont = n, text: "inline font",
                        style: { fontSize: 24 } }),
            h("list", { ref: (n) => list = n, style: { width: 100, height: 60 } }),
            h("button", {},
                h("span", { ref: (n) => spanInButton = n,
                            style: { width: 100, height: 30 } }),
            ),
            h("button", {},
                h("text", { ref: (n) => textInButton = n, text: "caption" }),
            ),
        )
    );

    const inlineFontHeight = getProperty(inlineFont, "fontLineHeight");
    check(getProperty(btn, "backgroundColor") === "#409eff",
          "button picks up the Element Plus primary token by default");
    check(getProperty(slider, "backgroundColor", { part: "knob" }) === "#ffffff",
          "selector-aware getter resolves the slider knob colour");
    check(getProperty(slider, "backgroundColor", { part: "indicator" }) === "#409eff",
          "selector-aware getter resolves the slider indicator colour");
    /* LVGL's LED draw path does not expose its post-mix paint. It uses the
     * resolved style bg_color brightness as a multiplier for led->color, so
     * this max-brightness white base is the observable regression seam; the
     * resulting assigned-colour paint is verified by the visual capture gate. */
    check(getProperty(led, "backgroundColor") === "#ffffff",
          "led base fill stays max-brightness so LVGL paints its assigned colour");
    check(getProperty(led, "bgOpa") === 255,
          "led base fill stays opaque so its assigned colour is visible");
    check(getProperty(btn, "padding") === 9,
          "button vertical padding matches the locked btn_pad_ver token");

    updateLayout(btn);
    const buttonWidth = getProperty(btn, "width");
    setThemeToken("btn_pad_hor", 24);
    updateLayout(btn);
    check(getProperty(btn, "width") === buttonWidth + 16,
          "btn_pad_hor token patch changes resolved button width");
    setThemeToken("btn_pad_hor", 16);
    updateLayout(btn);
    check(getProperty(btn, "width") === buttonWidth,
          "restoring btn_pad_hor restores resolved button width");

    setThemeToken("btn_pad_ver", 13);
    check(getProperty(btn, "padding") === 13,
          "btn_pad_ver token patch changes resolved button padding");
    setThemeToken("btn_pad_ver", 9);
    check(getProperty(btn, "padding") === 9,
          "restoring btn_pad_ver restores resolved button padding");

    setThemeToken("border_width", 3);
    check(getProperty(input, "borderWidth") === 3 &&
          getProperty(input, "borderWidth", { state: "focus" }) === 6 &&
          getProperty(dropdown, "borderWidth") === 3 &&
          getProperty(table, "borderWidth") === 3 &&
          getProperty(chart, "borderWidth") === 3,
          "border_width token patch changes field, focused field, dropdown, table, and chart borders");
    setThemeToken("border_width", 1);
    check(getProperty(input, "borderWidth") === 1 &&
          getProperty(input, "borderWidth", { state: "focus" }) === 2 &&
          getProperty(dropdown, "borderWidth") === 1 &&
          getProperty(table, "borderWidth") === 1 &&
          getProperty(chart, "borderWidth") === 1,
          "restoring border_width restores field, focused field, dropdown, table, and chart borders");

    /* Disabled button styling is introduced by Todo 8. For now this only
     * proves that the resolved disabled-state selector is observable. */
    const disabledColor = getProperty(btn, "backgroundColor", { state: "disabled" });
    check(/^#[0-9a-f]{6}$/.test(disabledColor),
          "disabled-state getter returns a valid resolved colour");

    let invalidSelectorThrew = false;
    try {
        getProperty(slider, "backgroundColor", { part: "bogus" });
    } catch (error) {
        invalidSelectorThrew = error instanceof TypeError;
    }
    check(invalidSelectorThrew, "invalid resolved-style selector throws TypeError");

    const rawGetterFailures = [
        ["unknown key", () => lv.getProperty(btn, "bogusKey")],
        ["unknown part", () => lv.getProperty(slider, "backgroundColor", { part: "bogus" })],
        ["unknown state", () => lv.getProperty(btn, "backgroundColor", { state: "bogus" })],
        ["numeric part", () => lv.getProperty(slider, "backgroundColor", { part: 123 })],
        ["boolean state", () => lv.getProperty(btn, "backgroundColor", { state: false })],
        ["null part", () => lv.getProperty(slider, "backgroundColor", { part: null })],
    ];
    for (const [selectorKind, read] of rawGetterFailures) {
        let threw = false;
        try {
            read();
        } catch (error) {
            threw = error instanceof TypeError;
        }
        check(threw, `raw lv.getProperty rejects ${selectorKind}`);
    }

    setThemeToken("primary", "#e74c3c");
    check(getProperty(btn, "backgroundColor") === "#e74c3c",
          "setThemeToken rebuilds shared styles (was a silent no-op)");

    let unknownTokenThrew = false;
    try {
        setThemeToken("bogus_name_xyz", "#123456");
    } catch (error) {
        unknownTokenThrew = error instanceof TypeError;
    }
    check(unknownTokenThrew, "setThemeToken rejects unknown token names");

    let colorForIntThrew = false;
    try {
        setThemeToken("radius_btn", "#123456");
    } catch (error) {
        colorForIntThrew = error instanceof TypeError;
    }
    check(colorForIntThrew, "setThemeToken rejects a color for an integer token");

    let intForColorThrew = false;
    try {
        setThemeToken("primary", 42);
    } catch (error) {
        intForColorThrew = error instanceof TypeError;
    }
    check(intForColorThrew, "setThemeToken rejects a number for a color token");

    setTheme("dark");
    check(getProperty(btn, "backgroundColor") === "#409eff",
          "setTheme('dark') replaces the whole token set");
    check(getProperty(led, "shadowWidth") === 8,
          "led uses sg_theme's flat 8px glow, not the default theme's 15px (A4)");
    /* Dark on_surface is text_primary per doc/theme.md §9.2. */
    check(getProperty(spanInButton, "textColor") === "#e5eaf3",
          "span inside a button keeps on_surface instead of the button's on_primary (A4)");
    check(getProperty(label, "textColor") === "#e5eaf3",
          "plain label text follows the dark token (was stuck at the default theme's #212121)");
    check(getProperty(textInButton, "textColor") === "#ffffff",
          "a label inside a button inherits on_primary, not on_surface (dark)");
    check(getProperty(list, "textColor") === "#e5eaf3",
          "list text follows the dark token instead of vanishing into the dark surface");
    check(getProperty(list, "backgroundColor") === "#1d1e1f",
          "list surface follows the dark token");

    setTheme("light");
    check(getProperty(btn, "backgroundColor") === "#409eff",
          "setTheme('light') restores the light preset");

    setThemeToken("radius_round", 31);
    const radiusPanel = showMsgbox({ title: "R", text: "radius", buttons: ["OK"] });
    const radiusBackdrop = lv.getParent(radiusPanel);
    const radiusContent = lv.getChild(radiusPanel, 1);
    check(getProperty(sw, "radius") === 31 &&
          getProperty(sw, "radius", { part: "knob" }) === 31 &&
          getProperty(progress, "radius") === 31 &&
          getProperty(progress, "radius", { part: "indicator" }) === 31 &&
          getProperty(slider, "radius", { part: "knob" }) === 31 &&
          getProperty(led, "radius") === 31,
          "radius_round token patch reaches switch, bar, knob, and LED circles");
    check(getProperty(input, "radius", { part: "scrollbar" }) === 31 &&
          getProperty(chart, "radius", { part: "indicator" }) === 31 &&
          getProperty(radiusContent, "radius", { part: "scrollbar" }) === 31,
          "radius_round token patch reaches shared and msgbox-style scrollbars plus chart markers");
    setThemeToken("radius_round", 20);
    check(getProperty(sw, "radius") === 20 &&
          getProperty(sw, "radius", { part: "knob" }) === 20 &&
          getProperty(progress, "radius") === 20 &&
          getProperty(progress, "radius", { part: "indicator" }) === 20 &&
          getProperty(slider, "radius", { part: "knob" }) === 20 &&
          getProperty(led, "radius") === 20 &&
          getProperty(input, "radius", { part: "scrollbar" }) === 20 &&
          getProperty(chart, "radius", { part: "indicator" }) === 20 &&
          getProperty(radiusContent, "radius", { part: "scrollbar" }) === 20,
          "restoring radius_round restores every circular theme role");
    lv.dispose(radiusBackdrop);
    for (let i = 0; i < 20; i++) {
        setTheme(i % 2 === 0 ? "dark" : "light");
    }
    check(getProperty(btn, "backgroundColor") === "#409eff",
          "style registry survives a 20-cycle scheme-switch stress test");
    check(getProperty(spanInButton, "textColor") === "#303133",
          "span text colour follows the active scheme");
    check(getProperty(label, "textColor") === "#303133",
          "plain label text returns to the light token");
    check(inlineFontHeight > 0 &&
          getProperty(inlineFont, "fontLineHeight") === inlineFontHeight,
          "inline font style survives repeated parentless scheme changes");
    check(getProperty(textInButton, "textColor") === "#ffffff",
          "a label inside a button stays on_primary across a scheme swap (light)");

    const themeSource = std.loadFile(`${EXAMPLES}/../src/sg_theme.c`);
    check(/lv_style_set_length\(&st_scale_indic,\s*t->space_sm\);/.test(themeSource) &&
          /lv_style_set_length\(&st_scale_items,\s*t->space_xs\);/.test(themeSource),
          "scale tick tiers consume space_sm and space_xs as visible lengths");
    /* BG_IMAGE_SRC is consumed directly by lv_checkbox's draw descriptor and
     * has no resolved-style getter in stonegui. Pin the style base here; the
     * real symbol pixels (including CJK-font fallback) are a visual gate. */
    check(/lv_style_set_bg_image_src\(&st_cb_box_checked,\s*LV_SYMBOL_OK\);/.test(themeSource),
          "checked checkbox style base supplies LV_SYMBOL_OK");
    check(/lv_style_set_line_rounded\(&st_line,\s*true\);/.test(themeSource),
          "line style base enables rounded caps");
    check(!/lv_obj_add_style\([^;]*&st_calendar_today,[^;]*LV_STATE_CHECKED[^;]*\);/.test(themeSource),
          "calendar does not claim a dead ITEMS|CHECKED today selector");
    dispose();
}

console.log("\n== foundational control states + parts (doc/theme.md §7, §8) ==");
{
    let btn = null, input = null, sw = null, slider = null, arc = null;
    let spinner = null, cb = null, dd = null, roller = null, spinbox = null;
    let view = null;
    const dispose = render(() =>
        h("view", { ref: (n) => view = n,
                    style: { width: "100%", height: "100%" } },
            h("button", { ref: (n) => btn = n, text: "states" }),
            h("input", { ref: (n) => input = n, placeholder: "ghost" }),
            h("switch", { ref: (n) => sw = n }),
            h("slider", { ref: (n) => slider = n, style: { width: 100 } }),
            h("arc", { ref: (n) => arc = n, style: { width: 60, height: 60 } }),
            h("spinner", { ref: (n) => spinner = n, style: { width: 40, height: 40 } }),
            h("checkbox", { ref: (n) => cb = n, text: "agree" }),
            h("dropdown", { ref: (n) => dd = n, options: "a\nb\nc" }),
            h("roller", { ref: (n) => roller = n, options: "x\ny\nz" }),
            h("spinbox", { ref: (n) => spinbox = n }),
        )
    );

    /* Row 3 — button. §7: hover primary.light_3, pressed primary.dark_2,
     * disabled text_disabled / fill_light / border_light. */
    check(getProperty(btn, "backgroundColor", { state: "hover" }) === "#79bbff",
          "button hover fill is primary.light_3");
    check(getProperty(btn, "backgroundColor", { state: "pressed" }) === "#337ecc",
          "button pressed fill is primary.dark_2");
    check(getProperty(btn, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "button disabled fill is fill_light");
    check(getProperty(btn, "textColor", { state: "disabled" }) === "#c0c4cc",
          "button disabled caption is text_disabled");
    check(getProperty(btn, "borderColor", { state: "disabled" }) === "#e4e7ed",
          "button disabled border is border_light");
    check(getProperty(btn, "backgroundColor", { state: "checked" }) === "#337ecc",
          "button checked fill is the engaged primary.dark_2 accent");

    /* Row 1 — view/lv_obj scrollbar, previously unstyled everywhere. */
    check(getProperty(view, "backgroundColor", { part: "scrollbar" }) === "#d4d7de",
          "view scrollbar thumb is border_dark");

    /* Row 5 — input/textarea. */
    check(getProperty(input, "borderColor", { state: "hover" }) === "#cdd0d6",
          "input hover border is border_darker");
    check(getProperty(input, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "input disabled surface is fill_light");
    check(getProperty(input, "backgroundColor", { part: "scrollbar" }) === "#d4d7de",
          "input scrollbar thumb is border_dark");
    check(getProperty(input, "backgroundColor",
                      { part: "cursor", state: "focus" }) === "#409eff",
          "input cursor is primary — and only while focused");
    check(getProperty(input, "backgroundColor", { part: "cursor" }) !== "#409eff",
          "input cursor is NOT styled in the default state");

    /* Row 18 — spinbox styles the digit highlight in DEFAULT, not FOCUSED. */
    check(getProperty(spinbox, "backgroundColor", { part: "cursor" }) === "#409eff",
          "spinbox digit cursor is primary in the DEFAULT state (unlike input)");
    check(getProperty(spinbox, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "spinbox disabled surface is fill_light");

    /* Row 6 — switch. */
    check(getProperty(sw, "backgroundColor",
                      { part: "indicator", state: "disabled" }) === "#c0c4cc",
          "switch disabled indicator is the text_disabled accent");
    check(getProperty(sw, "bgOpa", { part: "indicator" }) === 0,
          "switch indicator is transparent while unchecked");
    check(getProperty(sw, "backgroundColor",
                      { part: "indicator", state: "checked" }) === "#409eff" &&
          getProperty(sw, "bgOpa", { part: "indicator", state: "checked" }) === 255,
          "switch checked indicator owns the primary opaque track");
    check(getProperty(sw, "backgroundColor",
                      { part: "knob", state: "disabled" }) === "#f0f2f5",
          "switch disabled knob is fill_base");
    check(getProperty(sw, "backgroundColor", { part: "knob" }) === "#ffffff",
          "switch knob stays white in the default state");

    /* Row 8 — slider. */
    check(getProperty(slider, "backgroundColor",
                      { part: "indicator", state: "disabled" }) === "#c0c4cc",
          "slider disabled indicator is the text_disabled accent");
    check(getProperty(slider, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "slider disabled track is fill_light");
    check(getProperty(slider, "borderColor", { part: "knob" }) === "#409eff",
          "slider knob sits on a primary rim");
    check(getProperty(slider, "borderColor",
                      { part: "knob", state: "pressed" }) === "#337ecc",
          "slider knob rim darkens while pressed");
    check(getProperty(slider, "backgroundColor",
                      { part: "knob", state: "disabled" }) === "#f0f2f5",
          "slider disabled knob is fill_base");

    /* Row 9 — arc. */
    check(getProperty(arc, "arcColor",
                      { part: "indicator", state: "disabled" }) === "#c0c4cc",
          "arc disabled indicator is the text_disabled accent");

    /* Row 10 — a spinner has no draggable handle. */
    check(getProperty(spinner, "bgOpa", { part: "knob" }) === 0,
          "spinner knob is explicitly invisible");
    check(getProperty(arc, "bgOpa", { part: "knob" }) === 255,
          "an arc knob stays visible — the spinner rule is spinner-only");

    /* Row 11 — checkbox. */
    check(getProperty(cb, "textColor", { state: "checked" }) === "#409eff",
          "checkbox caption turns primary when checked");
    check(getProperty(cb, "textColor", { state: "disabled" }) === "#c0c4cc",
          "checkbox caption is text_disabled when disabled");
    check(getProperty(cb, "borderColor",
                      { part: "indicator", state: "hover" }) === "#409eff",
          "checkbox box border turns primary on hover");
    check(getProperty(cb, "backgroundColor",
                      { part: "indicator", state: "pressed" }) === "#ecf5ff",
          "checkbox box fills primary.light_9 while pressed");
    check(getProperty(cb, "backgroundColor",
                      { part: "indicator", state: "disabled" }) === "#f5f7fa",
          "checkbox box is fill_light when disabled");
    check(getProperty(cb, "textColor",
                      { part: "indicator", state: "checked" }) === "#ffffff",
          "checked checkbox tick is white on the primary fill");
    check(getProperty(cb, "radius", { part: "indicator" }) === 2,
          "checkbox box uses radius_small, not a bare 4");

    /* Row 12 — the popup list is a separate lv_dropdownlist object, which is
     * where LVGL actually reads SELECTED/SCROLLBAR from. */
    check(getProperty(dd, "backgroundColor", { part: "selected" }) === "#f5f7fa",
          "dropdown popup row is the fill_light neutral hover surface");
    check(getProperty(dd, "backgroundColor",
                      { part: "selected", state: "checked" }) === "#ecf5ff",
          "dropdown popup selected row is primary.light_9");
    check(getProperty(dd, "textColor",
                      { part: "selected", state: "checked" }) === "#409eff",
          "dropdown popup selected row text is primary");
    check(getProperty(dd, "backgroundColor",
                      { part: "selected", state: "pressed" }) === "#c6e2ff",
          "dropdown popup row deepens to primary.light_7 while pressed");
    check(getProperty(dd, "backgroundColor", { part: "scrollbar" }) === "#d4d7de",
          "dropdown popup scrollbar is styled on the popup object");
    check(getProperty(dd, "textColor", { part: "indicator" }) === "#a8abb2",
          "dropdown arrow is text_placeholder");
    check(getProperty(dd, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "dropdown disabled surface is fill_light");

    /* Row 13 — roller. */
    check(getProperty(roller, "backgroundColor", { part: "selected" }) === "#409eff",
          "roller selected band is primary");
    check(getProperty(roller, "backgroundColor",
                      { part: "selected", state: "checked" }) === "#337ecc",
          "roller selected+checked band is the engaged primary.dark_2 accent");
    check(getProperty(roller, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "roller disabled surface is fill_light");

    /* The whole point of §4: light_3 darkens and dark_2 brightens in dark mode,
     * so the same token names give correct-feeling states in both schemes. */
    setTheme("dark");
    check(getProperty(btn, "backgroundColor", { state: "hover" }) === "#3375b9",
          "button hover follows primary.light_3 into the dark scheme");
    check(getProperty(btn, "backgroundColor", { state: "pressed" }) === "#66b1ff",
          "button pressed brightens to the dark primary.dark_2");
    check(getProperty(cb, "textColor", { state: "checked" }) === "#409eff",
          "checked checkbox caption stays primary in dark mode");
    check(getProperty(spinner, "bgOpa", { part: "knob" }) === 0,
          "spinner knob stays invisible across a scheme swap");
    setTheme("light");

    /* The single most fragile invariant in sg_theme.c: labels must inherit. */
    check(getProperty(btn, "textColor") === "#ffffff",
          "button caption still inherits on_primary after the state additions");
    dispose();
}

console.log("\n== live theme color references + partStyles ==");
{
    let primaryBox = null, literalBox = null, hexBox = null, slider = null;
    const dispose = render(() =>
        h("view", {},
            h("view", { ref: (n) => primaryBox = n,
                        style: { backgroundColor: "$primary", width: 20, height: 20 } }),
            h("view", { ref: (n) => literalBox = n,
                        style: { backgroundColor: "blue", width: 20, height: 20 } }),
            h("view", { ref: (n) => hexBox = n,
                        style: { backgroundColor: "#123456", width: 20, height: 20 } }),
            h("slider", {
                ref: (n) => slider = n,
                style: {
                    width: 100,
                    partStyles: {
                        main: { backgroundColor: "$surface" },
                        knob: {
                            backgroundColor: "$surface",
                            pressed: { borderColor: "$primary" },
                        },
                        indicator: {
                            backgroundColor: "$primary",
                            disabled: { backgroundColor: "$bg_overlay" },
                        },
                    },
                },
            }),
        )
    );

    check(lv.getThemeToken("primary") === "#409eff",
          "native getThemeToken returns a normalized registry colour");
    check(getProperty(primaryBox, "backgroundColor") === "#409eff",
          "$primary resolves on a main-part style");
    check(getProperty(literalBox, "backgroundColor") === "#0000ff",
          "raw named color blue remains a literal");
    check(getProperty(hexBox, "backgroundColor") === "#123456",
          "non-token local hex resolves literally");
    check(getProperty(slider, "backgroundColor", { part: "knob" }) === "#ffffff",
          "partStyles applies a knob style");
    check(getProperty(slider, "backgroundColor", { part: "indicator" }) === "#409eff",
          "partStyles applies an indicator style");
    check(getProperty(slider, "borderColor", { part: "knob", state: "pressed" }) === "#409eff",
          "partStyles routes a nested pressed state with its part");
    check(getProperty(slider, "backgroundColor", { part: "indicator", state: "disabled" }) === "#ffffff",
          "partStyles routes a nested disabled state with its part");

    setTheme("dark");
    check(getProperty(slider, "backgroundColor", { part: "main" }) === "#1d1e1f" &&
          getProperty(slider, "backgroundColor", { part: "knob" }) === "#1d1e1f",
          "$surface part styles update live after setTheme");
    check(getProperty(slider, "backgroundColor", { part: "indicator", state: "disabled" }) === "#1d1e1f",
          "$bg_overlay nested part state updates live after setTheme");
    check(getProperty(literalBox, "backgroundColor") === "#0000ff" &&
          getProperty(hexBox, "backgroundColor") === "#123456",
          "literal named and hex colors remain stable across scheme changes");

    setThemeToken("primary", "#e74c3c");
    check(getProperty(primaryBox, "backgroundColor") === "#e74c3c" &&
          getProperty(slider, "backgroundColor", { part: "indicator" }) === "#e74c3c" &&
          getProperty(slider, "borderColor", { part: "knob", state: "pressed" }) === "#e74c3c",
          "$primary styles on main and sub-parts update live after token patch");

    let unknownReferenceThrew = false;
    try {
        render(() => h("view", { style: { backgroundColor: "$not_a_theme_token" } }));
    } catch (error) {
        unknownReferenceThrew = error instanceof TypeError;
        console.log("  PROBE unknown token:", String(error));
    }
    check(unknownReferenceThrew, "unknown $theme token throws TypeError");

    let nonColorTokenThrew = false;
    try {
        lv.getThemeToken("radius_base");
    } catch (error) {
        nonColorTokenThrew = error instanceof TypeError;
        console.log("  PROBE non-color token:", String(error));
    }
    check(nonColorTokenThrew, "native getThemeToken rejects non-color registry entries");

    let bogusPartThrew = false;
    try {
        render(() => h("slider", { style: {
            partStyles: { bogus: { backgroundColor: "#ffffff" } },
        } }));
    } catch (error) {
        bogusPartThrew = error instanceof TypeError;
        console.log("  PROBE bogus part:", String(error));
    }
    check(bogusPartThrew, "partStyles rejects a bogus part with TypeError");

    let bogusStateThrew = false;
    try {
        render(() => h("slider", { style: {
            partStyles: { knob: { bogus: { backgroundColor: "#ffffff" } } },
        } }));
    } catch (error) {
        bogusStateThrew = error instanceof TypeError;
        console.log("  PROBE bogus state:", String(error));
    }
    check(bogusStateThrew, "partStyles rejects a bogus nested state with TypeError");

    setTheme("light");
    dispose();
}

console.log("\n== layout: measurement + scroll clipping ==");
{
    let root = null, box = null, scrollBox = null;
    let inside = null, bleed = null, clipped = null;
    const dispose = render(() =>
        h("view", { ref: (n) => root = n,
                    style: { width: "100%", height: "100%", flexFlow: "row" } },
            h("view", { ref: (n) => box = n,
                        style: { width: 120, height: 60 } },
                h("text", { ref: (n) => inside = n, text: "in",  style: { x: 0, y: 10 } }),
                h("text", { ref: (n) => bleed  = n, text: "out", style: { x: 0, y: 70 } }),
            ),
            h("view", { ref: (n) => scrollBox = n, scrollable: true,
                        style: { width: 120, height: 60 } },
                h("text", { ref: (n) => clipped = n, text: "out", style: { x: 0, y: 70 } }),
            ),
        )
    );
    updateLayout(root);

    check(getProperty(box, "width") === 120 && getProperty(box, "height") === 60,
          "updateLayout flushes geometry so width/height read back (0 before the flush)");
    check(getProperty(scrollBox, "scrollable") === true,
          "scrollable=true is reflected by getProperty");
    check(getProperty(inside, "visible") === true,
          "a child inside the box is visible");
    check(getProperty(bleed, "visible") === true,
          "a plain View keeps OVERFLOW_VISIBLE so knobs past the edge still draw");
    check(getProperty(clipped, "visible") === false,
          "a scrollable View clips instead of bleeding ext_draw_size past its box");
    dispose();
}

console.log("\n== layout: size parsing ==");
{
    let root = null, auto = null, pct = null, fill = null, junk = null;
    const dispose = render(() =>
        h("view", { ref: (n) => root = n, style: { width: 400, height: 200 } },
            h("text", { ref: (n) => auto = n, text: "auto", style: { width: "auto" } }),
            h("view", { ref: (n) => pct  = n, style: { width: "50%", height: 20 } }),
            h("view", { ref: (n) => fill = n, style: { width: "fill", height: 20 } }),
            h("text", { ref: (n) => junk = n, text: "junk", style: { width: "potato" } }),
        )
    );
    updateLayout(root);

    check(getProperty(pct, "width") === 200,  "\"50%\" resolves against the parent");
    check(getProperty(fill, "width") === 400, "\"fill\" resolves to the full parent width");
    check(getProperty(auto, "width") > 0,     "\"auto\" hugs the text instead of collapsing");
    check(getProperty(junk, "width") > 0,
          "an unparseable size falls back to auto instead of a silent 0-width widget");
    dispose();
}

console.log("\n== Ctrl shortcuts + focus (C phase) ==");
{
    let a = null, b = null;
    const dispose = render(() =>
        h("view", { style: { width: "100%", height: "100%" } },
            h("input", { ref: (n) => a = n, text: "round-trip-payload",
                         style: { width: 200, height: 40 } }),
            h("input", { ref: (n) => b = n, style: { width: 200, height: 40 } }),
        )
    );

    check(focus(a), "focus(input A) reports the widget as focused");

    clipboard.write("");
    sendKey("a", true);
    sendKey("c", true);
    check(clipboard.read() === "round-trip-payload",
          "Ctrl+A then Ctrl+C copies the focused input's text");

    check(focus(b), "focus(input B) moves focus");
    sendKey("v", true);
    check(getProperty(b, "text") === "round-trip-payload",
          "Ctrl+V pastes the clipboard into the newly focused input");

    sendKey("x", true);
    check(getProperty(b, "text") === "",
          "Ctrl+X clears the focused input");

    focus(a);
    sendKey("home", false);
    check(getProperty(a, "cursorPos") === 0, "Home moves the cursor to 0");
    sendKey("end", false);
    check(getProperty(a, "cursorPos") === "round-trip-payload".length,
          "End moves the cursor to the last position");

    clipboard.write("");
    dispose();
}

console.log("\n== Image handle system ==");
{
    const validPath   = `${EXAMPLES}/showcase/assets/test.png`;
    const missingPath = "/no/such/file/__definitely_does_not_exist__.png";

    const validHandle   = loadImage(validPath);
    check(typeof validHandle === "number" && validHandle !== 0,
          "loadImage returns truthy handle for existing file");

    const missingHandle = loadImage(missingPath);
    check(missingHandle === 0, "loadImage returns 0 for missing file");

    const batch2 = loadImages([validPath, missingPath, validPath]);
    check(Array.isArray(batch2) && batch2.length === 3,
          "loadImages returns array of correct length");
    check(batch2[0] !== 0 && batch2[2] !== 0, "loadImages valid entries truthy");
    check(batch2[1] === 0, "loadImages missing entry is zero");

    const handleAgain = loadImage(validPath);
    check(typeof handleAgain === "number" && handleAgain !== 0,
          "loadImage on same path returns a usable handle");
}

console.log("\n== Font handle catalog ==");
{
    const cjkPath = findCjkFont();
    let plain = null, buttonCaption = null;
    let explicit14 = null, explicit16 = null, explicit20 = null, explicit24 = null;
    if (cjkPath) {
        const font14 = loadFont(cjkPath, 14);
        const font14Again = loadFont(cjkPath, 14);
        const font16 = loadFont(cjkPath, 16);
        const font20 = loadFont(cjkPath, 20);
        const font24 = loadFont(cjkPath, 24);

        check(font14 !== 0 && font14 === font14Again,
              "loadFont reuses the same handle for an identical path and size");
        check(font16 !== 0 && font16 !== font14,
              "loadFont creates a distinct usable handle for another size");

        setDefaultFont();
        const dispose = render(() => h("view", {},
            h("text", { ref: (n) => plain = n, text: "默认字体" }),
            h("button", { text: "按钮字体",
                          ref: (n) => buttonCaption = lv.getChild(n, 0) }),
            h("text", { ref: (n) => explicit14 = n, text: "十四",
                        style: { font: font14 } }),
            h("text", { ref: (n) => explicit16 = n, text: "十六",
                        style: { font: font16 } }),
            h("text", { ref: (n) => explicit20 = n, text: "二十",
                        style: { font: font20 } }),
            h("text", { ref: (n) => explicit24 = n, text: "二十四",
                        style: { font: font24 } }),
        ));
        const height14 = getProperty(explicit14, "fontLineHeight");
        const height16 = getProperty(explicit16, "fontLineHeight");
        const height20 = getProperty(explicit20, "fontLineHeight");
        const height24 = getProperty(explicit24, "fontLineHeight");
        check(font20 !== 0 && font24 !== 0 &&
              new Set([height14, height16, height20, height24]).size === 4,
              "four-size font map uses four real cached role handles");
        check(getProperty(plain, "fontLineHeight") > 0 &&
              getProperty(plain, "fontLineHeight") === height14,
              "parentless theme gives plain text the CJK font.base role");

        setDefaultFont(font16);
        check(getProperty(plain, "fontLineHeight") === height16 &&
              getProperty(buttonCaption, "fontLineHeight") === height16,
              "single-handle default font repaints live label and button caption");

        setDefaultFont({14: font14, 16: font16, 20: font20, 24: font24});
        check(getProperty(plain, "fontLineHeight") === height14 &&
              getProperty(buttonCaption, "fontLineHeight") === height14,
              "four-role map repaints live label and button caption to font.base");

        setDefaultFont(font16);
        check(getProperty(plain, "fontLineHeight") === height16 &&
              getProperty(buttonCaption, "fontLineHeight") === height16,
              "restoring one role font repaints both live text paths back");
        dispose();
    } else {
        const dispose = render(() => h("text", {
            ref: (n) => plain = n,
            text: "built-in fallback",
        }));
        check(getProperty(plain, "fontLineHeight") > 0,
              "parentless theme gives plain text a sane built-in fallback font");
        dispose();
        check(true, "SKIP CJK font unavailable - handle-reuse test not run");
    }
}

console.log("\n== <image src> accepts handle ==");
{
    const handle = loadImage(`${EXAMPLES}/showcase/assets/test.png`);
    const dispose = render(() => h("view", {},
        h("image", { src: handle, style: { width: 32, height: 32 } }),
        h("image", { src: `A:${EXAMPLES}/showcase/assets/test.png`,
                     style: { width: 32, height: 32 } }),
    ));
    check(true, "<image> mounts with both handle and string src — no JS exception");
    dispose();
    check(true, "<image> dispose — no JS exception");
}

console.log("\n== <animimg> + <imagebutton> (smoke) ==");
{
    const frames = loadImages([
        `${EXAMPLES}/showcase/assets/frame1.png`,
        `${EXAMPLES}/showcase/assets/frame2.png`,
        `${EXAMPLES}/showcase/assets/frame3.png`,
    ]);
    const released = loadImage(`${EXAMPLES}/showcase/assets/released.png`);
    const pressed  = loadImage(`${EXAMPLES}/showcase/assets/pressed.png`);

    check(frames.every((f) => f !== 0), "animimg frames all loaded");
    check(released !== 0 && pressed !== 0, "imagebutton state PNGs loaded");

    const dispose = render(() => h("view", {},
        h("animimg", {
            src: frames,
            duration: 500,
            repeat: "infinite",
            start: true,
            style: { width: 64, height: 64 },
        }),
        h("imagebutton", {
            released: released,
            pressed: pressed,
            checkable: true,
            style: { width: 64, height: 64 },
        }),
    ));
    check(true, "mount animimg + imagebutton — no JS exception");
    dispose();
    check(true, "dispose animimg + imagebutton — no JS exception");
}

console.log("\n== image family theming (doc/theme.md §8 rows 4, 30, 31) ==");
{
    const released = loadImage(`${EXAMPLES}/showcase/assets/released.png`);
    const frames   = loadImages([
        `${EXAMPLES}/showcase/assets/frame1.png`,
        `${EXAMPLES}/showcase/assets/frame2.png`,
    ]);

    let img = null, anim = null, ibtn = null;
    const dispose = render(() => h("view", {},
        h("image", { src: released, ref: (n) => { img = n; },
                     style: { width: 32, height: 32 } }),
        h("animimg", { src: frames, duration: 400, repeat: "infinite",
                       ref: (n) => { anim = n; },
                       style: { width: 32, height: 32 } }),
        h("imagebutton", { released: released, checkable: true,
                           ref: (n) => { ibtn = n; },
                           style: { width: 32, height: 32 } }),
    ));

    for (const [node, tag] of [[img, "image"], [anim, "animimg"], [ibtn, "imagebutton"]]) {
        check(getProperty(node, "bgOpa") === 0, `${tag} MAIN|DEFAULT is transparent`);
        check(getProperty(node, "borderWidth") === 0, `${tag} MAIN|DEFAULT is borderless`);
        check(getProperty(node, "imageOpa") === 255, `${tag} DEFAULT does not dim its pixels`);
    }

    /* Row 4 and row 31 dim with disabled_opa (128); row 30 deliberately does
     * NOT — animimg lists DEFAULT only, so its frames must stay untouched. */
    check(getProperty(img, "imageOpa", { state: "disabled" }) === 128,
          "image DISABLED dims to disabled_opa");
    check(getProperty(img, "imageOpa", { state: "pressed" }) === 128,
          "image PRESSED dims to disabled_opa");

    check(getProperty(ibtn, "imageOpa", { state: "disabled" }) === 128,
          "imagebutton DISABLED dims to disabled_opa");
    check(getProperty(ibtn, "imageOpa", { state: "pressed" }) === 128,
          "imagebutton PRESSED dims to disabled_opa");
    check(getProperty(ibtn, "imageOpa", { state: "checked" }) === 128,
          "imagebutton CHECKED dims to disabled_opa");

    check(getProperty(anim, "imageOpa", { state: "disabled" }) === 255,
          "animimg DISABLED keeps full opacity — row 30 lists no dimming");
    check(getProperty(anim, "imageOpa", { state: "pressed" }) === 255,
          "animimg PRESSED keeps full opacity — row 30 lists no dimming");

    /* Row 31: LVGL already swaps the source image per state, so the theme must
     * never add a background fill that would clash with the artwork. */
    check(getProperty(ibtn, "bgOpa", { state: "pressed" }) === 0,
          "imagebutton PRESSED adds no background fill");
    check(getProperty(ibtn, "bgOpa", { state: "checked" }) === 0,
          "imagebutton CHECKED adds no background fill");
    check(getProperty(ibtn, "bgOpa", { state: "disabled" }) === 0,
          "imagebutton DISABLED adds no background fill");

    dispose();
}

console.log("\n== imperative msgbox internals (doc/theme.md §8.1) ==");
{
    const panel    = showMsgbox({ title: "T", text: "Body", buttons: ["OK"] });
    const backdrop = lv.getParent(panel);
    const header   = lv.getChild(panel, 0);
    const content  = lv.getChild(panel, 1);
    const footer   = lv.getChild(panel, 2);
    const closeBtn = lv.getChild(header, 1);
    const okBtn    = lv.getChild(footer, 0);

    /* §8.1 pins the panel to radius_base (4). It used to be a hardcoded 8;
     * no pre-existing assertion depended on that value. */
    check(getProperty(panel, "radius") === 4, "msgbox panel uses radius_base (4), not 8");
    check(getProperty(panel, "backgroundColor") === "#ffffff", "msgbox panel is bg_overlay");
    check(getProperty(panel, "shadowWidth") === 12, "msgbox panel shadow is shadow_overlay_width");
    check(getProperty(panel, "shadowOpa") === 31, "msgbox panel shadow uses shadow_opa");

    check(getProperty(backdrop, "backgroundColor") === "#000000", "msgbox backdrop is overlay_mask");
    check(getProperty(backdrop, "bgOpa") === 128, "msgbox backdrop uses overlay_mask_opa");

    check(getProperty(header, "borderColor") === "#ebeef5", "msgbox header rule is border_lighter");
    check(getProperty(header, "borderWidth") === 1, "msgbox header carries a hairline rule");
    check(getProperty(content, "textColor") === "#606266", "msgbox content text is text_regular");
    check(getProperty(footer, "borderColor") === "#ebeef5", "msgbox footer rule is border_lighter");
    check(getProperty(footer, "borderWidth") === 1, "msgbox footer carries a hairline rule");

    /* Footer buttons reuse the standalone button styles rather than a parallel
     * set — they need their own dispatch because lv_msgbox_footer_button_class
     * derives from lv_obj_class, not lv_button_class. */
    check(getProperty(okBtn, "backgroundColor") === "#409eff",
          "msgbox footer button reuses the standalone button fill");
    check(getProperty(okBtn, "backgroundColor", { state: "pressed" }) === "#337ecc",
          "msgbox footer button reuses the standalone pressed fill");
    check(getProperty(okBtn, "backgroundColor", { state: "hover" }) === "#79bbff",
          "msgbox footer button reuses the standalone hover fill");
    check(getProperty(okBtn, "outlineColor", { state: "focusKey" }) === "#a0cfff",
          "msgbox footer button reuses the standalone focus-key ring");
    check(getProperty(okBtn, "backgroundColor", { state: "checked" }) === "#337ecc",
          "msgbox footer button reuses the standalone checked fill");
    check(getProperty(okBtn, "backgroundColor", { state: "disabled" }) === "#f5f7fa",
          "msgbox footer button reuses the standalone disabled fill");
    check(getProperty(closeBtn, "bgOpa") === 0,
          "msgbox close button is transparent, not primary-filled");
    check(getProperty(closeBtn, "backgroundColor", { state: "hover" }) === "#f5f7fa",
          "msgbox close button hovers to fill_light");

    setTheme("dark");
    check(getProperty(panel, "backgroundColor") === "#1d1e1f", "msgbox panel repaints on scheme swap");
    check(getProperty(backdrop, "bgOpa") === 128, "msgbox backdrop keeps overlay_mask_opa in dark");
    setTheme("light");

    lv.dispose(backdrop);
    check(true, "msgbox teardown — no JS exception");
}

console.log("\n== msgbox internals resolve the CJK role fonts ==");
{
    /* `lv_msgbox_create(NULL)` roots the whole modal at `lv_layer_top()`
     * (lv_msgbox.c:113), NOT at the active screen — and `install_theme` only
     * adds `st_screen` to `lv_display_get_screen_active()`. text_font is an
     * INHERITED property, so the walk from the content label runs
     * label → content → panel → backdrop → layer_top → (nothing) and lands on
     * LV_FONT_DEFAULT. Only `st_msgbox_header` stated a font, which is exactly
     * why the title rendered real Chinese while the body and both footer
     * captions rendered tofu. The theme now states font.base on
     * `st_msgbox_content` and `st_msgbox_footer`, the two ancestors those
     * labels actually inherit from.
     *
     * Every expected line height below is MEASURED off an explicit-font probe
     * label, never hardcoded, so the assertions cannot pass vacuously against
     * a Montserrat fallback that happens to share a metric. */
    const cjkPath = findCjkFont();
    if (!cjkPath) {
        check(true, "SKIP CJK font unavailable — msgbox role-font test not run");
    } else {
        const face14 = loadFont(cjkPath, 14);
        const face16 = loadFont(cjkPath, 16);
        const face20 = loadFont(cjkPath, 20);
        const face24 = loadFont(cjkPath, 24);

        let probe14 = null, probe20 = null, probe24 = null;
        const disposeProbes = render(() => h("view", {},
            h("text", { ref: (n) => probe14 = n, text: "基准", style: { font: face14 } }),
            h("text", { ref: (n) => probe20 = n, text: "标题", style: { font: face20 } }),
            h("text", { ref: (n) => probe24 = n, text: "特大", style: { font: face24 } }),
        ));
        const h14 = getProperty(probe14, "fontLineHeight");
        const h20 = getProperty(probe20, "fontLineHeight");
        const h24 = getProperty(probe24, "fontLineHeight");
        check(h14 > 0 && h20 > h14 && h24 > h20,
              "role probes measure three strictly increasing CJK line heights");

        setDefaultFont({ 14: face14, 16: face16, 20: face20, 24: face24 });

        const panel    = showMsgbox({ title: "系统提示", text: "主题演示消息框",
                                      buttons: ["取消", "确定"] });
        const backdrop = lv.getParent(panel);
        const header   = lv.getChild(panel, 0);
        const content  = lv.getChild(panel, 1);
        const footer   = lv.getChild(panel, 2);
        const titleCap = lv.getChild(header, 0);
        const bodyCap  = lv.getChild(content, 0);
        const cancelCap = lv.getChild(lv.getChild(footer, 0), 0);
        const okCap     = lv.getChild(lv.getChild(footer, 1), 0);

        check(getProperty(bodyCap, "fontLineHeight") === h14,
              "msgbox content label resolves font.base, not the Latin fallback");
        check(getProperty(cancelCap, "fontLineHeight") === h14 &&
              getProperty(okCap, "fontLineHeight") === h14,
              "both msgbox footer-button captions resolve font.base");
        check(getProperty(content, "fontLineHeight") === h14 &&
              getProperty(footer, "fontLineHeight") === h14,
              "the msgbox content and footer containers own font.base themselves");
        check(getProperty(titleCap, "fontLineHeight") === h20,
              "msgbox header title stays on the font.large role");

        /* Non-vacuous repaint: only the THEME can move an already-open modal,
         * so a role-map swap that leaves these unchanged means the font is
         * coming from somewhere else (a local style or the default fallback). */
        setDefaultFont({ 14: face24, 16: face24, 20: face24, 24: face24 });
        check(getProperty(bodyCap, "fontLineHeight") === h24 &&
              getProperty(cancelCap, "fontLineHeight") === h24 &&
              getProperty(okCap, "fontLineHeight") === h24,
              "a role-map repaint moves the live msgbox body and both footer captions");
        check(getProperty(titleCap, "fontLineHeight") === h24,
              "the same repaint moves the msgbox title through font.large");

        /* ANTI-VACUITY GUARD — the `=== h14` rows above cannot stand alone.
         * LV_FONT_DEFAULT is Montserrat-14 and reports the SAME 16 px line
         * height as this CJK face at 14, so a msgbox internal that states no
         * font and falls through to the fallback measures IDENTICALLY to one
         * that correctly resolves font.base. Reverting the fix and re-running
         * proves it: only the h24 row above flips to FAIL. Pin the base role to
         * the 16 px face — whose metric provably differs from the fallback's —
         * and re-probe every internal, including the two containers and the
         * close affordance, none of which any h24 row discriminates. */
        let probe16 = null, probeLatin14 = null;
        const disposeGuards = render(() => h("view", {},
            h("text", { ref: (n) => probe16 = n, text: "正文", style: { font: face16 } }),
            h("text", { ref: (n) => probeLatin14 = n, text: "Base", style: { fontSize: 14 } }),
        ));
        const h16 = getProperty(probe16, "fontLineHeight");
        const hFallback = getProperty(probeLatin14, "fontLineHeight");
        check(hFallback === h14 && h16 !== hFallback,
              "Montserrat-14 shares CJK@14's line height, so only a non-14 base role discriminates");

        /* header children are [0] the title label, [1] the close button
         * (lv_msgbox.c:157 then :173 — both parented to mbox->header). */
        const closeBtn = lv.getChild(header, 1);

        setDefaultFont({ 14: face16, 16: face16, 20: face20, 24: face24 });
        check(getProperty(bodyCap, "fontLineHeight") === h16 &&
              getProperty(cancelCap, "fontLineHeight") === h16 &&
              getProperty(okCap, "fontLineHeight") === h16,
              "msgbox body and both footer captions track font.base off the fallback metric");
        check(getProperty(content, "fontLineHeight") === h16 &&
              getProperty(footer, "fontLineHeight") === h16,
              "msgbox content and footer containers state font.base off the fallback metric");
        check(getProperty(closeBtn, "fontLineHeight") === h16,
              "the msgbox close affordance states font.base, not the header's font.large");
        check(getProperty(titleCap, "fontLineHeight") === h20,
              "pinning the base role off the fallback leaves the title on font.large");

        setTheme("dark");
        check(getProperty(bodyCap, "fontLineHeight") === h16 &&
              getProperty(okCap, "fontLineHeight") === h16 &&
              getProperty(footer, "fontLineHeight") === h16,
              "the msgbox keeps a non-fallback font.base across a light → dark swap");
        setTheme("light");
        disposeGuards();

        setDefaultFont({ 14: face14, 16: face16, 20: face20, 24: face24 });
        check(getProperty(bodyCap, "fontLineHeight") === h14 &&
              getProperty(okCap, "fontLineHeight") === h14 &&
              getProperty(titleCap, "fontLineHeight") === h20,
              "restoring the role map returns every msgbox caption to its own role");

        /* A scheme swap must not disturb typography. */
        setTheme("dark");
        check(getProperty(bodyCap, "fontLineHeight") === h14 &&
              getProperty(okCap, "fontLineHeight") === h14,
              "msgbox internals keep font.base across a light → dark swap");
        setTheme("light");

        lv.dispose(backdrop);
        disposeProbes();
    }
}

console.log("\n== complete host-tag + internal-class theme coverage matrix ==");
{
    const hostTagsSource = std.loadFile(`${EXAMPLES}/../js/framework.js`);
    const hostTagsBody = /const HOST_TAGS = \{([\s\S]*?)\n\};/.exec(hostTagsSource)?.[1] ?? "";
    const sourceHostTags = [...hostTagsBody.matchAll(/^\s*([A-Za-z][A-Za-z0-9]*):/gm)]
        .map((match) => match[1]);
    const expectedHostTags = [
        "view", "text", "button", "image", "input", "switch", "progress",
        "slider", "arc", "spinner", "checkbox", "dropdown", "roller",
        "tabview", "tab", "list", "listButton", "spinbox", "led", "chart",
        "buttonMatrix", "calendar", "scale", "span", "line", "table", "menu",
        "menuPage", "keyboard", "animimg", "imagebutton",
    ];

    check(sourceHostTags.length === 31 &&
          new Set(sourceHostTags).size === sourceHostTags.length &&
          sourceHostTags.join("\n") === expectedHostTags.join("\n"),
          "coverage manifest exactly matches all 31 HOST_TAGS in source order");

    const refs = {};
    const image = loadImage(`${EXAMPLES}/showcase/assets/test.png`);
    const frames = loadImages([
        `${EXAMPLES}/showcase/assets/frame1.png`,
        `${EXAMPLES}/showcase/assets/frame2.png`,
    ]);
    const released = loadImage(`${EXAMPLES}/showcase/assets/released.png`);
    const keep = (name) => (node) => { refs[name] = node; };

    const dispose = render(() => h("view", { ref: keep("view") },
        h("text", { ref: keep("text"), text: "coverage" }),
        h("button", { ref: keep("button"), text: "coverage" }),
        h("image", { ref: keep("image"), src: image }),
        h("input", { ref: keep("input"), placeholder: "coverage" }),
        h("switch", { ref: keep("switch") }),
        h("progress", { ref: keep("progress"), value: 50 }),
        h("slider", { ref: keep("slider"), value: 50 }),
        h("arc", { ref: keep("arc"), value: 50 }),
        h("spinner", { ref: keep("spinner") }),
        h("checkbox", { ref: keep("checkbox"), text: "coverage" }),
        h("dropdown", { ref: keep("dropdown"), options: "one\ntwo" }),
        h("roller", { ref: keep("roller"), options: "one\ntwo" }),
        h("tabview", { ref: keep("tabview") },
            h("tab", { ref: keep("tab"), title: "Coverage" }),
        ),
        h("list", { ref: keep("list") },
            h("listButton", { ref: keep("listButton"), text: "Coverage" }),
        ),
        h("spinbox", { ref: keep("spinbox") }),
        h("led", { ref: keep("led") }),
        h("chart", { ref: keep("chart") }),
        h("buttonMatrix", { ref: keep("buttonMatrix"), map: ["A", "B"] }),
        h("calendar", { ref: keep("calendar") }),
        h("scale", { ref: keep("scale") }),
        h("span", { ref: keep("span") }),
        h("line", { ref: keep("line"), points: [[0, 0], [10, 10]] }),
        h("table", { ref: keep("table"), rows: 1, cols: 1, cells: [["cell"]] }),
        h("menu", { ref: keep("menu") },
            h("menuPage", { ref: keep("menuPage"), title: "Coverage" },
                h("text", { text: "page" }),
            ),
        ),
        h("keyboard", { ref: keep("keyboard"), target: () => refs.input }),
        h("animimg", { ref: keep("animimg"), src: frames, duration: 400 }),
        h("imagebutton", { ref: keep("imagebutton"), released }),
    ));

    refs.tabHeader = lv.getChild(refs.tabview, 0);
    refs.tabButton = lv.getChild(refs.tabHeader, 0);
    refs.calendarHeader = lv.getChild(refs.calendar, 0);
    refs.calendarHeaderButton = lv.getChild(refs.calendarHeader, 0);
    refs.calendarGrid = lv.getChild(refs.calendar, 1);
    refs.menuMain = lv.getChild(refs.menu, 1);
    refs.menuHeader = lv.getChild(refs.menuMain, 0);
    refs.menuSection = lv.createThemeCoverageMenuInternals(refs.menuPage);
    refs.menuCont = lv.getChild(refs.menuSection, 0);
    refs.menuSeparator = lv.getChild(refs.menuPage, 2);

    const msgbox = showMsgbox({ title: "Coverage", text: "Body", buttons: ["OK"] });
    const msgboxBackdrop = lv.getParent(msgbox);
    const msgboxHeader = lv.getChild(msgbox, 0);
    const msgboxContent = lv.getChild(msgbox, 1);
    const msgboxFooter = lv.getChild(msgbox, 2);
    const msgboxClose = lv.getChild(msgboxHeader, 1);
    const msgboxButton = lv.getChild(msgboxFooter, 0);

    const D = undefined;
    const S = (part, state) => ({ part, state });
    const row = (name, assertions) => ({ name, assertions });
    const a = (node, property, selector, light, dark, label) =>
        ({ node, property, selector, light, dark, label });
    const rows = [
        row("view", [
            a(refs.view, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.view, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN transparent branch witness"),
            a(refs.view, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
            a(refs.view, "bgOpa", S("scrollbar", "scrolled"), 255, 255, "SCROLLBAR scrolled opacity"),
        ]),
        row("text", [a(refs.text, "textColor", D, "#303133", "#e5eaf3", "MAIN inherited text")]),
        row("button", [
            a(refs.button, "backgroundColor", D, "#409eff", "#409eff", "MAIN default"),
            a(refs.button, "backgroundColor", S("main", "hover"), "#79bbff", "#3375b9", "MAIN hover"),
            a(refs.button, "backgroundColor", S("main", "pressed"), "#337ecc", "#66b1ff", "MAIN pressed"),
            a(refs.button, "backgroundColor", S("main", "checked"), "#337ecc", "#66b1ff", "MAIN checked"),
            a(refs.button, "backgroundColor", S("main", "disabled"), "#f5f7fa", "#262727", "MAIN disabled"),
            a(refs.button, "outlineColor", S("main", "focusKey"), "#a0cfff", "#2a598a", "MAIN focus-key outline"),
            a(refs.button, "outlineWidth", S("main", "focusKey"), 2, 2, "MAIN focus-key width"),
        ]),
        row("image", [
            a(refs.image, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.image, "borderWidth", D, 0, 0, "MAIN borderless"),
            a(refs.image, "imageOpa", S("main", "pressed"), 128, 128, "MAIN pressed dim"),
            a(refs.image, "imageOpa", S("main", "disabled"), 128, 128, "MAIN disabled dim"),
        ]),
        row("input", [
            a(refs.input, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.input, "borderColor", S("main", "hover"), "#cdd0d6", "#636466", "MAIN hover"),
            a(refs.input, "borderColor", S("main", "focus"), "#409eff", "#409eff", "MAIN focus"),
            a(refs.input, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
            a(refs.input, "backgroundColor", S("cursor", "focus"), "#409eff", "#409eff", "CURSOR focus"),
            a(refs.input, "textColor", S("placeholder", "default"), "#a8abb2", "#8d9095", "placeholder default"),
        ]),
        row("switch", [
            a(refs.switch, "backgroundColor", D, "#e4e7ed", "#414243", "MAIN default"),
            a(refs.switch, "bgOpa", S("indicator", "default"), 0, 0, "INDICATOR unchecked transparent"),
            a(refs.switch, "backgroundColor", S("indicator", "checked"), "#409eff", "#409eff", "INDICATOR checked"),
            a(refs.switch, "bgOpa", S("indicator", "checked"), 255, 255, "INDICATOR checked opacity"),
            a(refs.switch, "backgroundColor", S("indicator", "disabled"), "#c0c4cc", "#6c6e72", "INDICATOR disabled"),
            a(refs.switch, "backgroundColor", S("knob", "default"), "#ffffff", "#ffffff", "KNOB default"),
        ]),
        row("progress", [
            a(refs.progress, "backgroundColor", D, "#e4e7ed", "#414243", "MAIN default"),
            a(refs.progress, "backgroundColor", S("indicator", "default"), "#409eff", "#409eff", "INDICATOR default"),
        ]),
        row("slider", [
            a(refs.slider, "backgroundColor", D, "#e4e7ed", "#414243", "MAIN default"),
            a(refs.slider, "backgroundColor", S("indicator", "disabled"), "#c0c4cc", "#6c6e72", "INDICATOR disabled"),
            a(refs.slider, "borderColor", S("knob", "pressed"), "#337ecc", "#66b1ff", "KNOB pressed"),
        ]),
        row("arc", [
            a(refs.arc, "arcColor", D, "#e4e7ed", "#414243", "MAIN default"),
            a(refs.arc, "arcColor", S("indicator", "disabled"), "#c0c4cc", "#6c6e72", "INDICATOR disabled"),
            a(refs.arc, "borderColor", S("knob", "pressed"), "#337ecc", "#66b1ff", "KNOB pressed"),
        ]),
        row("spinner", [
            a(refs.spinner, "arcColor", D, "#e4e7ed", "#414243", "MAIN default"),
            a(refs.spinner, "arcColor", S("indicator", "default"), "#409eff", "#409eff", "INDICATOR default"),
            a(refs.spinner, "bgOpa", S("knob", "default"), 0, 0, "KNOB invisible"),
        ]),
        row("checkbox", [
            a(refs.checkbox, "textColor", S("main", "checked"), "#409eff", "#409eff", "MAIN checked"),
            a(refs.checkbox, "borderColor", S("indicator", "hover"), "#409eff", "#409eff", "INDICATOR hover"),
            a(refs.checkbox, "textColor", S("indicator", "checked"), "#ffffff", "#ffffff", "INDICATOR checked tick"),
            a(refs.checkbox, "backgroundColor", S("indicator", "pressed"), "#ecf5ff", "#18222b", "INDICATOR pressed"),
            a(refs.checkbox, "backgroundColor", S("indicator", "disabled"), "#f5f7fa", "#262727", "INDICATOR disabled"),
        ]),
        row("dropdown", [
            a(refs.dropdown, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.dropdown, "textColor", S("indicator", "default"), "#a8abb2", "#8d9095", "INDICATOR default"),
            a(refs.dropdown, "backgroundColor", S("selected", "checked"), "#ecf5ff", "#18222b", "popup SELECTED checked"),
            a(refs.dropdown, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "popup SCROLLBAR default"),
        ]),
        row("roller", [
            a(refs.roller, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.roller, "backgroundColor", S("selected", "checked"), "#337ecc", "#66b1ff", "SELECTED checked"),
        ]),
        row("tabview", [
            a(refs.tabview, "backgroundColor", D, "#ffffff", "#141414", "MAIN default"),
            a(refs.tabHeader, "borderColor", D, "#e4e7ed", "#414243", "header MAIN default"),
            a(refs.tabButton, "textColor", S("main", "checked"), "#409eff", "#409eff", "tab button checked"),
        ]),
        row("tab", [
            a(refs.tab, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.tab, "padding", D, 16, 16, "MAIN space_lg padding"),
            a(refs.tab, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
        ]),
        row("list", [
            a(refs.list, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.list, "borderColor", D, "#e4e7ed", "#414243", "MAIN border"),
            a(refs.list, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
        ]),
        row("listButton", [
            a(refs.listButton, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.listButton, "backgroundColor", S("main", "hover"), "#f5f7fa", "#262727", "MAIN hover"),
            a(refs.listButton, "backgroundColor", S("main", "checked"), "#ecf5ff", "#18222b", "MAIN checked"),
        ]),
        row("spinbox", [
            a(refs.spinbox, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.spinbox, "backgroundColor", S("cursor", "default"), "#409eff", "#409eff", "CURSOR default"),
        ]),
        row("led", [
            a(refs.led, "backgroundColor", D, "#ffffff", "#ffffff", "MAIN max-brightness colour scale"),
            a(refs.led, "bgOpa", D, 255, 255, "MAIN opaque colour scale"),
            a(refs.led, "shadowWidth", D, 8, 8, "MAIN custom glow"),
        ]),
        row("chart", [
            a(refs.chart, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.chart, "lineColor", D, "#ebeef5", "#363637", "MAIN division lines"),
            a(refs.chart, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
            a(refs.chart, "lineWidth", S("items", "default"), 2, 2, "ITEMS series width"),
            a(refs.chart, "backgroundColor", S("indicator", "default"), "#409eff", "#409eff", "INDICATOR markers"),
            a(refs.chart, "lineColor", S("cursor", "default"), "#409eff", "#409eff", "CURSOR line"),
        ]),
        row("buttonMatrix", [
            a(refs.buttonMatrix, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.buttonMatrix, "backgroundColor", S("items", "hover"), "#f5f7fa", "#262727", "ITEMS hover"),
            a(refs.buttonMatrix, "backgroundColor", S("items", "checked"), "#409eff", "#409eff", "ITEMS checked"),
            a(refs.buttonMatrix, "backgroundColor", S("items", "disabled"), "#f5f7fa", "#262727", "ITEMS disabled"),
            a(refs.buttonMatrix, "outlineColor", S("main", "edited"), "#67c23a", "#67c23a", "MAIN edited outline"),
            a(refs.buttonMatrix, "outlineColor", S("items", "edited"), "#67c23a", "#67c23a", "ITEMS edited outline"),
        ]),
        row("calendar", [
            a(refs.calendar, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.calendar, "radius", D, 4, 4, "MAIN radius_base"),
            a(refs.calendarGrid, "bgOpa", D, 0, 0, "day grid MAIN transparent"),
            a(refs.calendarGrid, "textColor", S("items", "default"), "#303133", "#e5eaf3", "day grid ITEMS default"),
            a(refs.calendarGrid, "backgroundColor", S("items", "hover"), "#f5f7fa", "#262727", "day grid ITEMS hover"),
            a(refs.calendarGrid, "textColor", S("items", "disabled"), "#c0c4cc", "#6c6e72", "day grid ITEMS disabled"),
            a(refs.calendarHeader, "bgOpa", D, 0, 0, "header MAIN transparent"),
            a(refs.calendarHeaderButton, "backgroundColor", S("main", "hover"), "#f5f7fa", "#262727", "header arrow hover"),
        ]),
        row("scale", [
            a(refs.scale, "lineColor", D, "#dcdfe6", "#4c4d4f", "MAIN line"),
            a(refs.scale, "lineWidth", D, 1, 1, "MAIN baseline width"),
            a(refs.scale, "arcWidth", D, 1, 1, "MAIN arc width"),
            a(refs.scale, "textColor", S("indicator", "default"), "#606266", "#cfd3dc", "INDICATOR labels"),
            a(refs.scale, "lineWidth", S("indicator", "default"), 1, 1, "INDICATOR major-tick width"),
            a(refs.scale, "lineColor", S("items", "default"), "#d4d7de", "#58585b", "ITEMS ticks"),
            a(refs.scale, "lineWidth", S("items", "default"), 1, 1, "ITEMS minor-tick width"),
        ]),
        row("span", [a(refs.span, "textColor", D, "#303133", "#e5eaf3", "MAIN text")]),
        row("line", [
            a(refs.line, "lineColor", D, "#409eff", "#409eff", "MAIN line"),
            a(refs.line, "lineWidth", D, 2, 2, "MAIN width"),
        ]),
        row("table", [
            a(refs.table, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN default"),
            a(refs.table, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
            a(refs.table, "borderColor", S("items", "default"), "#ebeef5", "#363637", "ITEMS grid"),
            a(refs.table, "backgroundColor", S("items", "hover"), "#f5f7fa", "#262727", "ITEMS hover"),
            a(refs.table, "backgroundColor", S("items", "pressed"), "#ebedf0", "#39393a", "ITEMS pressed"),
            a(refs.table, "outlineColor", S("main", "edited"), "#67c23a", "#67c23a", "MAIN edited outline"),
            a(refs.table, "outlineColor", S("items", "edited"), "#67c23a", "#67c23a", "ITEMS edited outline"),
        ]),
        row("menu", [
            a(refs.menu, "backgroundColor", D, "#ffffff", "#141414", "MAIN default"),
            a(refs.menuMain, "bgOpa", D, 0, 0, "main container MAIN transparent"),
            a(refs.menuMain, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "main container SCROLLBAR"),
            a(refs.menuHeader, "bgOpa", D, 0, 0, "header MAIN transparent"),
            a(refs.menuSection, "backgroundColor", D, "#ffffff", "#1d1e1f", "section MAIN overlay"),
            a(refs.menuSection, "radius", D, 4, 4, "section MAIN radius"),
            a(refs.menuSeparator, "backgroundColor", D, "#ebeef5", "#363637", "separator MAIN rule"),
            a(refs.menuCont, "textColor", D, "#606266", "#cfd3dc", "item MAIN default"),
            a(refs.menuCont, "backgroundColor", S("main", "hover"), "#f5f7fa", "#262727", "item MAIN hover"),
            a(refs.menuCont, "backgroundColor", S("main", "checked"), "#ecf5ff", "#18222b", "item MAIN checked"),
        ]),
        row("menuPage", [
            a(refs.menuPage, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.menuPage, "backgroundColor", S("scrollbar", "default"), "#d4d7de", "#58585b", "SCROLLBAR default"),
        ]),
        row("keyboard", [
            a(refs.keyboard, "backgroundColor", D, "#f2f3f5", "#0a0a0a", "MAIN default"),
            a(refs.keyboard, "backgroundColor", S("items", "default"), "#ffffff", "#1d1e1f", "ITEMS default"),
            a(refs.keyboard, "backgroundColor", S("items", "pressed"), "#ebedf0", "#39393a", "ITEMS pressed"),
            a(refs.keyboard, "backgroundColor", S("items", "checked"), "#409eff", "#409eff", "ITEMS checked"),
            a(refs.keyboard, "backgroundColor", S("items", "disabled"), "#f5f7fa", "#262727", "ITEMS disabled"),
            a(refs.keyboard, "outlineColor", S("main", "focusKey"), "#a0cfff", "#2a598a", "MAIN focus-key outline"),
            a(refs.keyboard, "outlineColor", S("main", "edited"), "#67c23a", "#67c23a", "MAIN edited outline"),
        ]),
        row("animimg", [
            a(refs.animimg, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.animimg, "backgroundColor", D, "#ffffff", "#1d1e1f", "MAIN exact-class branch witness"),
            a(refs.animimg, "borderWidth", D, 0, 0, "MAIN borderless"),
            a(refs.animimg, "imageOpa", S("main", "disabled"), 255, 255, "MAIN disabled pixels intact"),
        ]),
        row("imagebutton", [
            a(refs.imagebutton, "bgOpa", D, 0, 0, "MAIN transparent"),
            a(refs.imagebutton, "borderWidth", D, 0, 0, "MAIN borderless"),
            a(refs.imagebutton, "imageOpa", S("main", "pressed"), 128, 128, "MAIN pressed dim"),
            a(refs.imagebutton, "imageOpa", S("main", "checked"), 128, 128, "MAIN checked dim"),
            a(refs.imagebutton, "imageOpa", S("main", "disabled"), 128, 128, "MAIN disabled dim"),
        ]),
        row("msgbox", [
            a(msgboxBackdrop, "bgOpa", D, 128, 128, "backdrop MAIN mask opacity"),
            a(msgbox, "backgroundColor", D, "#ffffff", "#1d1e1f", "panel MAIN overlay"),
            a(msgbox, "shadowWidth", D, 12, 12, "panel MAIN shadow"),
            a(msgboxHeader, "borderColor", D, "#ebeef5", "#363637", "header MAIN rule"),
            a(msgboxContent, "textColor", D, "#606266", "#cfd3dc", "content MAIN text"),
            a(msgboxContent, "backgroundColor", S("scrollbar", "default"), "#909399", "#a3a6ad", "content SCROLLBAR"),
            a(msgboxFooter, "borderColor", D, "#ebeef5", "#363637", "footer MAIN rule"),
            a(msgboxButton, "backgroundColor", S("main", "hover"), "#79bbff", "#3375b9", "footer button hover"),
            a(msgboxButton, "backgroundColor", S("main", "pressed"), "#337ecc", "#66b1ff", "footer button pressed"),
            a(msgboxButton, "backgroundColor", S("main", "checked"), "#337ecc", "#66b1ff", "footer button checked"),
            a(msgboxButton, "backgroundColor", S("main", "disabled"), "#f5f7fa", "#262727", "footer button disabled"),
            a(msgboxButton, "outlineColor", S("main", "focusKey"), "#a0cfff", "#2a598a", "footer button focus-key"),
            a(msgboxClose, "backgroundColor", S("main", "hover"), "#f5f7fa", "#262727", "close button hover"),
        ]),
    ];

    check(rows.slice(0, 31).map((entry) => entry.name).join("\n") === expectedHostTags.join("\n") &&
          rows.length === 32 && rows[31].name === "msgbox",
          "coverage matrix contains 31 unique host rows plus imperative msgbox");

    for (const scheme of ["light", "dark"]) {
        setTheme(scheme);
        for (const entry of rows) {
            for (const assertion of entry.assertions) {
                const actual = getProperty(assertion.node, assertion.property, assertion.selector);
                const expected = assertion[scheme];
                check(actual === expected,
                      `coverage ${scheme}: ${entry.name} ${assertion.label}`);
            }
        }
    }

    setTheme("light");
    lv.dispose(msgboxBackdrop);
    dispose();
}

/* Optional negative-path switch for CI: STONEGUI_TEST_FORCE_FAIL=1 makes the
 * bundle report a failure (exit 1) without weakening any real assertion. */
if (std.getenv("STONEGUI_TEST_FORCE_FAIL")) fail++;

console.log(`\n${pass} passed, ${fail} failed`);
console.log(fail === 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
std.exit(fail === 0 ? 0 : 1);
