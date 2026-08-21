/**
 * framework.js — Fine-grained reactive UI framework for stonegui.
 *
 * Reactive primitives:
 *   createSignal(init)            → [get, set]
 *   createEffect(fn)              → autotracked, owns nested resources
 *   createMemo(fn)                → cached derived signal
 *   createRoot(fn)                → explicit owner with manual dispose
 *   onCleanup(fn)                 → registers a teardown on the current owner
 *   untrack(fn)                   → read signals without subscribing
 *   batch(fn)                     → coalesce multiple sets into one effect run
 *
 * View layer:
 *   h(type, props, ...children)   → VNode (the JSX factory)
 *   Fragment                      → tag for grouping siblings
 *   render(rootFn, container?)    → mounts once, returns dispose()
 *
 * Native helpers (re-exports):
 *   loadFont, setDefaultFont, getProperty,
 *   chartAddSeries, chartSetData, showMsgbox
 *
 * Design (see doc/prompts.md — State Management):
 *   The tree is built ONCE. Reactive values are passed as accessor functions
 *   (thunks). Each reactive prop owns its own effect, so a signal change maps
 *   to a single `lv.setProperty(...)` call.
 *
 * Ownership model (Solid-style, see `Owner` below):
 *   Every effect runs inside an owner. Owners form a tree mirroring component
 *   nesting. Disposing an owner runs its `onCleanup` callbacks and recursively
 *   disposes its child owners — this is how a subtree is torn down (used by
 *   <For>, <Show>, hot reload, and the dispose function returned by render).
 */

import * as lv from "lvgl";

/* ── Ownership tree ─────────────────────────────────────────────────────── */

class Owner {
    constructor(parent) {
        this.parent   = parent;
        this.children = [];
        this.cleanups = [];
        if (parent) parent.children.push(this);
    }

    /* Tear down children (LIFO) before our own cleanups so resources are
     * released in reverse order of acquisition. Safe to call multiple times. */
    dispose() {
        for (let i = this.children.length - 1; i >= 0; i--) {
            this.children[i].dispose();
        }
        this.children.length = 0;
        for (let i = this.cleanups.length - 1; i >= 0; i--) {
            try { this.cleanups[i](); }
            catch (e) { console.error("onCleanup error:", e); }
        }
        this.cleanups.length = 0;
    }
}

let currentOwner   = null;
let currentEffect  = null;
let currentSources = null;

export function onCleanup(fn) {
    if (currentOwner) currentOwner.cleanups.push(fn);
}

export function untrack(fn) {
    const prev = currentEffect;
    currentEffect = null;
    try { return fn(); } finally { currentEffect = prev; }
}

export function createRoot(fn) {
    const owner    = new Owner(currentOwner);
    const prev     = currentOwner;
    currentOwner   = owner;
    try {
        return fn(() => owner.dispose());
    } finally {
        currentOwner = prev;
    }
}

/* ── Batching ───────────────────────────────────────────────────────────── */

let batchDepth = 0;
let batchedSubs = null;

export function batch(fn) {
    batchDepth++;
    if (batchDepth === 1) batchedSubs = new Set();
    try { return fn(); }
    finally {
        batchDepth--;
        if (batchDepth === 0) {
            const subs = batchedSubs;
            batchedSubs = null;
            for (const sub of subs) sub();
        }
    }
}

/* ── Signals ────────────────────────────────────────────────────────────── */

export function createSignal(init) {
    let value = init;
    const subs = new Set();

    const read = () => {
        if (currentEffect) {
            subs.add(currentEffect);
            if (currentSources) currentSources.add(subs);
        }
        return value;
    };

    const write = (next) => {
        const v = typeof next === "function" ? next(value) : next;
        if (v === value) return value;
        value = v;
        if (batchDepth > 0) {
            for (const sub of subs) batchedSubs.add(sub);
        } else {
            for (const sub of [...subs]) sub();
        }
        return value;
    };

    return [read, write];
}

/* ── Effects & memos ────────────────────────────────────────────────────── */

export function createEffect(fn) {
    const owner   = new Owner(currentOwner);
    const sources = new Set();
    let   disposed = false;

    const cleanup = () => {
        for (const src of sources) src.delete(run);
        sources.clear();
    };

    const run = () => {
        if (disposed) return;
        cleanup();
        owner.dispose();
        const prevOwner   = currentOwner;
        const prevEffect  = currentEffect;
        const prevSources = currentSources;
        currentOwner   = owner;
        currentEffect  = run;
        currentSources = sources;
        try { fn(); }
        finally {
            currentOwner   = prevOwner;
            currentEffect  = prevEffect;
            currentSources = prevSources;
        }
    };

    /* When the parent owner disposes — either externally via createRoot's
     * dispose, or because a parent effect is re-running — mark this effect
     * dead and unsubscribe from every signal it tracked. Without this, the
     * signal would keep `run` in its subscriber set and re-fire forever. */
    if (currentOwner) {
        currentOwner.cleanups.push(() => {
            disposed = true;
            cleanup();
        });
    }

    run();
}

export function createMemo(fn) {
    const [get, set] = createSignal();
    createEffect(() => set(fn()));
    return get;
}

/* ── VNode ──────────────────────────────────────────────────────────────── */

class VNode {
    constructor(type, props, children) {
        this.type     = type;
        this.props    = props || {};
        this.children = children || [];
        this.native   = null;
    }
}

/* ── Renderer ───────────────────────────────────────────────────────────── */

const [themeVersion, setThemeVersion] = createSignal(0);
const COLOR_STYLE_PROPERTIES = new Set(["backgroundColor", "borderColor", "textColor"]);

function resolveStyleValue(key, value) {
    if (!COLOR_STYLE_PROPERTIES.has(key) || typeof value !== "string" || !value.startsWith("$")) {
        return value;
    }
    themeVersion();
    return lv.getThemeToken(value.slice(1));
}

function bindProp(native, key, value, selector) {
    if (typeof value === "function") {
        createEffect(() => lv.setProperty(native, key, resolveStyleValue(key, value()), selector));
    } else if (COLOR_STYLE_PROPERTIES.has(key) && typeof value === "string" && value.startsWith("$")) {
        lv.getThemeToken(value.slice(1));
        createEffect(() => lv.setProperty(native, key, resolveStyleValue(key, value), selector));
    } else {
        lv.setProperty(native, key, value, selector);
    }
}

/* Pseudo-state keys recognised inside a `style` object — they nest another
 * style object whose entries are applied with the matching LVGL state
 * selector. Mirrors the C-side dispatch in js_setProperty. */
const PSEUDO_STATES = new Set(["default", "hover", "focus", "pressed", "checked", "disabled"]);

function applyStyleObject(native, style, selector) {
    for (const [key, value] of Object.entries(style)) {
        if (PSEUDO_STATES.has(key)) {
            if (!value || typeof value !== "object") {
                throw new TypeError(`style: state '${key}' must contain a style object`);
            }
            for (const [stateKey, stateValue] of Object.entries(value)) {
                if (stateValue && typeof stateValue === "object") {
                    throw new TypeError(`style: nested value '${stateKey}' is not supported`);
                }
                bindProp(native, stateKey, stateValue, {
                    part: selector?.part ?? "main",
                    state: key,
                });
            }
        } else {
            if (value && typeof value === "object") {
                throw new TypeError(`style: unknown state '${key}'`);
            }
            bindProp(native, key, value, selector);
        }
    }
}

function applyProp(native, key, value) {
    if (key === "children") return;

    if (key === "ref" && typeof value === "function") {
        value(native);
        return;
    }

    if (key === "style") {
        if (value) {
            for (const [sk, sv] of Object.entries(value)) {
                if (sk === "partStyles") {
                    if (!sv || typeof sv !== "object") {
                        throw new TypeError("style.partStyles must be an object");
                    }
                    for (const [part, partStyle] of Object.entries(sv)) {
                        if (!STYLE_PARTS.has(part)) {
                            throw new TypeError(`style.partStyles: unknown part '${part}'`);
                        }
                        if (!partStyle || typeof partStyle !== "object") {
                            throw new TypeError(`style.partStyles.${part} must be a style object`);
                        }
                        applyStyleObject(native, partStyle, { part, state: "default" });
                    }
                } else {
                    applyStyleObject(native, { [sk]: sv });
                }
            }
        }
        return;
    }

    if (key.startsWith("on") && typeof value === "function") {
        const eventName = key[2].toLowerCase() + key.slice(3).toLowerCase();
        lv.addEvent(native, eventName, value);
        return;
    }

    bindProp(native, key, value);
}

function mountVNode(vnode, parentNative) {
    if (vnode.type === Fragment) {
        for (const child of vnode.children) mountChild(child, parentNative);
        return parentNative;
    }

    if (vnode.type === SHOW_MARKER) {
        mountShow(vnode, parentNative);
        return parentNative;
    }
    if (vnode.type === FOR_MARKER) {
        mountFor(vnode, parentNative);
        return parentNative;
    }

    /* Composite widgets whose real LVGL parent is an internal sub-object
     * unreachable via appendChild — see AGENTS.md "Composite widgets". */
    if (vnode.type === "Tab") {
        const title  = vnode.props.title ?? "";
        const native = lv.addTab(parentNative, String(title));
        vnode.native = native;
        for (const [k, v] of Object.entries(vnode.props)) {
            if (k === "title") continue;
            applyProp(native, k, v);
        }
        for (const child of vnode.children) mountChild(child, native);
        return native;
    }
    if (vnode.type === "ListButton") {
        const text   = vnode.props.text ?? "";
        const native = lv.listAddButton(parentNative, String(text));
        vnode.native = native;
        for (const [k, v] of Object.entries(vnode.props)) {
            if (k === "text") continue;
            applyProp(native, k, v);
        }
        for (const child of vnode.children) mountChild(child, native);
        return native;
    }
    if (vnode.type === "MenuPage") {
        const title  = vnode.props.title ?? "";
        const native = lv.menuAddPage(parentNative, String(title));
        vnode.native = native;
        for (const [k, v] of Object.entries(vnode.props)) {
            if (k === "title") continue;
            applyProp(native, k, v);
        }
        for (const child of vnode.children) mountChild(child, native);
        return native;
    }

    const native = (vnode.type === "Calendar")
        ? lv.createNode(vnode.type, { arrowHeader: vnode.props.arrowHeader !== false })
        : (vnode.type === "Spinner" && vnode.props.arcAngle !== undefined)
        ? lv.createNode(vnode.type, { arcAngle: vnode.props.arcAngle })
        : lv.createNode(vnode.type);
    vnode.native = native;

    if (parentNative !== undefined) lv.appendChild(parentNative, native);

    for (const [k, v] of Object.entries(vnode.props)) applyProp(native, k, v);
    for (const child of vnode.children) mountChild(child, native);

    return native;
}

function mountChild(child, parentNative) {
    if (child instanceof VNode) {
        mountVNode(child, parentNative);
    } else if (typeof child === "function") {
        createEffect(() => lv.setProperty(parentNative, "text", String(child())));
    } else if (child !== null && child !== undefined && child !== false) {
        lv.setProperty(parentNative, "text", String(child));
    }
}

/* ── Dynamic components: Show / For ─────────────────────────────────────── */

const SHOW_MARKER = Symbol("Show");
const FOR_MARKER  = Symbol("For");

export function Show(props) {
    return new VNode(SHOW_MARKER, props, props.children || []);
}

export function For(props) {
    return new VNode(FOR_MARKER, props, props.children || []);
}

function mountShow(vnode, parentNative) {
    createEffect(() => {
        const w = vnode.props.when;
        const cond = typeof w === "function" ? w() : w;
        const toMount = cond
            ? vnode.children
            : (vnode.props.fallback ? [vnode.props.fallback] : []);
        for (const child of toMount) {
            /* A function child is a thunk re-evaluated on every (re-)mount,
             * which is how the user gets a fresh component instance — and
             * therefore per-mount onCleanup lifecycle — instead of replaying
             * the eagerly-evaluated JSX from the initial render. */
            const v = typeof child === "function" ? child() : child;
            if (v instanceof VNode) {
                const native = mountVNode(v, parentNative);
                if (native !== parentNative) {
                    onCleanup(() => lv.dispose(native));
                }
            }
        }
    });
}

function mountFor(vnode, parentNative) {
    const renderFn = vnode.children[0];
    if (typeof renderFn !== "function") {
        console.error("For: expected a render-function child (item, index) => VNode");
        return;
    }
    const keyFn = vnode.props.key || ((_, i) => i);

    /* Each row gets its own orphan createRoot (currentOwner=null) so re-runs
     * of the For effect don't cascade-dispose rows we want to keep. A Map
     * keyed on the user-supplied key tells us which rows survive each diff
     * vs need creation or disposal. */
    let rows = new Map();

    const makeRow = (item, i) => {
        const prevOwner = currentOwner;
        currentOwner = null;
        let native, dispose;
        try {
            dispose = createRoot((d) => {
                const v = renderFn(item, i);
                if (v instanceof VNode) {
                    native = mountVNode(v, parentNative);
                    if (native !== parentNative) {
                        onCleanup(() => lv.dispose(native));
                    }
                }
                return d;
            });
        } finally {
            currentOwner = prevOwner;
        }
        return { native, dispose };
    };

    createEffect(() => {
        const e = vnode.props.each;
        const items = typeof e === "function" ? e() : e;
        if (!Array.isArray(items)) return;

        const next = new Map();
        for (let i = 0; i < items.length; i++) {
            const k = keyFn(items[i], i);
            if (rows.has(k)) {
                next.set(k, rows.get(k));
                rows.delete(k);
            } else {
                next.set(k, makeRow(items[i], i));
            }
        }
        for (const r of rows.values()) r.dispose();
        /* Re-append in items order so LVGL native order matches the array,
         * even when items were reordered (lv.appendChild moves an existing
         * child to the end). O(n) per diff, simple and correct. */
        for (let i = 0; i < items.length; i++) {
            const k = keyFn(items[i], i);
            const r = next.get(k);
            if (r && r.native) lv.appendChild(parentNative, r.native);
        }
        rows = next;
    });

    onCleanup(() => {
        for (const r of rows.values()) r.dispose();
        rows.clear();
    });
}

/* ── JSX factory ────────────────────────────────────────────────────────── */

export const Fragment = Symbol("Fragment");

/* React-DOM convention: lowercase JSX tag = host element, Capitalised = component. */
const HOST_TAGS = {
    view:         "View",
    text:         "Text",
    button:       "Button",
    image:        "Image",
    input:        "Input",
    switch:       "Switch",
    progress:     "Progress",
    slider:       "Slider",
    arc:          "Arc",
    spinner:      "Spinner",
    checkbox:     "Checkbox",
    dropdown:     "Dropdown",
    roller:       "Roller",
    tabview:      "Tabview",
    tab:          "Tab",
    list:         "List",
    listButton:   "ListButton",
    spinbox:      "Spinbox",
    led:          "LED",
    chart:        "Chart",
    buttonMatrix: "ButtonMatrix",
    calendar:     "Calendar",
    scale:        "Scale",
    span:         "Span",
    line:         "Line",
    table:        "Table",
    menu:         "Menu",
    menuPage:     "MenuPage",
    keyboard:     "Keyboard",
    animimg:      "AnimImg",
    imagebutton:  "ImageButton",
};

export function h(type, props, ...children) {
    const flat = children.flat(Infinity);

    if (typeof type === "function") {
        return type({ ...(props || {}), children: flat });
    }

    if (typeof type === "string") {
        type = HOST_TAGS[type] || type;
    }

    return new VNode(type, props, flat);
}

/* ── Native helpers re-exported for apps ────────────────────────────────── */

export function loadFont(path, size) {
    return lv.loadFont(path, size);
}

export function loadFontSizes(path, sizes) {
    const result = {};
    for (const sz of sizes) {
        const h = lv.loadFont(path, sz);
        if (h) result[sz] = h;
    }
    return result;
}

export function loadImage(path) {
    return lv.loadImage(path);
}

export function moduleDir(importMetaUrl) {
    const path = importMetaUrl.startsWith("file://")
        ? importMetaUrl.slice("file://".length)
        : importMetaUrl;
    const cut = path.lastIndexOf("/");
    return cut > 0 ? path.slice(0, cut) : ".";
}

export function loadImages(paths) {
    return lv.loadImages(paths);
}

export function findCjkFont() {
    return lv.findCjkFontPath();
}

export function setDefaultFont(handle) {
    if (!handle) {
        const p = lv.findCjkFontPath();
        if (p) handle = loadFontSizes(p, [14, 16, 20, 24]);
    }
    if (handle) lv.setDefaultFont(handle);
}

const GET_PROPERTY_KEYS = new Set([
    "checked", "text", "cursorPos", "target", "scrollable", "visible",
    "width", "height", "x", "y", "value", "backgroundColor", "textColor",
    "borderColor", "borderWidth", "outlineColor", "outlineWidth", "radius", "padding", "shadowWidth",
    "shadowOpa", "bgOpa", "imageOpa", "textOpa", "borderOpa", "lineColor", "lineWidth",
    "arcColor", "arcWidth", "fontLineHeight",
]);
const STYLE_PARTS = new Set([
    "main", "scrollbar", "indicator", "knob", "selected", "items", "cursor", "placeholder",
]);
const STYLE_STATES = new Set([
    "default", "hover", "focus", "pressed", "checked", "disabled",
    "focusKey", "edited", "scrolled",
]);

export function getProperty(node, key, selector) {
    if (!GET_PROPERTY_KEYS.has(key)) {
        throw new TypeError(`getProperty: unknown key '${key}'`);
    }
    if (selector !== undefined && (selector === null || typeof selector !== "object")) {
        throw new TypeError("getProperty: selector must be an object");
    }
    const part = selector?.part ?? "main";
    const state = selector?.state ?? "default";
    if (!STYLE_PARTS.has(part)) {
        throw new TypeError(`getProperty: unknown part '${part}'`);
    }
    if (!STYLE_STATES.has(state)) {
        throw new TypeError(`getProperty: unknown state '${state}'`);
    }
    return lv.getProperty(node, key, { part, state });
}

export function updateLayout(node) {
    return lv.updateLayout(node);
}

export function chartAddSeries(chart, color) {
    return lv.chartAddSeries(chart, color);
}

export function chartSetSeriesColor(chart, series, color) {
    return lv.chartSetSeriesColor(chart, series, color);
}

export function chartSetData(chart, series, data) {
    return lv.chartSetData(chart, series, data);
}

export function showMsgbox(opts, onClose) {
    return lv.showMsgbox(opts, onClose ?? (() => {}));
}

export function createAnimation(node, opts) {
    return lv.createAnimation(node, opts);
}

export function setMenuPage(menu, page) {
    return lv.menuSetPage(menu, page);
}

export const clipboard = {
    read()       { return lv.clipboardRead(); },
    write(text)  { return lv.clipboardWrite(text); },
};

export function focus(node) {
    return lv.focus(node);
}

export function sendKey(key, ctrl = false) {
    return lv.sendKey(key, ctrl);
}

export function setTheme(scheme) {
    const result = lv.setTheme(scheme);
    setThemeVersion((version) => version + 1);
    return result;
}

export function setThemeToken(name, value) {
    const result = lv.setThemeToken(name, value);
    setThemeVersion((version) => version + 1);
    return result;
}

/* ── Mount ──────────────────────────────────────────────────────────────── */

/**
 * render(fn, containerNative?) → dispose()
 *
 * Mounts the VNode tree returned by fn into an LVGL parent (the active screen
 * by default). The whole tree runs inside one root Owner; the returned
 * function disposes that owner — every effect created during mount stops
 * tracking and every onCleanup callback fires.
 */
export function render(fn, containerNative) {
    const root = containerNative !== undefined ? containerNative : lv.getScreen();

    let dispose;
    createRoot((d) => {
        dispose = d;
        const tree  = fn();
        const nodes = Array.isArray(tree) ? tree : [tree];
        for (const vnode of nodes) {
            if (vnode instanceof VNode) mountVNode(vnode, root);
        }
    });

    return dispose;
}
