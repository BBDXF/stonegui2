# AGENTS.md — stonegui

QuickJS + LVGL 9.x + SDL2 declarative GUI for Linux. No XML, no virtual DOM —
JS reactivity drives `lv_*` calls directly. Read `README.md` first; this file
only covers things an agent would otherwise miss.

## Layout & sources of truth

```
src/main.c          host: LVGL/SDL boot, QuickJS runtime, event loop
src/lv_bindings.c   *the* renderer surface — exposes module "lvgl" to JS
src/sg_theme.c      Material-ish theme layered over LVGL's default theme
js/framework.js     reactive core: createSignal/createEffect, h(), render()
lv_conf.h           LVGL build config (CMake passes its path to LVGL)
examples/hello      hyperscript demo — runs as-is (no build step)
examples/jsx        JSX demo — must be transpiled with esbuild
doc/prompts.md      ORIGINAL DESIGN DOC, partly stale (see "Stale" below)
third_party/lvgl    LVGL 9.2 — vendored, NOT a git submodule
third_party/quickjs QuickJS 2025-09-13 — vendored, NOT a git submodule
conanfile.txt       empty — present but unused; do not rely on it
```

`.gitignore` lists `third_party/` but those trees are in fact committed.
Don't try `git submodule update`.

## Build, run, iterate

```sh
cmake -S . -B build && cmake --build build      # produces ./build/stonegui
./build/stonegui                                # runs examples/hello/app.js
./build/stonegui examples/jsx/app.js            # other bundle
./test_jsx.sh                                   # full pipeline: cmake + jsx build + run
./test_jsx.sh --build                           # build only, don't launch
./test_jsx.sh --run                             # launch existing binary only
```

- Requires `pkg-config sdl2`. Linux/X11 (SDL2) only; Wayland is a *plan*, not implemented.
- CMake POST_BUILD copies `js/` and `examples/` into `build/`. The binary
  resolves imports relative to its CWD, so run from the repo root or `build/`.
- After editing `examples/jsx/app.jsx`, regenerate `app.js` via `npm run build`
  inside `examples/jsx/` (esbuild). `app.js` is committed so the demo runs
  without `npm install`; `node_modules/` and `package-lock.json` are gitignored.
- No tests, no CI, no formatter, no linter, no TypeScript. Don't go looking.

## Adding a widget / prop / event (multi-file change)

Every JS-visible LVGL feature crosses two files. Touch both, in this order:

1. `src/lv_bindings.c`
   - `js_createNode`: add a `strcmp` branch creating the LVGL widget. Add it
     to `g_group` if the keyboard focus group should reach it.
   - `js_setProperty`: add a `strcmp` branch translating the JS prop into
     `lv_obj_set_*` / `lv_<widget>_set_*`. For colours go through
     `parse_color_ex`; for sizes through `parse_size` (handles `"100%"`,
     `"fill"`, raw px).
   - `widget_value_to_js`: if the widget has a "current value" returnable to
     `onChange(value)` / `getProperty`.
   - `js_addEvent`: only if a new event-name mapping is needed (most reuse
     `change`/`click`/`longpress`/`focus`/`blur`).
2. `js/framework.js` `HOST_TAGS`: add the lowercase JSX tag → canonical name
   mapping. (`Capitalized` tags already work via the canonical name.)

Style props are NOT a separate system — they are just keys handled in
`js_setProperty`. There is no CSS parser and no selector engine; do not add
one (explicitly forbidden by the design doc).

## Reactivity contract (don't break this)

The tree is mounted **once**. Any prop value that is a `function` is treated
as a reactive accessor: `bindProp` wraps the call in `createEffect` so a
signal change runs a single `lv.setProperty(node, key, value())`. Widgets
are never destroyed and recreated. Implication when reviewing JS:

- `<Text text={count}/>` — reactive (signal getter). Updates the label only.
- `<Text text={count()}/>` — *not* reactive: the value is read once at mount.
- `removeChild` / `dispose` are exported from C but not used from JS today;
  there is no list-keyed reconciliation. Don't assume one exists.

## Fonts (two distinct systems — keep them straight)

- `fontSize: 14|16|20|24` → built-in **Latin Montserrat**, compiled into
  `lv_conf.h`. CJK shows as tofu.
- `font: handle` where `handle = loadFont(path, sizePx)` → **Tiny-TTF**,
  any size, supports CJK. One handle per pixel size you need. CJK fonts
  automatically fall back to the matching Montserrat for `LV_SYMBOL_*`
  glyphs (checkbox tick, dropdown arrow); without that fallback you'd see
  tofu for the symbols.
- `setDefaultFont(handle)` reapplies `sg_theme` with that font, so every
  widget inherits it unless it overrides `font`. Call it once at startup.
- The font-file buffer passed to Tiny-TTF is **intentionally never freed**
  (the font references it for its lifetime). Don't add a free.

## CJK input plumbing (fragile)

`main.c` replaces LVGL's SDL keyboard read callback with `sg_keyboard_read`
because the default emits one byte at a time, corrupting multi-byte UTF-8.
The `sg_sdl_kb_t` struct mirrors the layout of LVGL's private SDL keyboard
driver (`char buf[KEYBOARD_BUFFER_SIZE]; bool dummy_read;`). **If you bump
LVGL, re-verify this layout** in `third_party/lvgl/src/drivers/sdl/`. IME
candidate-window placement uses `SDL_SetTextInputRect` from
`sg_update_ime_rect` in `lv_bindings.c`.

## Colours, sizes, layout — small but easy-to-miss specifics

- Colours: `#rrggbb`, `#rrggbbaa`, or named (`black white red green blue
  yellow cyan magenta orange purple pink gray/grey silver lime maroon navy
  olive teal transparent`). See `parse_color_ex` for the canonical list.
- Sizes accept numbers (px), `"NN%"`, or `"fill"` (= 100%).
- `View` widgets have their default LVGL theme stripped (`make_clean_container`)
  *and* `OVERFLOW_VISIBLE` + a 16px `ext_draw_size` so slider/arc knobs at
  track ends aren't clipped. Don't reintroduce clipping when changing layout.
- `flexFlow` is `"row" | "column"` only; `gap`, `alignItems`,
  `justifyContent` (CSS naming) all live in `js_setProperty`.

## Stale / aspirational content (don't trust at face value)

- `doc/prompts.md` still calls the project **"LVUI"**, prescribes
  TypeScript/Solid/Preact adapters, a `packages/` monorepo, and hot
  reload — **none of that exists**. It's the original vision, not the code.
- README's component table still lists 7 widgets; the bindings now ship 23.
  See [`HOST_TAGS`](file:///home/andy/learn/stonegui/js/framework.js) and
  [`js_createNode`](file:///home/andy/learn/stonegui/src/lv_bindings.c) for
  the authoritative list (View, Text, Button, Image, Input, Switch, Progress,
  Slider, Arc, Spinner, Checkbox, Dropdown, Roller, Tabview, Tab, List,
  ListButton, Spinbox, LED, Chart, ButtonMatrix, Calendar, Scale, Span).
  All are exercised in [`examples/jsx/app.jsx`](file:///home/andy/learn/stonegui/examples/jsx/app.jsx).

## Composite widgets that bypass createNode/appendChild

`Tab` and `ListButton` have no `js_createNode` branch. LVGL builds them via
composite calls (`lv_tabview_add_tab`, `lv_list_add_button`) whose true
parent is an internal sub-object you cannot reach with `appendChild`.
Both are special-cased in `mountVNode` in `js/framework.js`, which calls
`lv.addTab` / `lv.listAddButton` and mounts children into the returned
handle. **When adding more composite widgets (e.g. Menu pages, Msgbox
buttons), follow the same pattern.**

Msgbox does not have a declarative form at all — use the imperative
`showMsgbox({title, text, buttons}, onClose)` helper exported from
`js/framework.js`. Modal-on-demand doesn't fit the mount-once tree.

Chart series are also imperative: `chartAddSeries(chart, color)` returns
a handle, `chartSetData(chart, series, [...])` replaces points. Typical
usage is from a `ref={node => ...}` callback on the `<chart>` element
(framework.js calls the ref with the native handle during mount).

## ButtonMatrix map lifetime

`lv_buttonmatrix_set_map` keeps the pointer it is given — the strings AND
the pointer array must outlive the widget. The `map` prop allocates a
`btnmatrix_map_t` (strdup'd strings + a NULL-terminated pointer array) and
attaches it to the widget via `lv_obj_set_user_data`. A `LV_EVENT_DELETE`
handler (`btnmatrix_map_delete_cb`) frees it. **LVGL's map terminator is
`""` (empty string), NOT `NULL` — don't "clean up" that sentinel.**

## Known gaps (what the project is missing today)

If asked to extend the project, these are the obvious holes — none of them
is done, none has a stub:

- **Hot reload** — planned in README/design doc, not started. `main.c` has
  no file watcher and re-runs the bundle once.
- **Wayland backend** — README mentions it; only `lv_sdl_window_create` is
  wired in `main.c`.
- **TypeScript / `.d.ts`** — design doc assumes TS; codebase is plain JS.
- **List rendering with stable identity** — `removeChild`/`dispose` are
  exposed but unused; framework.js does no keyed diffing.
- **Tests / CI / formatter / linter** — none.
- **Clean shutdown** — `main()`'s loop is `while(1)`; the `JS_FreeContext`
  / `JS_FreeRuntime` after it is unreachable.
- **`conanfile.txt`** — empty. Either fill it or delete it.
- **README components table** — out of sync with `HOST_TAGS` (lists 7,
  ships 23). Demo in `examples/jsx/app.jsx` covers all of them.
- **Framework adapters (Solid/Preact)** — design goal; not started. The
  current `framework.js` *is* the renderer + the (only) adapter in one file.
- **Still-unbound LVGL widgets** compiled into this build: `animimg`,
  `canvas`, `imagebutton`, `keyboard`, `line`, `menu`, `table`, `tileview`,
  `win`. Most need either image data (animimg/imagebutton), point-list
  drawing (line/canvas), or page-management semantics (menu/win/tileview)
  that the current declarative model doesn't yet express.

## Hard rules from the design doc

- Renderer talks to LVGL via direct API calls. **No XML / HTML / serialized
  UI tree / runtime parser.**
- No CSS parsing, no CSS selectors. Style objects only, handled in
  `js_setProperty`.
- The tree is built once; reactivity drives property-only updates. Do not
  introduce a virtual DOM diff or remount-on-change.
