# stonegui

A modern, declarative embedded GUI framework: **QuickJS + LVGL**, rendering
directly to native LVGL widgets — no XML, no HTML, no serialized UI tree.

You write React-Native-style components in JavaScript; the renderer maps them
straight to LVGL API calls.

```js
h("Text", { text: () => `Count: ${count()}` })
//                         ↓ on signal change
// lv_label_set_text(...)      // a single property update, not a remount
```

See [`doc/prompts.md`](doc/prompts.md) for the original (and partly stale)
design vision; the rest of this README reflects what currently ships.

## Architecture

```text
Application (examples/*)
    ↓
framework.js          fine-grained reactivity (signals + effects + owners)
    ↓  HostRenderer API (createNode / appendChild / setProperty / addEvent / dispose)
lv_bindings.c         native "lvgl" QuickJS module
    ↓
LVGL                  widgets, layout, styles, events, rendering, animation
    ↓
SDL2 (Linux/X11)
```

* **`js/framework.js`** — framework-agnostic reactive core. The component tree
  is built **once**; reactive prop values (thunks) each own their own effect,
  so a signal change updates a single LVGL property without remount. Owner
  tree drives `dispose()` for `<Show>` / `<For>` / hot teardown.
* **`src/lv_bindings.c`** — minimal `HostRenderer` surface exposed to JS as
  the native `lvgl` module.
* **`src/main.c`** — host: boots LVGL + SDL2, the QuickJS runtime (with the
  ES module loader), wires SIGINT/SIGTERM/SDL_QUIT clean shutdown, loads the
  bundle, runs the event loop.

## Build & run

Requires CMake (≥ 3.16), a C/C++ toolchain, and SDL2 (`pkg-config sdl2`).
LVGL **9.2.2** and QuickJS **2025-09-13** are fetched from GitHub via CMake
`FetchContent` on first configure (~60–120 s); no `git submodule` needed.

```sh
cmake -S . -B build
cmake --build build
./build/stonegui                       # runs examples/hello/app.js
./build/stonegui examples/jsx/app.js   # or pass a bundle explicitly
```

The build copies `js/` and `examples/` next to the binary so relative imports
resolve at runtime. Override dep sources with
`-DSTONEGUI_LVGL_SOURCE_DIR=/path` or `-DSTONEGUI_QUICKJS_SOURCE_DIR=/path`
for offline work or upstream hacking.

Press `Ctrl+C` or close the window to exit cleanly (`lv_deinit` →
`JS_FreeRuntime` → `SDL_Quit`, exit code `128+signo`).

## Components (host tags)

30 lowercase JSX host tags map to LVGL widgets. Capitalised aliases also work.
See [`js/framework.d.ts`](js/framework.d.ts) and
[`HOST_TAGS`](js/framework.js) for the authoritative list.

| Tag (JSX)      | LVGL widget         | Notes                                  |
| -------------- | ------------------- | -------------------------------------- |
| `view`         | `lv_obj`            | Transparent layout container           |
| `text`         | `lv_label`          | Plain text                             |
| `button`       | `lv_button`         | Themed button, inline child label      |
| `image`        | `lv_image`          | PNG / JPG / BMP via bundled decoders   |
| `input`        | `lv_textarea`       | Single- or multi-line, CJK IME aware   |
| `switch`       | `lv_switch`         | On/off pill                            |
| `progress`     | `lv_bar`            | Determinate progress bar               |
| `slider`       | `lv_slider`         | Draggable bar with thumb               |
| `arc`          | `lv_arc`            | Circular slider / progress             |
| `spinner`      | `lv_spinner`        | Looping arc indicator                  |
| `checkbox`     | `lv_checkbox`       | Square check + inline text             |
| `dropdown`     | `lv_dropdown`       | Pop-up list, `options="a\nb\nc"`       |
| `roller`       | `lv_roller`         | Wheel-style selector                   |
| `tabview`      | `lv_tabview`        | With `<tab title="...">` children      |
| `tab`          | `lv_tab` (internal) | Tabview page                           |
| `list`         | `lv_list`           | With `<listButton text="...">` items   |
| `listButton`   | (internal)          | List row                               |
| `spinbox`      | `lv_spinbox`        | Numeric stepper                        |
| `led`          | `lv_led`            | Coloured indicator                     |
| `chart`        | `lv_chart`          | Line / bar / scatter; series via `ref` |
| `buttonMatrix` | `lv_buttonmatrix`   | Grid of buttons, `"\n"` row separators |
| `calendar`     | `lv_calendar`       | Date picker with month nav             |
| `scale`        | `lv_scale`          | Tick / label scale                     |
| `span`         | `lv_spangroup`      | Rich text (per-span color / size)      |
| `line`         | `lv_line`           | Polyline from `points={[[x,y],...]}`   |
| `table`        | `lv_table`          | Grid: `rows`/`cols` + `cells={[[…]]}`  |
| `menu`         | `lv_menu`           | Multi-page menu container              |
| `menuPage`     | (internal)          | Menu page; first one auto-activates    |
| `animimg`      | `lv_animimg`        | Multi-frame image loop (PNG/JPG/BMP)   |
| `imagebutton`  | `lv_imagebutton`    | Per-state image button (released/pressed/checked) |

`View` is a transparent, borderless, zero-padding layout box (React-Native
semantics) — set `backgroundColor`, `borderRadius`, `padding` etc. explicitly
when you want decoration.

## Reactivity

```js
import {
    createSignal, createEffect, createMemo, createRoot,
    onCleanup, untrack, batch,
} from "./js/framework.js";

const [count, setCount] = createSignal(0);
const doubled = createMemo(() => count() * 2);

createEffect(() => {
    console.log("count =", count(), "doubled =", doubled());
    onCleanup(() => console.log("effect torn down"));
});

batch(() => { setCount(1); setCount(2); });   // one effect run, not two
untrack(() => count());                       // read without subscribing
```

Owners form a tree: every effect runs inside an owner; disposing an owner
runs its `onCleanup` callbacks and recursively disposes its children. The
function returned by `render()` is the root dispose.

## Dynamic UI: `<Show>` / `<For>`

```jsx
import { Show, For } from "./js/framework.js";

<Show when={visible}>{() => <Header />}</Show>
<Show when={() => loading()} fallback={() => <Empty />}>
    {() => <List items={items} />}
</Show>

<For each={items} key={(item) => item.id}>
    {(item, index) => <Row data={item} index={index} />}
</For>
```

* `<For>` is **keyed** — existing items keep their state across re-renders;
  only added/removed items mount/unmount. Children must be a render function
  `(item, index) => VNode`.
* `<Show>` children should be a function `() => VNode` when you want a fresh
  component instance (and per-mount `onCleanup`) on every toggle. Static
  VNode children also work but are not re-evaluated.

## Images (`<image>`, `<animimg>`, `<imagebutton>`)

LVGL's bundled `lodepng`, `tjpgd`, and BMP decoders are enabled — zero system
dependencies for PNG / JPG / BMP. The POSIX filesystem driver is registered
on letter `A:`, so absolute file paths look like:

```jsx
<image src="A:/abs/path/to/picture.png" style={{ width: 128, height: 128 }} />
```

### `loadImage` — reusable handles

For images you reference more than once, in `<animimg>` arrays, or per
`<imagebutton>` state, prefer the handle form:

```js
import { loadImage, loadImages } from "./js/framework.js";

const logo   = loadImage("/abs/path/logo.png");       // 0 if file missing
const frames = loadImages([                            // batch
    "/abs/path/frame1.png",
    "/abs/path/frame2.png",
    "/abs/path/frame3.png",
]);
```

`loadImage` validates the file with `fopen`, auto-prepends the `A:` prefix
if you pass a `/`-absolute path, and returns an opaque integer handle (just
like `loadFont`). Handles live for the lifetime of the runtime — they are
intentionally never freed (LVGL caches the decoded image keyed on the path).

`<image src>` accepts both forms:

```jsx
<image src={logo} style={{ width: 64, height: 64 }} />     {/* handle */}
<image src="A:/abs/path/icon.png" style={{ width: 32 }} /> {/* string */}
```

### `<animimg>` — multi-frame loop

```jsx
<animimg
    src={frames}
    duration={900}             /* one full cycle, ms */
    repeat="infinite"          /* or a finite count */
    start={true}
    style={{ width: 128, height: 128 }}
/>
```

### `<imagebutton>` — per-state PNG button

```jsx
const released = loadImage("/abs/path/btn-normal.png");
const pressed  = loadImage("/abs/path/btn-pressed.png");

<imagebutton
    released={released}
    pressed={pressed}
    checkable={false}          /* set true so clicks toggle to checked state */
    style={{ width: 128, height: 128 }}
    onClick={() => console.log("click")}
/>
```

Six state props accept handles or strings: `released`, `pressed`,
`disabled`, `checkedReleased`, `checkedPressed`, `checkedDisabled`.
Stonegui binds the solid-image variant — the LVGL 9-patch 3-tile API is
not exposed (no `*_mid` / `*_right` props).

Run the bundled smoke tests:

```sh
./build/stonegui examples/image/app.js          # single PNG
./build/stonegui examples/animimg/app.js        # 3-frame loop
./build/stonegui examples/imagebutton/app.js    # released vs pressed
```

Refresh the test assets with their adjacent `make_*.py` scripts (Python
stdlib only — they pull 72×72 PNGs from the
[Twemoji](https://github.com/twitter/twemoji) CDN). Twemoji graphics are
CC-BY 4.0 © Twitter Inc and other contributors.

## Pseudo-state styles

```jsx
<button style={{
    width: 120, height: 40, borderRadius: 8,
    backgroundColor: "#3498db",
    hover:    { backgroundColor: "#5dade2" },
    pressed:  { backgroundColor: "#2980b9" },
    focus:    { borderWidth: 2, borderColor: "#ffffff" },
    disabled: { backgroundColor: "#7f8c8d" },
}}>Click me</button>
```

Pseudo-state objects nest inside `style`; their entries apply with the
matching LVGL state selector (`hover` / `focus` / `pressed` / `disabled`).

## Animations

```js
import { createAnimation } from "./js/framework.js";

createAnimation(node, {
    property:   "width",      // x|y|width|height|opacity|rotation|scale|value
    from:       100,
    to:         250,
    duration:   200,          // ms
    easing:     "ease-out",   // linear|ease-in|ease-out|ease-in-out|overshoot|bounce|step
    delay:      0,            // ms
    repeat:     1,            // int or "infinite"
    onComplete: () => {},
});
```

LVGL's `lv_anim` engine drives the property; `onComplete` fires when the
animation finishes (or after every loop when repeating).

## Theme (Element Plus)

stonegui ships with a Vue Element Plus–inspired theme (primary `#409EFF`,
4 px radius, flat buttons). Switch between light and dark at runtime:

```js
import { setTheme, setThemeToken } from "./js/framework.js";

setTheme("dark");                         // full dark mode
setTheme("light");                        // back to light
setThemeToken("primary", "#e74c3c");      // override one token
```

Available token names: `primary`, `primary_dark`, `on_primary`, `secondary`,
`bg`, `surface`, `on_surface`, `on_variant`, `outline`, `track`,
`danger`, `warning`.

`setTheme` / `setThemeToken` both call `lv_obj_report_style_change(NULL)` —
every widget repaints. On a 200-widget screen this takes ~1-2 ms; it is
intended for theme toggle, not per-frame animation.

## Hot reload

By default the binary watches the bundle's parent directory and re-runs the
bundle on every save (`IN_CLOSE_WRITE | IN_MOVED_TO | IN_MODIFY`). On reload:
all LVGL widgets are torn down, the QuickJS runtime is freed and recreated,
the bundle re-executes from the top. LVGL state (window, theme, input
devices, focus group) persists; JS state (signals, `loadFont` handles,
animations) does not — re-initialise it in the bundle's top level.

Disable with `--no-watch`. CI passes `--no-watch` for the smoke test.

## CJK font auto-discovery

`setDefaultFont()` with no argument auto-discovers the first installed CJK
font from a built-in candidate list and loads it at 18 px:

```js
import { setDefaultFont, findCjkFont, loadFontSizes } from "./js/framework.js";

setDefaultFont();               // zero-config — auto-discovers CJK at 18 px

const path = findCjkFont();     // returns path string or null
const fonts = loadFontSizes(path, [14, 18, 24]);  // {14: h, 18: h, 24: h}
```

Candidate paths probed in order:
1. `/usr/share/fonts/truetype/wqy/wqy-microhei.ttc`
2. `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`
3. `/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc`
4. `/System/Library/Fonts/PingFang.ttc`
5. `C:/Windows/Fonts/msyh.ttc`

## Fonts & internationalization (CJK / Chinese)

The built-in Montserrat font is Latin-only. To render Chinese (or any other
script) load a TTF/TTC at runtime. The recommended pattern is a **global
default font**, overriding per-element only when you need a different
size/face:

```js
import { loadFont, setDefaultFont } from "./js/framework.js";

const body  = loadFont("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 20);
const title = loadFont("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 30);

setDefaultFont(body);                                   // global default
h("Text", { text: "你好，世界" });                       // inherits `body`
h("Text", { style: { font: title }, text: "标题" });    // explicit override
```

* `loadFont(path, size)` reads the file into memory and builds a Tiny-TTF
  font; returns `0` if the file can't be opened. Text is UTF-8.
* CJK fonts automatically fall back to the matching Montserrat for
  `LV_SYMBOL_*` glyphs (checkbox tick, dropdown arrow); without that
  fallback you'd see tofu for the symbols.
* Tiny-TTF fonts are a fixed pixel size per handle — load one handle per
  size you need. (`fontSize: 14|16|20|24` selects the built-in Latin
  Montserrat sizes.)

## Input field props

All props may be reactive (`() => signal()`).

| Prop | Type | Description |
|---|---|---|
| `placeholder` | string | Ghost text when empty |
| `oneLine` | boolean | Single-line mode |
| `maxLength` | number | Max character count (0 = unlimited) |
| `acceptedChars` | string | Allowed chars, e.g. `"0123456789"` |
| `password` | boolean | Mask text with bullets |
| `align` | `"left"` \| `"center"` \| `"right"` | Text alignment |
| `textSelection` | boolean | Enable mouse/touch text selection |
| `cursorPos` | number | Set cursor position (get via `getProperty(ref, "cursorPos")`) |

Clipboard is exposed via:
```js
import { clipboard } from "./js/framework.js";
clipboard.write("text");
const s = clipboard.read();   // returns string or null
```

Ctrl+A/C/V/X and Home/End work automatically in any focused `<input>`.

## Keyboard input

A global focus group is wired to the keyboard device. Interactive widgets
(`Input`, `Button`, `Switch`, `Slider`, `Arc`, `Checkbox`, `Dropdown`,
`Roller`, `Spinbox`, `ButtonMatrix`, `Calendar`) are added automatically, so
you can click an `Input` to focus it and then type, or `Tab` between widgets.

ASCII and multi-byte **UTF-8 (CJK) input** both work: the host installs a
UTF-8-aware keyboard read callback that delivers one whole character per
keypress (LVGL's default SDL driver otherwise splits multi-byte characters
and corrupts them).

To compose Chinese you still need a system **IME** (e.g. fcitx5 or ibus)
running. Each `Input` reports its on-screen rectangle to SDL when focused,
so the IME's candidate window follows the input box.

## TypeScript support

[`js/framework.d.ts`](js/framework.d.ts) ships full type declarations for
the public API + JSX intrinsics + per-widget props + pseudo-state styles.
VS Code and other LSP editors pick it up automatically for `.js` and `.jsx`
files — no `tsconfig.json` required.

## Examples

| Path                  | What it shows                                    |
| --------------------- | ------------------------------------------------ |
| `examples/hello`      | Hyperscript MVP — signals, fonts, input          |
| `examples/jsx`        | 23 widgets via JSX (esbuild transpile)           |
| `examples/test`       | Framework unit tests — 54 assertions             |
| `examples/anim`       | `createAnimation` smoke test                     |
| `examples/image`      | PNG decoder smoke test                           |
| `examples/animimg`    | Multi-frame `<animimg>` loop                     |
| `examples/imagebutton`| Per-state `<imagebutton>` (released/pressed)     |

### `examples/jsx` — JSX (React/Vue style)

Real JSX, transpiled to plain JS with esbuild (QuickJS can't parse JSX
itself).

```sh
cd examples/jsx
npm install        # once — installs esbuild (dev dependency)
npm run build      # app.jsx → app.js  (or: npm run watch)
cd ../..
./build/stonegui examples/jsx/app.js
```

`node_modules/` and `package-lock.json` are gitignored — `app.js` is
committed so the demo runs without installing anything; you only need
`npm install` to rebuild after editing `app.jsx`.

## Status

LVGL 9.x + QuickJS + SDL2 on Linux.

**Implemented:** 30+ widgets (incl. on-screen `keyboard`, `<animimg>` loops
and `<imagebutton>` per-state PNGs via the new `loadImage` handle system),
style props with pseudo-states (hover/focus/pressed/disabled), Vue Element
Plus light+dark theme with runtime `setTheme()` / `setThemeToken()`, reactive
signals + effects + memos + owner tree, `<Show>` / keyed `<For>`, CJK
auto-discovery (`setDefaultFont()` zero-arg), `loadFontSizes()`, runtime
TTF/CJK fonts with IME placement, clipboard API (`clipboard.read/write`),
Ctrl+A/C/V/X/Home/End shortcuts, full `<input>` props (maxLength /
acceptedChars / password / align / textSelection / cursorPos), keyboard focus
group, animation API, PNG/JPG/BMP image decoders, ES module loading,
FetchContent build, clean SIGINT/SIGTERM/SDL_QUIT shutdown, inotify-driven
hot reload, CI smoke test, TypeScript declarations.

**Planned:** Wayland backend, framework adapters (Solid/Preact), atom-interned
`js_setProperty`, DevTools / signal inspector, remaining LVGL widgets
(`canvas`, `tileview`, `win`).
