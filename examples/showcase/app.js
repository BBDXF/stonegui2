/**
 * app.js — stonegui 主题总览 / Element Plus theme showcase (中文)
 *
 * Two modes in one bundle:
 *
 *   1. INTERACTIVE (default) — a polished Element Plus light/dark console.
 *      Every one of the 31 public host tags is on screen with Chinese
 *      labels, laid out as cards on a `bg_page` surface. The header carries
 *      the light/dark toggle and an accent-token cycler; both repaint the
 *      whole tree through `lv_obj_report_style_change(NULL)`.
 *
 *   2. SMOKE (`STONEGUI_SHOWCASE_SMOKE=1`) — the same tree, then a scripted,
 *      finite regression that drives focus/sendKey/signal setters and reads
 *      resolved part+state styles back. Prints one PASS/FAIL per scenario,
 *      a summary, `SHOWCASE SMOKE PASSED`, and exits 0 only when all pass.
 *      `STONEGUI_SHOWCASE_SMOKE_INVALID_TOKEN=1` (used together with smoke
 *      mode) deliberately resolves `$not_a_theme_token`, prints
 *      `UNKNOWN THEME TOKEN`, and exits 1.
 *
 * Run:
 *   ./build/stonegui --no-watch examples/showcase/app.js                    # interactive
 *   STONEGUI_SHOWCASE_SMOKE=1 ./build/stonegui --no-watch examples/showcase/app.js
 *   STONEGUI_SHOWCASE_SMOKE=1 STONEGUI_SHOWCASE_SMOKE_INVALID_TOKEN=1 \
 *       ./build/stonegui --no-watch examples/showcase/app.js               # exits 1
 *
 * Expected values below come from `doc/theme.md` §3-§7 — never re-derived.
 */

import {
    h, render, createSignal,
    Show, For, createAnimation,
    setTheme, setThemeToken, setDefaultFont, findCjkFont, loadFont, loadFontSizes,
    loadImage, loadImages, moduleDir,
    chartAddSeries, chartSetData, chartSetSeriesColor, showMsgbox, setMenuPage,
    getProperty, focus, sendKey, clipboard,
} from "../../js/framework.js";

/* The msgbox and the menu build sub-objects LVGL never hands back through a
 * host tag, so the scripted checks reach them with the read-only tree
 * getters. Also the source of `getThemeToken` for the chart series colours. */
import * as lv from "lvgl";

/* QuickJS native std: getenv selects the mode, exit reports it truthfully. */
import * as std from "std";

const SMOKE               = !!std.getenv("STONEGUI_SHOWCASE_SMOKE");
const SMOKE_INVALID_TOKEN = !!std.getenv("STONEGUI_SHOWCASE_SMOKE_INVALID_TOKEN");

/* ── Assets & fonts (bundle-relative, never absolute) ───────────────────── */

const ASSETS = moduleDir(import.meta.url) + "/assets";
const REPO    = moduleDir(import.meta.url) + "/../..";

const CJK_SIZES = [14, 16, 20, 24];
const CJK_PATH  = findCjkFont();
const cjkFonts  = CJK_PATH ? loadFontSizes(CJK_PATH, CJK_SIZES) : {};
const cjkReady  = CJK_SIZES.every((size) => !!cjkFonts[size]);

if (cjkReady) {
    setDefaultFont(cjkFonts);
    console.log(`theme: CJK role fonts 14/16/20/24 loaded from ${CJK_PATH}`);
} else {
    console.log("SKIP CJK font unavailable - falling back to the built-in Montserrat faces");
}

/* One text-face helper for both worlds: a real Tiny-TTF handle when a CJK
 * font exists, otherwise the compiled-in Montserrat of the same pixel size
 * so the visual hierarchy survives on a font-less host. */
function face(px) {
    return cjkReady ? { font: cjkFonts[px] } : { fontSize: px };
}

const imgHero   = loadImage(`${ASSETS}/test.png`);
const imgFrames = loadImages([
    `${ASSETS}/frame1.png`,
    `${ASSETS}/frame2.png`,
    `${ASSETS}/frame3.png`,
]);
const imgReleased = loadImage(`${ASSETS}/released.png`);
const imgPressed  = loadImage(`${ASSETS}/pressed.png`);

if (!imgHero || !imgReleased || !imgPressed || imgFrames.some((f) => !f)) {
    console.log("SKIP image assets unavailable - the media card renders empty frames");
}

/* ── Theme state ────────────────────────────────────────────────────────── */

/* The accent palette is the theme's OWN semantic ramps, addressed by token
 * name — a literal hex here would mean the showcase demonstrates tokens with
 * values the token system never produced. `primary` is the single token the
 * cycler overwrites, so its Element Plus value is snapshotted once, before any
 * patch can run; the other three stay live lookups. */
const ACCENTS = [
    { label: "经典蓝", token: "primary"      },
    { label: "成功绿", token: "success.base" },
    { label: "警示橙", token: "warning.base" },
    { label: "危险红", token: "danger.base"  },
];

const PRISTINE_PRIMARY = lv.getThemeToken("primary");

function accentColor(index) {
    const { token } = ACCENTS[index];
    return token === "primary" ? PRISTINE_PRIMARY : lv.getThemeToken(token);
}

/* Integer roles read from the same registry the theme paints with. Layout
 * metrics that ARE tokens must come from the tokens; only genuine content
 * dimensions (a 320px calendar, a 180px chart) stay literal below. */
const METRIC = {
    radius: lv.getThemeMetric("radius_base"),
    border: lv.getThemeMetric("border_width"),
    xs:     lv.getThemeMetric("space_xs"),
    sm:     lv.getThemeMetric("space_sm"),
    md:     lv.getThemeMetric("space_md"),
    lg:     lv.getThemeMetric("space_lg"),
};

const [scheme, setSchemeSignal]   = createSignal("light");
const [accent, setAccentSignal]   = createSignal(0);

/* `lv_chart_add_series` COPIES the colour into the series struct, so the chart
 * is the one control `lv_obj_report_style_change(NULL)` cannot reach. Every
 * accent or scheme change has to re-push the series colours explicitly, or the
 * trend line keeps whatever accent was active when it was first mounted. */
function repaintChartSeries() {
    if (extra.chartVisits === undefined) return;
    chartSetSeriesColor(refs.chart, extra.chartVisits,      lv.getThemeToken("primary"));
    chartSetSeriesColor(refs.chart, extra.chartConversions, lv.getThemeToken("success.base"));
}

/* `setTheme` copies a whole preset over the working token set, so the chosen
 * accent has to be re-applied after every scheme change — otherwise flipping
 * to dark would silently revert the user's primary patch. */
function applyScheme(next) {
    setTheme(next);
    setSchemeSignal(next);
    setThemeToken("primary", accentColor(accent()));
    repaintChartSeries();
}

function toggleScheme() {
    applyScheme(scheme() === "light" ? "dark" : "light");
}

function cycleAccent() {
    const next = (accent() + 1) % ACCENTS.length;
    setAccentSignal(next);
    setThemeToken("primary", accentColor(next));
    repaintChartSeries();
}

/* ── Widget handles ─────────────────────────────────────────────────────── */

/** Exactly the 31 public host tags — the smoke manifest asserts this. */
const refs  = {};
/** Everything else the scripted checks need (internals, extra pages, …). */
const extra = {};

const keep = (name) => (node) => { refs[name]  = node; };
const hold = (name) => (node) => { extra[name] = node; };

/* ── Layout primitives (components, not host tags) ──────────────────────── */

function Card({ title, cardRef, gap = METRIC.md, children }) {
    const props = {
        style: {
            width:           "100%",
            height:          "auto",
            flexFlow:        "column",
            gap,
            padding:         METRIC.lg,
            borderRadius:    METRIC.radius,
            borderWidth:     METRIC.border,
            backgroundColor: "$bg_overlay",
            borderColor:     "$border_light",
        },
    };
    if (cardRef) props.ref = cardRef;

    return h("view", props,
        title
            ? h("view", { style: { width: "100%", height: "auto", flexFlow: "column", gap: METRIC.sm } },
                h("text", { style: { ...face(16), textColor: "$text_primary" }, text: title }),
                h("view", { style: { width: "100%", height: METRIC.border, backgroundColor: "$border_lighter" } }),
              )
            : null,
        children,
    );
}

function Field({ label, height = 40, gap = METRIC.md, children }) {
    return h("view", {
        style: { width: "100%", height, flexFlow: "row", gap, alignItems: "center" },
    },
        label ? h("text", { style: { width: 110, textColor: "$text_regular" }, text: label }) : null,
        children,
    );
}

function Hint({ text }) {
    return h("text", { style: { width: "100%", textColor: "$text_secondary" }, text });
}

function Page({ pageRef, children }) {
    const props = {
        style: {
            width:           "100%",
            height:          "100%",
            flexFlow:        "column",
            gap:             METRIC.lg,
            scrollable:      true,
            backgroundColor: "$bg_page",
        },
    };
    if (pageRef) props.ref = pageRef;
    return h("view", props, children);
}

/* ── Header ─────────────────────────────────────────────────────────────── */

function Header() {
    return h("view", {
        style: { width: "100%", height: "auto", flexFlow: "column", backgroundColor: "$bg_overlay" },
    },
        h("view", {
            style: {
                width: "100%", height: 88, padding: METRIC.lg,
                flexFlow: "row", gap: METRIC.md, alignItems: "center",
            },
        },
            h("view", { style: { flexGrow: 1, height: "auto", flexFlow: "column", gap: METRIC.xs } },
                h("text", {
                    style: { ...face(24), textColor: "$text_primary" },
                    text:  "石语 · 主题总览",
                }),
                h("text", {
                    style: { textColor: "$text_secondary" },
                    text:  () => `Element Plus 明暗双主题 · 当前${scheme() === "light" ? "浅色" : "深色"}模式 · 主色${ACCENTS[accent()].label}`,
                }),
            ),
            h("led", {
                ref:        keep("led"),
                style:      { width: 16, height: 16 },
                color:      () => accentColor(accent()),
                brightness: 220,            }),
            h("button", {
                style:   { width: 132, height: 36 },
                text:    () => (scheme() === "light" ? "切换深色模式" : "切换浅色模式"),
                onClick: toggleScheme,
            }),
            h("button", {
                style:   { width: 110, height: 36 },
                text:    "更换主色",
                onClick: cycleAccent,
            }),
        ),
        h("view", { style: { width: "100%", height: METRIC.border, backgroundColor: "$border_light" } }),
    );
}

/* ── Tab 1 — 基础控件 ───────────────────────────────────────────────────── */

const INPUT_TEXT = "你好，世界";

function BasicsTab() {
    const [volume,  setVolume]  = createSignal(35);
    const [ratio,   setRatio]   = createSignal(60);
    const [notify,  setNotify]  = createSignal(true);
    const [agree,   setAgree]   = createSignal(false);
    const [count,   setCount]   = createSignal(18);

    /* Exposed so the scripted checks can drive real signal updates. */
    extra.setVolume = setVolume;
    extra.setRatio  = setRatio;
    extra.setNotify = setNotify;
    extra.volume    = volume;

    return h(Page, { pageRef: hold("page") },
        h(Card, { title: "文字排版" },
            h("text", {
                ref:   keep("text"),
                style: { width: "100%" },
                text:  "你好，世界 —— 主题化文本自动继承 text_primary，深浅两套配色都不需要改动一行业务代码。",
            }),
            h(Field, { label: "字号 14", height: 26 },
                h("text", { ref: hold("face14"), style: { ...face(14), textColor: "$text_regular" },
                            text: "正文 · 中文示例" })),
            h(Field, { label: "字号 16", height: 28 },
                h("text", { ref: hold("face16"), style: { ...face(16), textColor: "$text_regular" },
                            text: "小标题 · 中文示例" })),
            h(Field, { label: "字号 20", height: 34 },
                h("text", { ref: hold("face20"), style: { ...face(20), textColor: "$text_primary" },
                            text: "章节标题 · 中文示例" })),
            h(Field, { label: "字号 24", height: 40 },
                h("text", { ref: hold("face24"), style: { ...face(24), textColor: "$text_primary" },
                            text: "展示标题 · 中文示例" })),
        ),

        h(Card, { title: "按钮与对话框" },
            h(Field, { label: "操作" },
                h("button", { ref: keep("button"), style: { width: 120, height: 36 }, text: "主要按钮" }),
                h("button", {
                    style:   { width: 130, height: 36 },
                    text:    "打开对话框",
                    onClick: () => showMsgbox(
                        {
                            title:   "系统提示",
                            text:    "遮罩、面板、页眉与页脚均由主题绘制，并随深浅色模式同步重绘。",
                            buttons: ["取消", "确定"],
                        },
                        (index) => console.log(`theme: 对话框返回按钮索引 ${index}`),
                    ),
                }),
            ),
            h(Hint, { text: "按钮的悬停色取 primary.light_3，按下色取 primary.dark_2；深色模式下两者自动互换方向。" }),
        ),

        h(Card, { title: "输入与表单" },
            h(Field, { label: "用户名" },
                h("input", {
                    ref:         keep("input"),
                    style:       {
                        flexGrow: 1,
                        height:   40,
                        partStyles: {
                            placeholder: { textColor: "$text_placeholder" },
                            scrollbar:   { backgroundColor: "$border_dark" },
                            cursor:      { focus: { backgroundColor: "$primary" } },
                        },
                    },
                    oneLine:     true,
                    text:        INPUT_TEXT,
                    placeholder: "请输入内容…",
                }),
            ),
            h(Field, { label: "消息通知" },
                h("switch", {
                    ref:      keep("switch"),
                    checked:  () => notify(),
                    onChange: (value) => setNotify(value),
                }),
                h("text", { style: { width: 70, textColor: "$text_secondary" },
                            text: () => (notify() ? "已开启" : "已关闭") }),
            ),
            h(Field, { label: "用户协议" },
                h("checkbox", {
                    ref:      keep("checkbox"),
                    text:     "我已阅读并同意用户协议",
                    checked:  () => agree(),
                    onChange: (value) => setAgree(value),
                }),
            ),
            h(Show, { when: () => notify() },
                () => h(Hint, { text: "提示：通知开启后才会收到系统消息 —— 这一段由 <Show> 动态挂载。" }),
            ),
        ),

        h(Card, { title: "进度与数值" },
            h(Field, { label: "下载进度" },
                h("progress", {
                    ref:   keep("progress"),
                    style: { flexGrow: 1, height: 6 },
                    min:   0, max: 100,
                    value: () => volume(),
                }),
                h("text", { style: { width: 56, textColor: "$text_secondary" },
                            text: () => `${volume()}%` }),
            ),
            h(Field, { label: "音量" },
                h("slider", {
                    ref:      keep("slider"),
                    style:    {
                        flexGrow: 1,
                        partStyles: {
                            indicator: { backgroundColor: "$primary" },
                            knob:      {
                                backgroundColor: "$bg_overlay",
                                borderColor:     "$primary",
                                pressed:         { borderColor: "$primary.dark_2" },
                            },
                        },
                    },
                    min:      0, max: 100,
                    value:    () => volume(),
                    onChange: (value) => setVolume(value),
                }),
                h("text", { style: { width: 56, textColor: "$text_secondary" },
                            text: () => `${volume()}` }),
            ),
            h(Field, { label: "完成度", height: 110 },
                h("arc", {
                    ref:      keep("arc"),
                    style:    { width: 96, height: 96 },
                    min:      0, max: 100,
                    value:    () => ratio(),
                    onChange: (value) => setRatio(value),
                }),
                h("text", { style: { width: 56, textColor: "$text_secondary" },
                            text: () => `${ratio()}%` }),
                h("spinner", { ref: keep("spinner"), style: { width: 48, height: 48 }, spinTime: 1400 }),
                h("text", { style: { textColor: "$text_secondary" }, text: "加载中…" }),
            ),
            h(Field, { label: "数量" },
                h("spinbox", {
                    ref:      keep("spinbox"),
                    style:    { width: 150, height: 40 },
                    digits:   "3.0",
                    step:     1,
                    value:    () => count(),
                    onChange: (value) => setCount(value),
                }),
                h("text", { style: { textColor: "$text_secondary" }, text: () => `共 ${count()} 件` }),
            ),
        ),
    );
}

/* ── Tab 2 — 选择与日期 ─────────────────────────────────────────────────── */

const FRUITS = ["苹果", "香蕉", "樱桃", "榴莲"];
const CITIES = ["北京", "上海", "广州", "深圳", "杭州"];
const STATUS = [
    { label: "服务正常", token: "success.base" },
    { label: "响应偏慢", token: "warning.base" },
    { label: "节点离线", token: "danger.base"  },
];

function ChoicesTab() {
    const [fruit,  setFruit]  = createSignal(0);
    const [city,   setCity]   = createSignal(2);
    const [picked, setPicked] = createSignal("尚未选择");

    extra.setFruit = setFruit;

    return h(Page, {},
        h(Card, { title: "下拉与滚轮" },
            h(Field, { label: "水果" },
                h("dropdown", {
                    ref:      keep("dropdown"),
                    style:    { width: 180, height: 40 },
                    options:  FRUITS.join("\n"),
                    value:    () => fruit(),
                    onChange: (index) => setFruit(index),
                }),
                h("text", { style: { textColor: "$text_secondary" },
                            text: () => `已选：${FRUITS[fruit()]}` }),
            ),
            h(Field, { label: "城市", height: 150 },
                h("roller", {
                    ref:      keep("roller"),
                    style:    { width: 180, height: 140 },
                    options:  CITIES.join("\n"),
                    value:    () => city(),
                    onChange: (index) => setCity(index),
                }),
                h("text", { style: { textColor: "$text_secondary" },
                            text: () => `已选：${CITIES[city()]}` }),
            ),
            h(Hint, { text: "下拉弹层是独立的 lv_dropdownlist 对象，选中行取 primary.light_9，文字取 primary。" }),
        ),

        h(Card, { title: "状态指示" },
            h("view", { style: { width: "100%", height: 36, flexFlow: "row", gap: 24, alignItems: "center" } },
                h(For, { each: STATUS, key: (item) => item.label }, (item) =>
                    h("view", { style: { width: 120, height: 30, flexFlow: "row", gap: METRIC.sm, alignItems: "center" } },
                        h("led", { style: { width: 12, height: 12 },
                                   color: () => lv.getThemeToken(item.token), brightness: 210 }),
                        h("text", { style: { textColor: "$text_regular" }, text: item.label }),
                    ),
                ),
            ),
        ),

        h(Card, { title: "日历" },
            h("calendar", {
                ref:      keep("calendar"),
                style:    { width: 320, height: 300 },
                today:    "2026-08-18",
                shown:    "2026-08",
                onChange: (date) => {
                    if (date && date.year) {
                        setPicked(`${date.year} 年 ${date.month} 月 ${date.day} 日`);
                    }
                },
            }),
            h(Hint, { text: () => `选中日期：${picked()}` }),
        ),
    );
}

/* ── Tab 3 — 数据展示 ───────────────────────────────────────────────────── */

function DataTab() {
    const visits      = [10, 28, 16, 42, 35, 58, 47, 70, 60, 82, 75, 95];
    const conversions = [5, 14, 12, 26, 22, 31, 30, 44, 41, 52, 50, 63];
    const [selected, setSelected] = createSignal("尚未选择");

    return h(Page, {},
        h(Card, { title: "趋势图" },
            h("chart", {
                ref: (node) => {
                    refs.chart = node;
                    /* Handles are kept so `repaintChartSeries` can re-push the
                     * colours; a series never follows a theme repaint by itself. */
                    extra.chartVisits      = chartAddSeries(node, lv.getThemeToken("primary"));
                    extra.chartConversions = chartAddSeries(node, lv.getThemeToken("success.base"));
                    chartSetData(node, extra.chartVisits,      visits);
                    chartSetData(node, extra.chartConversions, conversions);
                },
                style:     { width: "100%", height: 180 },
                chartType: "line",
                rangeMax:  100,
                divLines:  "4x6",
            }),
            h(Hint, { text: "两条序列分别取 primary 与 success，网格线取 border_lighter。" }),
        ),

        h(Card, { title: "刻度" },
            h("scale", {
                ref:        keep("scale"),
                style:      { width: "100%", height: 44 },
                scaleMode:  "h-bottom",
                min:        0, max: 100,
                totalTicks: 11,
                majorEvery: 2,
                showLabels: true,
            }),
        ),

        h(Card, { title: "富文本" },
            h("span", {
                ref:   keep("span"),
                style: { width: "100%", height: 44 },
                /* `lv.getThemeToken` is a plain call with no subscription, so
                 * the two theme signals are read explicitly to give this effect
                 * its dependencies — without them the spans would resolve once
                 * and keep the boot accent forever. Re-running is safe: the
                 * `parts` setter deletes every existing span before rebuilding
                 * (lv_bindings.c), so it cannot duplicate text. */
                parts: () => {
                    accent();
                    scheme();
                    return [
                        { text: "石语 ",     color: lv.getThemeToken("primary"),
                          font: cjkReady ? cjkFonts[20] : undefined, fontSize: 20 },
                        { text: "= LVGL ",   color: lv.getThemeToken("success.base"),
                          font: cjkReady ? cjkFonts[20] : undefined, fontSize: 20 },
                        { text: "+ QuickJS", color: lv.getThemeToken("warning.base"),
                          font: cjkReady ? cjkFonts[20] : undefined, fontSize: 20 },
                    ];
                },
            }),
        ),

        h(Card, { title: "折线" },
            h("line", {
                ref:    keep("line"),
                style:  { width: 260, height: 70 },
                points: [[0, 60], [40, 26], [80, 46], [120, 12], [160, 40], [200, 6], [250, 30]],
            }),
            h(Hint, { text: "折线颜色由主题的 primary 直接驱动，更换主色时会立即重绘。" }),
        ),

        h(Card, { title: "表格" },
            h("table", {
                ref:   keep("table"),
                style: {
                    width: "100%", height: 140,
                    partStyles: {
                        items: { textColor: "$text_regular", borderColor: "$border_lighter" },
                    },
                },
                cells: [
                    ["名称",     "状态",   "数量"],
                    ["华东节点", "正常",   "128"],
                    ["华北节点", "维护中", "64"],
                    ["华南节点", "正常",   "96"],
                ],
            }),
        ),

        h(Card, { title: "列表" },
            h("list", { ref: keep("list"), style: { width: "100%", height: 170 } },
                h("listButton", { ref: keep("listButton"), text: "收件箱",
                                  onClick: () => setSelected("收件箱") }),
                h("listButton", { text: "已发送", onClick: () => setSelected("已发送") }),
                h("listButton", { text: "草稿箱", onClick: () => setSelected("草稿箱") }),
                h("listButton", { text: "垃圾箱", onClick: () => setSelected("垃圾箱") }),
            ),
            h(Hint, { text: () => `当前选择：${selected()}` }),
        ),
    );
}

/* ── Tab 4 — 组合与导航 ─────────────────────────────────────────────────── */

const KEYPAD = ["7", "8", "9", "\n", "4", "5", "6", "\n", "1", "2", "3", "\n", "清除", "0", "确定"];
const KEYPAD_KEYS = KEYPAD.filter((key) => key !== "\n");

function CompositeTab() {
    const [tapped, setTapped] = createSignal("尚未按键");

    return h(Page, {},
        h(Card, { title: "数字键区" },
            h("buttonMatrix", {
                ref:      keep("buttonMatrix"),
                style:    { width: "100%", height: 210 },
                map:      KEYPAD,
                onChange: (index) => setTapped(KEYPAD_KEYS[index] ?? "尚未按键"),
            }),
            h(Hint, { text: () => `最近按下：${tapped()}` }),
        ),

        h(Card, { title: "多页菜单" },
            h("view", { style: { width: "100%", height: 40, flexFlow: "row", gap: 12, alignItems: "center" } },
                h("button", {
                    style:   { width: 110, height: 34 },
                    text:    "常规设置",
                    onClick: () => setMenuPage(refs.menu, refs.menuPage),
                }),
                h("button", {
                    style:   { width: 110, height: 34 },
                    text:    "显示设置",
                    onClick: () => setMenuPage(refs.menu, extra.menuPageSecond),
                }),
            ),
            h("menu", { ref: keep("menu"), style: { width: "100%", height: 220 } },
                h("menuPage", { ref: keep("menuPage"), title: "常规设置" },
                    h("text", { style: { width: "100%" },
                                text: "常规页：菜单容器、页眉、分节与条目全部由主题着色。" }),
                ),
                h("menuPage", { ref: hold("menuPageSecond"), title: "显示设置" },
                    h("text", { style: { width: "100%" },
                                text: "显示页：切换深浅色后本页同样会重绘。" }),
                ),
            ),
        ),
    );
}

/* ── Tab 5 — 图像与键盘 ─────────────────────────────────────────────────── */

function MediaTab() {
    return h(Page, {},
        h(Card, { title: "图像家族" },
            h("view", { style: { width: "100%", height: 96, flexFlow: "row", gap: 24, alignItems: "center" } },
                h("view", { style: { width: 96, height: 90, flexFlow: "column", gap: 6, alignItems: "center" } },
                    h("image", { ref: keep("image"), src: imgHero, style: { width: 64, height: 64 } }),
                    h("text", { style: { textColor: "$text_secondary" }, text: "静态图" }),
                ),
                h("view", { style: { width: 96, height: 90, flexFlow: "column", gap: 6, alignItems: "center" } },
                    h("animimg", {
                        ref:      keep("animimg"),
                        src:      imgFrames,
                        duration: 900,
                        repeat:   "infinite",
                        start:    true,
                        style:    { width: 64, height: 64 },
                    }),
                    h("text", { style: { textColor: "$text_secondary" }, text: "帧动画" }),
                ),
                h("view", { style: { width: 110, height: 90, flexFlow: "column", gap: 6, alignItems: "center" } },
                    h("imagebutton", {
                        ref:       keep("imagebutton"),
                        released:  imgReleased,
                        pressed:   imgPressed,
                        checkable: true,
                        style:     { width: 64, height: 64 },
                    }),
                    h("text", { style: { textColor: "$text_secondary" }, text: "图片按钮" }),
                ),
            ),
            h(Hint, { text: "图像三兄弟的底板保持完全透明，主题只在按下 / 禁用时按 disabled_opa 压暗像素。" }),
        ),

        h(Card, { title: "动画引擎" },
            h("view", { style: { width: "100%", height: 60, flexFlow: "row", gap: 16, alignItems: "center" } },
                h("view", {
                    ref:   hold("animBar"),
                    style: { width: 80, height: 24, backgroundColor: "$primary.base", borderRadius: 6 },
                }),
                h("button", {
                    style:   { width: 120, height: 40 },
                    text:    "播放动画",
                    onClick: () => {
                        if (!extra.animBar) return;
                        createAnimation(extra.animBar, {
                            property: "width", from: 80, to: 260,
                            duration: 400, easing: "ease-out", repeat: 1,
                            onComplete: () => console.log("ANIM_COMPLETED width(80→260)"),
                        });
                    },
                }),
            ),
            h(Hint, { text: "createAnimation 直接驱动 lv_anim 引擎：点击后条宽 80→260px，ease-out，完成时回调打印 ANIM_COMPLETED。" }),
        ),

        h(Card, { title: "屏幕键盘" },
            h("keyboard", {
                ref:    keep("keyboard"),
                /* Thunk: the input's ref fires while tab 1 mounts, which is
                 * before this tab's props are read by the mount effect. */
                target: () => refs.input,
                style:  {
                    width: "100%", height: 220,
                    partStyles: {
                        items: {
                            backgroundColor: "$bg_overlay",
                            textColor:       "$text_primary",
                            pressed:         { backgroundColor: "$primary.light_9" },
                        },
                    },
                },
            }),
            h(Hint, { text: "键盘绑定第一页的「用户名」输入框，键面颜色来自 $bg_overlay，按下态来自 $primary.light_9。" }),
        ),
    );
}

/* ── App shell ──────────────────────────────────────────────────────────── */

function App() {
    return h("view", {
        ref:   keep("view"),
        style: { width: "100%", height: "100%", flexFlow: "column" },
    },
        h(Header),
        h("tabview", {
            ref:        keep("tabview"),
            style:      { flexGrow: 1, width: "100%" },
            tabBarSize: 48,
        },
            h("tab", { ref: keep("tab"), title: "基础控件" }, h(BasicsTab)),
            h("tab", { title: "选择与日期" }, h(ChoicesTab)),
            h("tab", { title: "数据展示"   }, h(DataTab)),
            h("tab", { title: "组合与导航" }, h(CompositeTab)),
            h("tab", { title: "图像与键盘" }, h(MediaTab)),
        ),
    );
}

render(App);

if (!SMOKE) {
    console.log("theme: 交互模式运行中 —— 点击右上角按钮切换深浅色 / 更换主色");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scripted smoke mode — everything below only runs with STONEGUI_SHOWCASE_SMOKE=1
 * ═════════════════════════════════════════════════════════════════════════ */

/** Exact values from doc/theme.md §3-§5, keyed by scheme. */
const TOKEN = {
    bg_page:          { light: "#f2f3f5", dark: "#0a0a0a" },
    bg_base:          { light: "#ffffff", dark: "#141414" },
    bg_overlay:       { light: "#ffffff", dark: "#1d1e1f" },
    text_primary:     { light: "#303133", dark: "#e5eaf3" },
    text_regular:     { light: "#606266", dark: "#cfd3dc" },
    text_placeholder: { light: "#a8abb2", dark: "#8d9095" },
    text_disabled:    { light: "#c0c4cc", dark: "#6c6e72" },
    border_light:     { light: "#e4e7ed", dark: "#414243" },
    border_lighter:   { light: "#ebeef5", dark: "#363637" },
    border_dark:      { light: "#d4d7de", dark: "#58585b" },
    fill_light:       { light: "#f5f7fa", dark: "#262727" },
    primary:          { light: "#409eff", dark: "#409eff" },
    primary_light_9:  { light: "#ecf5ff", dark: "#18222b" },
    primary_dark_2:   { light: "#337ecc", dark: "#66b1ff" },
    white:            { light: "#ffffff", dark: "#ffffff" },
};

const HOST_TAG_COUNT = 31;

let scenariosPassed = 0;
let scenariosFailed = 0;
let scenariosSkipped = 0;

function scenario(name, body) {
    const checks = [];
    const expect = (condition, detail) => {
        checks.push({ ok: !!condition, detail });
        return !!condition;
    };
    const equal = (actual, wanted, detail) =>
        expect(actual === wanted, `${detail} (expected ${wanted}, got ${actual})`);

    let thrown = null;
    try { body(expect, equal); }
    catch (error) { thrown = error; }

    const failed = checks.filter((check) => !check.ok);
    if (!thrown && checks.length > 0 && failed.length === 0) {
        scenariosPassed++;
        console.log(`  PASS ${name} — ${checks.length} checks`);
        return;
    }
    scenariosFailed++;
    console.log(`  FAIL ${name}`);
    if (checks.length === 0 && !thrown) console.log("       no checks were executed");
    for (const check of failed) console.log(`       ${check.detail}`);
    if (thrown) console.log(`       threw ${String(thrown)}`);
}

function skipScenario(name, reason) {
    scenariosSkipped++;
    console.log(`  SKIP ${name} — ${reason}`);
}

function childCount(node) {
    let count = 0;
    while (lv.getChild(node, count) !== null) count++;
    return count;
}

/* The one deliberate failure path: an unknown $token must raise instead of
 * silently resolving to black. Runs first so the negative mode is cheap. */
function invalidTokenProbe() {
    console.log("\n== deliberate invalid theme token ==");
    let thrown = null;
    try {
        render(() => h("view", { style: { backgroundColor: "$not_a_theme_token" } }));
    } catch (error) {
        thrown = error;
    }
    if (thrown) {
        console.log(`  UNKNOWN THEME TOKEN: ${String(thrown)}`);
        console.log("SHOWCASE SMOKE FAILED — deliberate invalid-token probe");
    } else {
        console.log("  UNKNOWN THEME TOKEN was accepted silently — the resolver has a black-fallback bug");
        console.log("SHOWCASE SMOKE FAILED — invalid-token probe did not throw");
    }
    std.exit(1);
}

function runSmoke() {
    if (SMOKE_INVALID_TOKEN) invalidTokenProbe();

    console.log("\n== stonegui theme smoke ==");

    /* 1 — every public host tag is really on screen. */
    scenario("host-tags: all 31 public tags constructed", (expect, equal) => {
        const source = std.loadFile(`${REPO}/js/framework.js`) ?? "";
        const body   = /const HOST_TAGS = \{([\s\S]*?)\n\};/.exec(source)?.[1] ?? "";
        const tags   = [...body.matchAll(/^\s*([A-Za-z][A-Za-z0-9]*):/gm)].map((m) => m[1]);

        equal(tags.length, HOST_TAG_COUNT, "HOST_TAGS entries found in js/framework.js");
        equal(Object.keys(refs).length, HOST_TAG_COUNT, "showcase kept one handle per host tag");
        for (const tag of tags) {
            expect(typeof refs[tag] === "number" && refs[tag] !== 0,
                   `host tag <${tag}> was constructed (got ${refs[tag]})`);
        }
        equal(new Set(Object.values(refs)).size, HOST_TAG_COUNT,
              "every host-tag handle is a distinct LVGL object");
    });

    /* 2 — CJK role fonts at 14/16/20/24. */
    if (!cjkReady) {
        skipScenario("cjk-role-fonts", "SKIP CJK font unavailable");
    } else {
        scenario("cjk-role-fonts: 14/16/20/24 map drives inherited text", (expect, equal) => {
            equal(loadFont(CJK_PATH, 14), cjkFonts[14], "loadFont reuses the cached (path,14) handle");
            equal(loadFont(CJK_PATH, 24), cjkFonts[24], "loadFont reuses the cached (path,24) handle");

            const heights = CJK_SIZES.map((size) =>
                getProperty(extra[`face${size}`], "fontLineHeight"));
            equal(new Set(heights).size, CJK_SIZES.length, "four faces give four distinct line heights");
            for (let i = 1; i < heights.length; i++) {
                expect(heights[i] > heights[i - 1],
                       `line height grows with size (${CJK_SIZES[i]} > ${CJK_SIZES[i - 1]})`);
            }

            const base = heights[0];
            equal(getProperty(refs.text, "fontLineHeight"), base,
                  "inherited body text uses the font.base (14) role");
            equal(getProperty(lv.getChild(refs.button, 0), "fontLineHeight"), base,
                  "the button caption label also inherits font.base");
        });

        /* 2b — the scenario above CANNOT fail on the bug it appears to cover.
         * LV_FONT_DEFAULT is Montserrat-14 and measures 16, which is exactly
         * what this CJK face measures at 14; Montserrat-16 and CJK@16 collide
         * at 18 too. So `=== base` holds whether the theme states font.base or
         * the inheritance walk falls off layer_top into the Latin fallback —
         * the precise failure that rendered every msgbox body and footer
         * caption as tofu. Remap the base role onto a metric the fallback
         * cannot produce and require already-created labels to MOVE. */
        scenario("cjk-anti-vacuity: msgbox internals state font.base, not LV_FONT_DEFAULT",
                 (expect, equal) => {
            let latin = null;
            const disposeLatin = render(() => h("view", {},
                h("text", { ref: (n) => latin = n, text: "Base", style: { fontSize: 14 } }),
            ));
            const fallback = getProperty(latin, "fontLineHeight");
            const h14 = getProperty(extra.face14, "fontLineHeight");
            const h16 = getProperty(extra.face16, "fontLineHeight");
            equal(fallback, h14, "Montserrat-14 and CJK@14 share a line height — the trap");
            expect(h16 !== fallback, "the 16px role metric is one the Latin fallback cannot produce");

            const panel     = showMsgbox({ title: "字体探针", text: "中文字形探针",
                                           buttons: ["取消", "确定"] }, () => {});
            const backdrop  = lv.getParent(panel);
            const header    = lv.getChild(panel, 0);
            const content   = lv.getChild(panel, 1);
            const footer    = lv.getChild(panel, 2);
            const bodyCap   = lv.getChild(content, 0);
            const cancelCap = lv.getChild(lv.getChild(footer, 0), 0);
            const okCap     = lv.getChild(lv.getChild(footer, 1), 0);
            const closeBtn  = lv.getChild(header, 1);

            setDefaultFont({ 14: cjkFonts[16], 16: cjkFonts[16],
                             20: cjkFonts[20], 24: cjkFonts[24] });
            equal(getProperty(bodyCap, "fontLineHeight"), h16,
                  "the msgbox body moved off the fallback metric");
            equal(getProperty(cancelCap, "fontLineHeight"), h16,
                  "the msgbox 取消 caption moved off the fallback metric");
            equal(getProperty(okCap, "fontLineHeight"), h16,
                  "the msgbox 确定 caption moved off the fallback metric");
            equal(getProperty(content, "fontLineHeight"), h16,
                  "the msgbox content container states font.base itself");
            equal(getProperty(footer, "fontLineHeight"), h16,
                  "the msgbox footer container states font.base itself");
            equal(getProperty(closeBtn, "fontLineHeight"), h16,
                  "the msgbox close affordance states font.base, not the header's font.large");
            equal(getProperty(refs.text, "fontLineHeight"), h16,
                  "the live showcase body text moved with the role map");
            equal(getProperty(lv.getChild(refs.button, 0), "fontLineHeight"), h16,
                  "the live button caption moved with the role map");

            setDefaultFont({ 14: cjkFonts[14], 16: cjkFonts[16],
                             20: cjkFonts[20], 24: cjkFonts[24] });
            equal(getProperty(bodyCap, "fontLineHeight"), h14,
                  "restoring the role map returns the msgbox body to font.base");
            equal(getProperty(okCap, "fontLineHeight"), h14,
                  "restoring the role map returns the 确定 caption to font.base");

            lv.dispose(backdrop);
            disposeLatin();
        });
    }

    /* 3 — light → dark → light on already-created widgets. */
    scenario("scheme-switch: light → dark → light repaints live widgets", (expect, equal) => {
        const probe = (name) => {
            equal(getProperty(extra.page, "backgroundColor"), TOKEN.bg_page[name],
                  `${name}: page surface is bg_page`);
            equal(getProperty(refs.text, "textColor"), TOKEN.text_primary[name],
                  `${name}: inherited label text is text_primary`);
            equal(getProperty(refs.button, "backgroundColor"), TOKEN.primary[name],
                  `${name}: button fill is primary`);
            equal(getProperty(refs.list, "backgroundColor"), TOKEN.bg_overlay[name],
                  `${name}: list surface is bg_overlay`);
            equal(getProperty(refs.keyboard, "backgroundColor"), TOKEN.bg_page[name],
                  `${name}: keyboard plate is bg_page`);
            equal(getProperty(refs.table, "backgroundColor"), TOKEN.bg_overlay[name],
                  `${name}: table surface is bg_overlay`);
        };

        probe("light");
        applyScheme("dark");
        probe("dark");
        applyScheme("light");
        probe("light");
    });

    /* 4 — runtime primary patch and revert. */
    scenario("token-patch: primary patch and revert reach theme + $token styles", (expect, equal) => {
        setThemeToken("primary", "#e74c3c");
        equal(getProperty(refs.button, "backgroundColor"), "#e74c3c",
              "themed button fill follows the patched primary");
        equal(getProperty(refs.progress, "backgroundColor", { part: "indicator" }), "#e74c3c",
              "themed progress indicator follows the patched primary");
        equal(getProperty(refs.line, "lineColor"), "#e74c3c",
              "themed polyline follows the patched primary");
        equal(getProperty(refs.slider, "backgroundColor", { part: "indicator" }), "#e74c3c",
              "$primary partStyle on the slider indicator follows the patch");

        setThemeToken("primary", accentColor(accent()));
        equal(getProperty(refs.button, "backgroundColor"), TOKEN.primary.light,
              "reverting the token restores the Element Plus primary");
        equal(getProperty(refs.slider, "backgroundColor", { part: "indicator" }), TOKEN.primary.light,
              "the $primary partStyle reverts with it");
    });

    /* 5 — $token references on main and sub-parts stay live. */
    scenario("token-references: $token main + part styles update live", (expect, equal) => {
        const probe = (name) => {
            equal(getProperty(extra.page, "backgroundColor"), TOKEN.bg_page[name],
                  `${name}: $bg_page on a MAIN surface`);
            equal(getProperty(refs.slider, "backgroundColor", { part: "knob" }), TOKEN.bg_overlay[name],
                  `${name}: $bg_overlay on the slider KNOB`);
            equal(getProperty(refs.slider, "borderColor", { part: "knob", state: "pressed" }),
                  TOKEN.primary_dark_2[name],
                  `${name}: $primary.dark_2 on KNOB|PRESSED`);
            equal(getProperty(refs.input, "textColor", { part: "placeholder" }),
                  TOKEN.text_placeholder[name],
                  `${name}: $text_placeholder on the textarea placeholder part`);
            equal(getProperty(refs.input, "backgroundColor", { part: "scrollbar" }),
                  TOKEN.border_dark[name],
                  `${name}: $border_dark on the textarea SCROLLBAR`);
            equal(getProperty(refs.table, "textColor", { part: "items" }), TOKEN.text_regular[name],
                  `${name}: $text_regular on the table ITEMS`);
            equal(getProperty(refs.table, "borderColor", { part: "items" }), TOKEN.border_lighter[name],
                  `${name}: $border_lighter on the table ITEMS grid`);
            equal(getProperty(refs.keyboard, "backgroundColor", { part: "items" }),
                  TOKEN.bg_overlay[name],
                  `${name}: $bg_overlay on the keyboard ITEMS`);
            equal(getProperty(refs.keyboard, "backgroundColor", { part: "items", state: "pressed" }),
                  TOKEN.primary_light_9[name],
                  `${name}: $primary.light_9 on ITEMS|PRESSED`);
            equal(getProperty(refs.switch, "backgroundColor", { part: "knob" }), TOKEN.white[name],
                  `${name}: $white on the switch KNOB`);
        };

        probe("light");
        applyScheme("dark");
        probe("dark");
        applyScheme("light");
        probe("light");
    });

    /* 6 — focus, cursor and injected keys on the Chinese input. */
    scenario("input: focus, cursor style and sendKey round-trip", (expect, equal) => {
        expect(focus(refs.input), "focus(input) reports the field as focused");
        equal(getProperty(refs.input, "backgroundColor", { part: "cursor", state: "focus" }),
              TOKEN.primary.light, "the caret picks up $primary while focused");

        sendKey("end");
        equal(getProperty(refs.input, "cursorPos"), [...INPUT_TEXT].length,
              "End moves the caret past the last CJK character");
        sendKey("home");
        equal(getProperty(refs.input, "cursorPos"), 0, "Home moves the caret back to 0");

        clipboard.write("");
        sendKey("a", true);
        sendKey("c", true);
        equal(clipboard.read(), INPUT_TEXT, "Ctrl+A then Ctrl+C copies the Chinese content");
        clipboard.write("");

        equal(getProperty(refs.input, "text"), INPUT_TEXT, "the field content survived the round-trip");
    });

    /* 7 — slider / switch driven through their signals. */
    scenario("controls: slider and switch state follow signal updates", (expect, equal) => {
        extra.setVolume(88);
        equal(getProperty(refs.slider, "value"), 88, "the slider follows its signal");
        equal(getProperty(refs.progress, "value"), 88, "the progress bar shares the same signal");
        extra.setVolume(35);
        equal(getProperty(refs.slider, "value"), 35, "the slider follows the signal back");

        extra.setRatio(75);
        equal(getProperty(refs.arc, "value"), 75, "the arc follows its signal");

        extra.setNotify(false);
        equal(getProperty(refs.switch, "checked"), false, "the switch turns off through its signal");
        extra.setNotify(true);
        equal(getProperty(refs.switch, "checked"), true, "and back on");

        equal(getProperty(refs.slider, "backgroundColor", { state: "disabled" }), TOKEN.fill_light.light,
              "the slider track uses fill_light when disabled");
        equal(getProperty(refs.switch, "backgroundColor", { part: "indicator", state: "disabled" }),
              TOKEN.text_disabled.light, "the switch indicator uses the text_disabled accent when disabled");
    });

    /* 8 — keyboard target binding plus key styling. */
    scenario("keyboard: target binding and key surfaces", (expect, equal) => {
        equal(getProperty(refs.keyboard, "target"), refs.input,
              "the keyboard is attached to the Chinese input");
        equal(getProperty(refs.keyboard, "backgroundColor"), TOKEN.bg_page.light,
              "the keyboard plate is bg_page");
        equal(getProperty(refs.keyboard, "backgroundColor", { part: "items" }), TOKEN.bg_overlay.light,
              "keys sit on $bg_overlay");
        equal(getProperty(refs.keyboard, "backgroundColor", { part: "items", state: "pressed" }),
              TOKEN.primary_light_9.light, "pressed keys use $primary.light_9");
    });

    /* 9 — the dropdown popup is a separate object; the getter redirects. */
    scenario("dropdown: popup surfaces follow the scheme", (expect, equal) => {
        extra.setFruit(2);
        equal(getProperty(refs.dropdown, "value"), 2, "the dropdown selection follows its signal");

        const probe = (name) => {
            equal(getProperty(refs.dropdown, "backgroundColor", { part: "selected", state: "checked" }),
                  TOKEN.primary_light_9[name], `${name}: popup selected row is primary.light_9`);
            equal(getProperty(refs.dropdown, "textColor", { part: "selected", state: "checked" }),
                  TOKEN.primary[name], `${name}: popup selected text is primary`);
            equal(getProperty(refs.dropdown, "backgroundColor", { part: "scrollbar" }),
                  TOKEN.border_dark[name], `${name}: popup scrollbar is border_dark`);
            equal(getProperty(refs.dropdown, "textColor", { part: "indicator" }),
                  TOKEN.text_placeholder[name], `${name}: the arrow is text_placeholder`);
        };

        probe("light");
        applyScheme("dark");
        probe("dark");
        applyScheme("light");
        extra.setFruit(0);
    });

    /* 10 — imperative menu page switching. */
    scenario("menu: setMenuPage swaps the visible page", (expect, equal) => {
        /* Derived, not indexed: the first page auto-activates, so its parent
         * IS the menu's main container. */
        const main = lv.getParent(refs.menuPage);
        expect(main !== null, "the auto-activated first page has a live main container");

        setMenuPage(refs.menu, extra.menuPageSecond);
        equal(lv.getParent(extra.menuPageSecond), main, "the second page moved into the main container");
        expect(lv.getParent(refs.menuPage) !== main, "the first page left the main container");

        setMenuPage(refs.menu, refs.menuPage);
        equal(lv.getParent(refs.menuPage), main, "switching back restores the first page");
        expect(lv.getParent(extra.menuPageSecond) !== main, "the second page stepped aside");
    });

    /* 11 — imperative msgbox open / close. */
    scenario("msgbox: open, theme the internals, close", (expect, equal) => {
        const panel    = showMsgbox({ title: "系统提示", text: "主题烟测", buttons: ["取消", "确定"] },
                                    () => {});
        expect(typeof panel === "number" && panel !== 0, "showMsgbox returned a live panel");

        const backdrop = lv.getParent(panel);
        const host     = lv.getParent(backdrop);
        const opened   = childCount(host);

        equal(getProperty(backdrop, "bgOpa"), 128, "the backdrop uses overlay_mask_opa");
        equal(getProperty(panel, "backgroundColor"), TOKEN.bg_overlay.light, "the panel is bg_overlay");
        equal(getProperty(panel, "radius"), 4, "the panel uses radius_base");
        equal(getProperty(panel, "shadowWidth"), 12, "the panel uses shadow_overlay_width");

        const header  = lv.getChild(panel, 0);
        const content = lv.getChild(panel, 1);
        const footer  = lv.getChild(panel, 2);
        equal(getProperty(header, "borderColor"), TOKEN.border_lighter.light,
              "the header rule is border_lighter");
        equal(getProperty(content, "textColor"), TOKEN.text_regular.light,
              "the content text is text_regular");
        equal(getProperty(lv.getChild(footer, 0), "backgroundColor"), TOKEN.primary.light,
              "footer buttons reuse the standalone button fill");

        applyScheme("dark");
        equal(getProperty(panel, "backgroundColor"), TOKEN.bg_overlay.dark,
              "the open panel repaints on a scheme swap");
        applyScheme("light");

        lv.dispose(backdrop);
        equal(childCount(host), opened - 1, "closing the msgbox removes its backdrop from the layer");
    });

    /* 12 — drive the REAL handlers. Scenarios 3/9/11 reach past the widgets
     * they describe (applyScheme instead of the onClick, a never-opened popup,
     * a disposed backdrop), which proves the styles but never the behaviour a
     * user actually triggers. These checks go through the same entry points. */
    scenario("interaction: real handlers drive tabs, scheme, accent, dropdown and msgbox",
             (expect, equal) => {
        /* Tab ACTIVATION is deliberately not driven here. `lv_tabview_set_active`
         * scrolls the content, that scroll emits LV_EVENT_SCROLL_END
         * synchronously when LV_ANIM_OFF, and LVGL's own
         * `cont_scroll_end_event_cb` (lv_tabview.c:312-355) calls
         * `lv_tabview_set_active` right back — unbounded recursion that
         * stack-overflows. Interactive mode never hits it because a live indev
         * and the timer handler make the scroll settle asynchronously; a
         * headless scripted run has neither. Editing LVGL is out of scope, so
         * the real activation path (`button_clicked_event_cb` → set_active) is
         * exercised by the XTEST capture runs instead, with a real pointer.
         * What IS safe to assert here is the readback the tab bar writes. */
        equal(getProperty(refs.tabview, "value"), 0,
              "the tabview reports its active tab through the shared value getter");
        expect(refs.tab !== undefined && refs.tab !== 0,
               "the first tab page is a live object the tab bar can activate");

        const schemeBefore = scheme();
        toggleScheme();
        equal(scheme(), schemeBefore === "light" ? "dark" : "light",
              "toggleScheme() — the actual onClick — flipped the scheme signal");
        equal(getProperty(extra.page, "backgroundColor"), TOKEN.bg_page[scheme()],
              "toggleScheme() repainted the live page surface");
        toggleScheme();
        equal(scheme(), schemeBefore, "toggleScheme() flips back");

        const accentBefore = accent();
        cycleAccent();
        equal(accent(), (accentBefore + 1) % ACCENTS.length,
              "cycleAccent() — the actual onClick — advanced the accent index");
        equal(lv.getThemeToken("primary"), accentColor(accent()),
              "cycleAccent() patched primary to the next semantic ramp");
        equal(getProperty(refs.button, "backgroundColor"), accentColor(accent()),
              "themed controls followed the new accent");
        equal(getProperty(refs.line, "lineColor"), accentColor(accent()),
              "the themed polyline followed the new accent");
        while (accent() !== accentBefore) cycleAccent();
        equal(lv.getThemeToken("primary"), accentColor(accentBefore),
              "cycling all the way round restores the original accent");

        expect(lv.sendEvent(refs.dropdown, "released"),
               "the dropdown accepted a synthetic release (LVGL's own open path)");
        equal(getProperty(refs.dropdown, "backgroundColor", { part: "selected", state: "checked" }),
              TOKEN.primary_light_9[scheme()],
              "the OPENED popup paints its selected row primary.light_9");
        expect(lv.sendEvent(refs.dropdown, "released"),
               "the dropdown accepted the closing release");

        let closedWith = null;
        const panel  = showMsgbox({ title: "关闭路径", text: "通过页脚按钮关闭",
                                    buttons: ["取消", "确定"] },
                                  (index) => { closedWith = index; });
        const footer = lv.getChild(panel, 2);
        expect(lv.sendEvent(lv.getChild(footer, 1), "click"),
               "the 确定 footer button accepted a click");
        equal(closedWith, 1, "closing through the footer button reported button index 1");

        let closedByX = null;
        const panelX = showMsgbox({ title: "关闭路径", text: "通过关闭图标关闭",
                                    buttons: ["确定"] },
                                  (index) => { closedByX = index; });
        const headerX = lv.getChild(panelX, 0);
        expect(lv.sendEvent(lv.getChild(headerX, 1), "click"),
               "the ✕ affordance accepted a click");
        equal(closedByX, -1, "closing through ✕ reported index -1");
    });

    console.log(`\n${scenariosPassed} scenarios passed, ${scenariosFailed} failed, ${scenariosSkipped} skipped`);
    console.log(scenariosFailed === 0 ? "SHOWCASE SMOKE PASSED" : "SHOWCASE SMOKE FAILED");
    std.exit(scenariosFailed === 0 ? 0 : 1);
}

if (SMOKE) runSmoke();
