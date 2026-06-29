/**
 * app.js — stonegui Element Plus theme showcase
 *
 * Living proof that Phase A is complete: exercises every bound host tag
 * across four tabs, with a single button that flips the global colour scheme
 * between Element Plus light and dark. Text / surface / track / primary
 * tokens all repaint live via `lv_obj_report_style_change(NULL)` inside
 * `sg_theme_set_scheme`.
 *
 * Run:
 *   timeout --preserve-status -s TERM 3 \
 *       ./build/stonegui --no-watch examples/theme/app.js
 */

import {
    h, render, createSignal, createEffect,
    setTheme, setThemeToken, setDefaultFont, findCjkFont,
    loadFont, chartAddSeries, chartSetData, showMsgbox,
    Show, For,
} from "../../js/framework.js";
import * as lv from "lvgl";

setDefaultFont();
const _cjkPath  = findCjkFont();
const fontTitle = _cjkPath ? loadFont(_cjkPath, 24) : 0;

/* ── Scheme state ───────────────────────────────────────────────────────── */

let   currentScheme         = "light";
const [scheme, setSchemeSig] = createSignal("light");

function toggleScheme() {
    currentScheme = currentScheme === "light" ? "dark" : "light";
    setTheme(currentScheme);
    setSchemeSig(currentScheme);
}

function ThemeToggle() {
    return h("button", {
        style: { width: 200, height: 40 },
        text:  () => `Toggle Dark / Light  (${scheme()})`,
        onClick: toggleScheme,
    });
}

/* ── Layout helpers (components, not host tags) ─────────────────────────── */

function Row({ height = 56, gap = 12, children }) {
    return h("view", {
        style: {
            flexFlow:   "row",
            width:      "100%",
            height,
            gap,
            alignItems: "center",
        },
    }, children);
}

function Label({ text, width = 140 }) {
    return h("text", { style: { width }, text });
}

function TabPage({ children }) {
    return h("view", {
        style: {
            width:      "100%",
            height:     "100%",
            padding:    16,
            flexFlow:   "column",
            gap:        14,
            scrollable: true,
        },
    }, children);
}

/* ── Tab 1: Basics — text/button/input/switch/checkbox/progress/slider/arc/spinbox ── */

function BasicsTab() {
    const [progress, setProg]   = createSignal(40);
    const [slider,   setSlider] = createSignal(35);
    const [arcVal,   setArcVal] = createSignal(60);
    const [on,       setOn]     = createSignal(true);
    const [agree,    setAgree]  = createSignal(false);
    const [age,      setAge]    = createSignal(18);
    let inputRef = null;

    return h(TabPage, {},
        h(Row, {},
            h(Label, { text: "Text" }),
            h("text", { text: "Hello, 世界 — themed text inherits on_surface" }),
        ),
        h(Row, {},
            h(Label, { text: "Button" }),
            h("button", { style: { width: 120, height: 40 }, text: "Primary" }),
            h("button", {
                style:   { width: 120, height: 40 },
                text:    "Show msg",
                onClick: () => showMsgbox(
                    { title: "Hello", text: "Msgbox repaints with the theme.", buttons: ["OK"] }
                ),
            }),
        ),
        h(Row, {},
            h(Label, { text: "Input" }),
            h("input", {
                style:       { flexGrow: 1, height: 42 },
                oneLine:     true,
                placeholder: "Type something here…",
                ref:         (n) => { inputRef = n; },
            }),
        ),
        h(Row, {},
            h(Label, { text: "Switch / Check" }),
            h("switch", { checked: () => on(), onChange: (v) => setOn(v) }),
            h("text",   { style: { width: 50 }, text: () => on() ? "on" : "off" }),
            h("checkbox", {
                text:     "Agree",
                checked:  () => agree(),
                onChange: (v) => setAgree(v),
            }),
        ),
        h(Row, {},
            h(Label, { text: "Progress" }),
            h("progress", {
                style: { flexGrow: 1, height: 18 },
                min:   0, max: 100,
                value: () => progress(),
            }),
            h("text", { style: { width: 60 }, text: () => `${progress()}%` }),
        ),
        h(Row, {},
            h(Label, { text: "Slider" }),
            h("slider", {
                style: { flexGrow: 1 },
                min:   0, max: 100,
                value:    () => slider(),
                onChange: (v) => { setSlider(v); setProg(v); },
            }),
            h("text", { style: { width: 60 }, text: () => `${slider()}` }),
        ),
        h(Row, { height: 110 },
            h(Label, { text: "Arc / Spinbox" }),
            h("arc", {
                style: { width: 90, height: 90 },
                min:   0, max: 100,
                value:    () => arcVal(),
                onChange: (v) => setArcVal(v),
            }),
            h("text", { style: { width: 60 }, text: () => `${arcVal()}%` }),
            h("spinbox", {
                style:    { width: 140, height: 42 },
                digits:   "3.0",
                step:     1,
                value:    () => age(),
                onChange: (v) => setAge(v),
            }),
            h("text", { text: () => `age=${age()}` }),
        ),
    );
}

/* ── Tab 2: Choices — dropdown/roller/spinner/led/calendar ──────────────── */

function ChoicesTab() {
    const FRUITS = ["Apple 苹果", "Banana 香蕉", "Cherry 樱桃", "Durian 榴莲"];
    const [drop,   setDrop]   = createSignal(0);
    const [reel,   setReel]   = createSignal(2);
    const [picked, setPicked] = createSignal("(none)");

    return h(TabPage, {},
        h(Row, {},
            h(Label, { text: "Dropdown" }),
            h("dropdown", {
                style:    { width: 180 },
                options:  FRUITS.join("\n"),
                value:    () => drop(),
                onChange: (i) => setDrop(i),
            }),
            h("text", { text: () => `→ ${FRUITS[drop()]}` }),
        ),
        h(Row, { height: 150 },
            h(Label, { text: "Roller" }),
            h("roller", {
                style:    { width: 180, height: 140 },
                options:  FRUITS.join("\n"),
                value:    () => reel(),
                onChange: (i) => setReel(i),
            }),
            h("text", { text: () => `→ ${FRUITS[reel()]}` }),
        ),
        h(Row, { height: 70 },
            h(Label, { text: "Spinner / LED" }),
            h("spinner", { style: { width: 48, height: 48 }, spinTime: 1500 }),
            h("led",     { style: { width: 32, height: 32 }, color: "#67C23A", brightness: 220 }),
            h("led",     { style: { width: 32, height: 32 }, color: "#F56C6C", brightness: 220 }),
            h("led",     { style: { width: 32, height: 32 }, color: "#E6A23C", brightness: 220 }),
        ),
        h(Row, { height: 320 },
            h(Label, { text: "Calendar" }),
            h("calendar", {
                style:    { width: 300, height: 300 },
                today:    "2026-06-29",
                shown:    "2026-06",
                onChange: (d) => {
                    if (d && d.year) {
                        setPicked(
                            `${d.year}-${String(d.month).padStart(2, "0")}-${String(d.day).padStart(2, "0")}`,
                        );
                    }
                },
            }),
            h("text", { text: () => `Picked: ${picked()}` }),
        ),
    );
}

/* ── Tab 3: Data — chart/scale/span/table/list/listButton/line ──────────── */

function DataTab() {
    const data = [10, 28, 16, 42, 35, 58, 47, 70, 60, 82, 75, 95];
    const [selected, setSelected] = createSignal("(none)");

    return h(TabPage, {},
        h("text", { text: "Chart (line series)" }),
        h("chart", {
            style:     { width: "100%", height: 160 },
            chartType: "line",
            rangeMax:  100,
            divLines:  "4x6",
            ref: (n) => {
                const s = chartAddSeries(n, "#409EFF");
                chartSetData(n, s, data);
            },
        }),
        h("text", { text: "Scale (horizontal, bottom)" }),
        h("scale", {
            style:      { width: "100%", height: 36 },
            scaleMode:  "h-bottom",
            min:        0, max: 100,
            totalTicks: 11,
            majorEvery: 2,
            showLabels: true,
        }),
        h("text", { text: "Span (per-part colour / size)" }),
        h("span", {
            style: { width: "100%", height: 50, padding: 8 },
            parts: [
                { text: "stonegui ", color: "#409EFF", fontSize: 20 },
                { text: "= LVGL ",   color: "#67C23A", fontSize: 20 },
                { text: "+ QuickJS", color: "#E6A23C", fontSize: 20 },
            ],
        }),
        h("text", { text: "Line (polyline points)" }),
        h("line", {
            style:  { width: 240, height: 60, borderColor: "#409EFF", borderWidth: 2 },
            points: [[0, 50], [40, 20], [80, 40], [120, 10], [160, 35], [200, 5], [240, 25]],
        }),
        h("text", { text: "Table (2 cols × 3 rows)" }),
        h("table", {
            style: { width: "100%", height: 110 },
            cells: [
                ["Header A", "Header B"],
                ["Row 1A",   "Row 1B"],
                ["Row 2A",   "Row 2B"],
            ],
        }),
        h("text", { text: () => `List selected: ${selected()}` }),
        h("list", { style: { width: "100%", height: 160 } },
            h("listButton", { text: "Inbox",   onClick: () => setSelected("Inbox")   }),
            h("listButton", { text: "Sent",    onClick: () => setSelected("Sent")    }),
            h("listButton", { text: "Drafts",  onClick: () => setSelected("Drafts")  }),
        ),
    );
}

/* ── Tab 4: Composite — buttonMatrix/menu/menuPage ──────────────────────── */

function CompositeTab() {
    const MAP = ["1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", "C", "0", "="];
    const KEYS = MAP.filter((k) => k !== "\n");
    const [pressed, setPressed] = createSignal(-1);

    return h(TabPage, {},
        h("text", {
            text: () => `Button matrix pressed: ${pressed() < 0 ? "(none)" : KEYS[pressed()]}`,
        }),
        h("buttonMatrix", {
            style:    { width: "100%", height: 220 },
            map:      MAP,
            onChange: (i) => setPressed(i),
        }),
        h("text", { text: "Menu (first page auto-active)" }),
        h("menu", { style: { width: "100%", height: 240 } },
            h("menuPage", { title: "General" },
                h("text", { text: "General page — themed surface + on_surface text." }),
            ),
            h("menuPage", { title: "Display" },
                h("text", { text: "Display page — switch the theme to see this repaint." }),
            ),
        ),
    );
}

/* ── App shell — header + tabview ───────────────────────────────────────── */

function App() {
    return h("view", {
        style: { width: "100%", height: "100%", flexFlow: "column" },
    },
        h("view", {
            style: {
                width:      "100%",
                height:     56,
                padding:    12,
                flexFlow:   "row",
                gap:        12,
                alignItems: "center",
            },
        },
            h("text", {
                style: { flexGrow: 1, font: fontTitle },
                text:  "stonegui — Theme Showcase",
            }),
            h(ThemeToggle),
        ),
        h("tabview", { style: { flexGrow: 1, width: "100%" }, tabBarSize: 44 },
            h("tab", { title: "Basics"    }, h(BasicsTab)),
            h("tab", { title: "Choices"   }, h(ChoicesTab)),
            h("tab", { title: "Data"      }, h(DataTab)),
            h("tab", { title: "Composite" }, h(CompositeTab)),
        ),
    );
}

render(App);
