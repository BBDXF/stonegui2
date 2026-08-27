# AGENTS.md — stonegui

QuickJS + LVGL 9.x + SDL2 declarative GUI for Linux. No XML, no virtual DOM —
JS reactivity drives `lv_*` calls directly. Read `README.md` first; this file
only covers things an agent would otherwise miss.

## Layout & sources of truth

```
src/main.c                host: LVGL/SDL boot, QuickJS runtime, event loop,
                          inotify-driven hot reload, SDL_QUIT/SIGINT shutdown
src/lv_bindings.c         *the* renderer surface — exposes module "lvgl" to JS
src/sg_theme.c            self-contained Element Plus theme (parent = NULL)
js/framework.js           reactive core: signals/effects/memo/owner/batch,
                          h(), render(), <Show>, <For>, createAnimation
js/framework.d.ts         TypeScript types for the JS API + JSX intrinsics
                          (IDE IntelliSense; not used at runtime)
lv_conf.h                 LVGL build config (CMake passes its path to LVGL)
examples/showcase         *the* core demo — all 31 host tags, light/dark
                          themes + runtime tokens, CJK role fonts, image
                          family, on-screen keyboard, chart/menu/msgbox,
                          animation engine. Interactive by default;
                          STONEGUI_SHOWCASE_SMOKE=1 runs the scripted
                          regression (prints SHOWCASE SMOKE PASSED, exit 0).
                          Image assets live in examples/showcase/assets/
examples/jsx              JSX demo — must be transpiled with esbuild
examples/test             framework + theme/layout/input/keyboard (534 assertions)
doc/prompts.md            ORIGINAL DESIGN DOC, ARCHIVED with banner
CMakeLists.txt            pins LVGL v9.2.2 + QuickJS commit 4c722ce
                          (VERSION 2025-09-13) via FetchContent →
                          build/_deps/{lvgl,quickjs}-src/ on first configure
```

LVGL and QuickJS are **not vendored, not a submodule** — they live in
`build/_deps/` after `cmake -S . -B build`. First configure takes ~60–120 s
because of the GitHub clones; subsequent configures reuse the cache. To
hack on upstream or work offline, pass
`-DSTONEGUI_LVGL_SOURCE_DIR=/path/to/lvgl` and/or
`-DSTONEGUI_QUICKJS_SOURCE_DIR=/path/to/quickjs` to point at a local
checkout.

## Build, run, iterate

```sh
cmake -S . -B build && cmake --build build      # produces ./build/stonegui
./build/stonegui                                # runs examples/showcase/app.js
./build/stonegui examples/jsx/app.js            # other bundle
./build/stonegui --no-watch examples/test/app.js  # disable hot reload
./scripts/run_regression.sh                     # canonical full regression
./test_jsx.sh                                   # JSX-focused build/run entry point
```

- Requires `pkg-config sdl2`. Linux/X11 (SDL2) only; Wayland still planned.
- CMake POST_BUILD copies `js/` and `examples/` into `build/`. The binary
  resolves imports relative to its CWD, so run from the repo root or `build/`.
- After editing `examples/jsx/app.jsx`, regenerate `app.js` via `npm run build`
  inside `examples/jsx/` (esbuild). `app.js` is committed so the demo runs
  without `npm install`; `node_modules/` and `package-lock.json` are gitignored.
- **No formatter, no linter, no CI.** `.github/` was deleted deliberately —
  the canonical regression is run by hand:

  ```sh
  ./scripts/run_regression.sh
  ./scripts/run_regression.sh --skip-jsx-build  # repeat run without npm
  ```

  The runner requires exit 0 plus named pass markers from the finite assertion
  and showcase-smoke bundles; only its explicitly classified interactive bundles
  may pass with timeout status 143. It also runs the strict declaration check.

## Adding a widget / prop / event (multi-file change)

Every JS-visible LVGL feature crosses two files. Touch both, in this order:

1. `src/lv_bindings.c`
   - `js_createNode`: add a `strcmp` branch creating the LVGL widget. Add it
     to `g_group` if the keyboard focus group should reach it.
   - `js_setProperty`: add a `strcmp` branch translating the JS prop into
     `lv_obj_set_*` / `lv_<widget>_set_*`. For colours go through
     `parse_color_ex`; for sizes through `parse_size`. Pass `selector`
     (NOT hardcoded `0`) so pseudo-state styles work.
   - `widget_value_to_js`: if the widget has a "current value" returnable to
     `onChange(value)` / `getProperty`.
   - `js_addEvent`: only if a new event-name mapping is needed.
2. `js/framework.js` `HOST_TAGS`: add the lowercase JSX tag → canonical name.
3. `js/framework.d.ts`: add a `*Props` interface + `JSX.IntrinsicElements` entry
   so VS Code IntelliSense catches typos.

Style props are NOT a separate system — they are just keys handled in
`js_setProperty`. **No CSS parser, no selector engine** (forbidden by design).

## Reactivity contract (don't break this)

The tree is mounted **once**. Any prop value that is a `function` is treated
as a reactive accessor: `bindProp` wraps the call in `createEffect` so a
signal change runs a single `lv.setProperty(node, key, value(), state?)`.
Widgets are never destroyed and recreated by the static path.

- `<Text text={count}/>` — reactive (signal getter). Updates the label only.
- `<Text text={count()}/>` — *not* reactive: value read once at mount.
- Owner tree: every effect runs inside an `Owner`. `render()` returns
  `dispose()` that recursively tears the subtree down (runs `onCleanup`
  callbacks, unsubscribes from signals, fires `lv.dispose` for `<For>` /
  `<Show>` widgets).
- Cleanup contract is encoded in [`createEffect`](js/framework.js): the
  marker cleanup on `currentOwner.cleanups` is what flips `disposed=true`
  and unsubscribes from `sources`. **Don't "simplify" it away.**
- Imperative escape hatches: reactivity is one-way (signals → widget).
  To READ a widget's current state use `getProperty(node, "value"|"checked"|"text")`.
  To get the native handle for one-time imperative setup — Chart series,
  imperative `setMenuPage` — pass `ref={n => ...}`.

### `<Show>` / `<For>` gotchas

- `<For each={…} key={…}>{(item, i) => <X/>}</For>` — children **must be a
  render function**. Each row gets its own orphan `createRoot` so re-running
  the For effect doesn't cascade-dispose surviving rows.
- `<Show when={…}>{() => <X/>}</Show>` — for per-mount component lifecycle
  (Tracker-style `onCleanup` that fires on every toggle), the child **must
  be a function**. Static VNode children mount/unmount the widget but the
  component body only runs once at the initial JSX evaluation.

## Theme API (Element Plus, Vue default style)

The active theme is **Element Plus light** by default (primary `#409EFF`,
4 px radius, flat buttons). Runtime switching:

```js
import { setTheme, setThemeToken } from "./js/framework.js";
setTheme("dark");                          // full dark palette
setThemeToken("primary.base", "#e74c3c");  // colour token → colour value
setThemeToken("radius_base", 8);           // integer token → number value
```

`setTheme` / `setThemeToken` both call `lv_obj_report_style_change(NULL)` to
repaint every widget. On a typical screen (~100 widgets) this takes ~1 ms.
**Don't call it per frame** — it's for theme toggle, not animation.

Tokens are typed and split into two kinds, mirroring
`sg_theme_token_kind_t` in `sg_theme.h`:

- **colour** — the six semantic ramps (`primary`/`success`/`warning`/`danger`
  (alias `error`)/`info`, each `.base`, `.light_3`, `.light_5`, `.light_7`,
  `.light_9`, `.dark_2`) plus the neutral roles (`text_*`, `border_*`,
  `fill_*`, `bg_*`, `overlay_mask`, `white`, `black`);
- **integer** — `radius_base`, `radius_small`, `radius_round`, `border_width`,
  `space_xs`…`space_xl`, `control_height`, `slider_track_size`,
  `slider_knob_size`, `arc_width`, `scrollbar_size`, `shadow_small_width`,
  `shadow_overlay_width`, `shadow_opa`, `disabled_opa`, `overlay_mask_opa`,
  `btn_pad_hor`, `btn_pad_ver`, `radius_btn`, `radius_field`.

The legacy flat names (`primary`, `primary_dark`, `on_primary`, `secondary`,
`bg`, `surface`, `on_surface`, `on_variant`, `outline`, `track`, `danger`,
`warning`) remain as aliases sharing storage with their canonical roles;
`radius_btn` / `radius_field` are independent fields defaulting to
`radius_base`. `doc/theme.md` is the authoritative table with exact hexes and
`tools/verify_theme_tokens.mjs --check` verifies it.

An unknown name returns `SG_THEME_TOKEN_UNKNOWN` and a kind mismatch returns
`SG_THEME_TOKEN_WRONG_KIND`; both surface to JS as a `TypeError`. The same
split exists in `js/framework.d.ts` as `ColorThemeTokenName` vs
`IntegerThemeTokenName`, so `setThemeToken` overloads reject the wrong value
kind at compile time. **Adding a token means editing `sg_theme.h`, the
registry in `sg_theme.c`, `doc/theme.md` and `framework.d.ts` together.**

Style props may reference a colour token live with `"$name"` on
`backgroundColor`, `borderColor` and `textColor` only (`COLOR_STYLE_PROPERTIES`
in `framework.js`). The reference re-resolves through the `themeVersion`
signal on every `setTheme`/`setThemeToken`; a non-colour or unknown name
throws a `TypeError` when the style is applied.

The mutable working copy is `g_tokens` in `sg_theme.c`; `sg_tokens_light`
and `sg_tokens_dark` are read-only presets. `sg_theme_set_scheme` copies a
preset into `g_tokens`; `sg_theme_set_token` patches one field directly.

**`styles_init` must never gate on the token POINTER.** `&g_tokens` is the
only address ever passed in, so a `g_active_tokens == t` fast path turns both
setters into silent no-ops (this shipped broken once). It rebuilds every
shared style on every call by design.

**The theme has NO parent.** `install_theme` sets `g_theme.parent = NULL` and
fills all ten `lv_theme_t` fields itself (`apply_cb`, `parent`, `user_data`,
`disp`, `color_primary`, `color_secondary`, `font_small`, `font_normal`,
`font_large`, `flags`). LVGL's default theme is never installed, so anything
`sg_theme_apply_cb` does not paint stays unstyled — that is the contract, and
it is why the widget/part/state matrix in `doc/theme.md` has to be complete.
Do not reintroduce `lv_theme_default_init`.

**Never use `lv_theme_apply()` to restyle after a scheme/token change.** In
pinned LVGL 9.2.2 it removes all styles from the object first
(`build/_deps/lvgl-src/src/themes/lv_theme.c:46-54`), which would erase JS
local styles. `sg_theme_set_scheme` / `sg_theme_set_token` rebuild the shared
styles in place and call `lv_obj_report_style_change(NULL)` instead.

**`st_text` is applied to every object EXCEPT labels**, before the
class-specific styles. Both halves of that condition are load-bearing and each
has regressed once:

- *without* it, containers that write their own `text_color` (list / table /
  calendar / menu / chart) keep it instead of inheriting from the screen,
  because a local style beats inheritance — they held `#212121` in dark mode,
  contrast 1.04 against the dark surface: invisible;
- *with* it applied to labels too, a Button's caption — which is a child
  **label** object — gets its own `text_color` and stops inheriting the
  button's `on_primary`, so captions rendered `on_surface` grey over the blue
  fill (contrast 3.4 light / 1.9 dark).

Labels must inherit; whatever ancestor owns the colour is what sets it.
`examples/test` asserts both directions.

Read resolved styles back with `getProperty(node, key, {part, state})` — that
is how the theme regression in `examples/test` proves a scheme swap, a token
patch or a `partStyles` override actually repainted:

```js
getProperty(slider, "backgroundColor", { part: "knob" });
getProperty(button, "backgroundColor", { state: "disabled" });
```

Parts: `main`, `scrollbar`, `indicator`, `knob`, `selected`, `items`,
`cursor`, `placeholder` (`placeholder` maps to
`LV_PART_TEXTAREA_PLACEHOLDER`). States: the six writable pseudo-states plus
the read-only `focusKey`, `edited`, `scrolled`. Style keys cover
background/text/border/outline/line/arc colour, the matching widths and
opacities, `radius`, `padding` (resolved `pad_top`), `shadowWidth`,
`shadowOpa`, `bgOpa`, `imageOpa` and `fontLineHeight`. An unknown key, part or
state throws a `TypeError` — it must never fall through to the widget `value`
or return black. Both the `framework.js` wrapper and raw `lv.getProperty` in
`lv_bindings.c` enforce that contract, so native callers cannot bypass the
selector validation. The vocabulary is mirrored by `StylePart` /
`ResolvedState` / `GetPropertyResults` in `framework.d.ts`.

Dropdowns are the one special case: LVGL draws the `selected` row and the
popup `scrollbar` on a separate `lv_dropdownlist` object, so `js_getProperty`
redirects those two parts to `lv_dropdown_get_list(obj)`.

## Measuring layout

Geometry getters — `getProperty(node, "width" | "height" | "x" | "y" |
"visible")` — read `obj->coords`, which LVGL only recomputes inside
`lv_timer_handler`. Straight after `render()` every object is still at 0,0, so
a measurement taken then is meaningless (an early version of the layout test
"passed" against all-zero coordinates). Call `updateLayout(node)` first; it
maps to `lv_obj_update_layout`.

`getProperty(node, "scrollable")` reports the flag.

**A scrollable View must not keep `OVERFLOW_VISIBLE`.** `make_clean_container`
sets that flag (plus a 16 px `ext_draw_size`) on every View so slider/arc knobs
at the track ends aren't clipped — but on a scroll container the same flag
widens both the child clip area (`lv_refr.c`) and the hit-test box
(`lv_indev.c`) by `ext_draw_size`, leaving scrolled-away rows painted over, and
clickable through, whatever sits beside the container. The `scrollable` setter
therefore clears the flag when enabling scrolling and restores it (for plain
`lv_obj` Views only) when disabling.

## Pseudo-state styles

## Pseudo-state and part styles

```jsx
<slider style={{
    backgroundColor: "$fill_light",
    hover:    { backgroundColor: "$primary.light_3" },
    pressed:  { backgroundColor: "$primary.dark_2" },
    checked:  { backgroundColor: "$primary.base" },
    focus:    { borderWidth: 2 },
    disabled: { backgroundColor: "$fill_dark" },
    partStyles: {
        indicator: { backgroundColor: "$primary.base" },
        knob:      { backgroundColor: "$white", pressed: { backgroundColor: "$fill_dark" } },
    },
}} />
```

The 6 pseudo-state strings (`default`, `hover`, `focus`, `pressed`, `checked`,
`disabled`) are mirrored across three places:

- [`framework.js`](js/framework.js): `PSEUDO_STATES` Set used by
  `applyStyleObject`
- [`lv_bindings.c`](src/lv_bindings.c) `js_setProperty`: `parse_pseudo_state`
  maps the string to an `lv_style_selector_t` (LV_STATE_HOVERED etc.) that
  every `lv_obj_set_style_*` call passes as the selector
- [`framework.d.ts`](js/framework.d.ts): the `PseudoState` union

**Adding a new pseudo-state requires editing all three.**

`style.partStyles` maps a part name (same vocabulary as the `getProperty`
selector, validated against `STYLE_PARTS`) to a style object with the same
shape, so each part carries base values plus one level of pseudo-state
nesting. Nesting is strictly one deep: a style object inside a state object
throws a `TypeError`, and so does an unknown part name.

`style.partStyles` is **not** `SpanProps.parts`. The latter predates it and
describes styled text runs inside one `<span>`; the two never overlap and
`parts` stays as it is.

## Fonts (two distinct systems — keep them straight)

- `fontSize: 14|16|20|24` → built-in **Latin Montserrat**, compiled into
  `lv_conf.h`. CJK shows as tofu.
- `font: handle` where `handle = loadFont(path, sizePx)` → **Tiny-TTF**,
  any size, supports CJK. One handle per pixel size. CJK fonts automatically
  fall back to the matching Montserrat for `LV_SYMBOL_*` glyphs (checkbox
  tick, dropdown arrow); without that fallback you'd see tofu for symbols.
- `setDefaultFont(handle)` reapplies `sg_theme` with that font, so every
  widget inherits it unless it overrides `font`. Call it once at startup.
- **`setDefaultFont()` with no args** auto-discovers the first installed CJK
  font via `lv.findCjkFontPath()` and loads the four typography role sizes
  14 / 16 / 20 / 24 through `loadFontSizes`. Probe order: wqy-microhei →
  NotoSansCJK (opentype) → NotoSansCJK (truetype) → PingFang → MS YaHei.
  With no CJK font installed it is a no-op — do not document it as a
  guarantee.
- **`setDefaultFont(map)`** also accepts a partial `{14, 16, 20, 24}` →
  handle map, which feeds `sg_theme_set_role_fonts(base, medium, large,
  display)`. LVGL exposes only three theme font slots, so `font_small` and
  `font_normal` both take the base face and `font_large` takes the 20 px one;
  the 16 px and 24 px roles are reached through explicit theme styles.
- **`loadFontSizes(path, [14, 16, 20, 24])`** loads multiple sizes in one
  call and returns `{14: handle, …}`. It only records truthy handles, so a
  size that failed to load is **absent from the map** — the lookup is
  `undefined`, never `0`. Check with `in` or a truthiness test.
- **`findCjkFont()`** re-exports `lv.findCjkFontPath()` — returns the path
  string or null.
- **The font file bytes are shared by exact path** (`g_font_files`) and font
  objects are cached per `(path, size)` (`g_font_handles`), so four sizes read
  the file once. Both tables are **intentionally immortal**: Tiny-TTF keeps
  the source buffer by reference for the font's lifetime. Don't add a free.
  The cache key is the literal path string; relative paths and symlink
  aliases are deliberately not normalised.

## Images (`<image>`, `<animimg>`, `<imagebutton>`)

`lv_conf.h` enables LVGL's bundled decoders — **zero system dependencies**:

- `LV_USE_LODEPNG` — PNG
- `LV_USE_TJPGD` — JPEG
- `LV_USE_BMP` — BMP
- `LV_USE_FS_POSIX` with `LV_FS_POSIX_LETTER='A'` — file paths via `"A:/abs/path/to/file.png"`

GIF is *not* an image decoder in LVGL — it's a separate animation widget
(`lv_gif_create`), currently unbound.

### Two ways to refer to an image

| API | Returns | Where it lives | When to use |
|---|---|---|---|
| `"A:/abs/path.png"` | string | JS heap, freed by JS_FreeCString after `lv_image_set_src` internally `lv_strdup`s | one-off, no `loadImage` indirection |
| `loadImage("/abs/path.png")` | opaque int handle | `sg_image_handle_t` in `lv_malloc`, **never freed** (image catalog) | preferred — reusable, fails fast on missing file, future-proof |

`loadImage` probes the file with `fopen`, normalizes the prefix (auto-prepends
`A:` if the path is `/`-absolute), returns `0` on failure. `loadImages([…])`
is the batch helper. The handle is the heap struct pointer cast to `int64_t`
— same trick as `loadFont`.

`<image src>` accepts **both forms**, dispatched in `js_setProperty`'s `src`
branch via the `resolve_image_src` helper.

### `<animimg src={…} duration repeat start>`

| Prop | Type | LVGL call |
|---|---|---|
| `src` | `(string \| handle)[]` | `lv_animimg_set_src` |
| `duration` | number (ms) | `lv_animimg_set_duration` |
| `repeat` | number \| `"infinite"` | `lv_animimg_set_repeat_count` (string maps to `LV_ANIM_REPEAT_INFINITE = 0xFFFFFFFF`) |
| `start` | boolean | `lv_animimg_start` when truthy |

**Ownership**: `lv_animimg_set_src` stores the array pointer AS-IS
(`animimg->dsc = dsc;`); each frame swap calls `lv_image_set_src` which
copies the inner string. We own the array + always strdup the individual
paths into `sg_animimg_ctx_t`; both freed in `sg_animimg_delete_cb`.
Always strdup (handles and plain strings alike) — predictability beats
per-frame ownership branching, and per-frame paths are tiny.

### `<imagebutton released pressed disabled checkedReleased checkedPressed checkedDisabled checkable>`

Each state prop accepts a string OR handle. The helper
`imagebutton_set_state` strdups into `sg_imagebutton_ctx.srcs[STATE]`,
replace-and-frees on reassignment, frees all 6 slots in
`sg_imagebutton_delete_cb`.

Internally always calls `lv_imagebutton_set_src(btn, STATE, src, NULL, NULL)`
— stonegui binds the simple solid case, NOT the 9-patch 3-tile variant
(would require additional `*_mid` / `*_right` props).

`checkable={true}` adds `LV_OBJ_FLAG_CHECKABLE` so clicks toggle between
released and checked-released states. The 6 imagebutton states map to
`LV_IMAGEBUTTON_STATE_RELEASED`/`_PRESSED`/`_DISABLED` and their
`_CHECKED_*` siblings (see `lv_imagebutton.h`).

## CJK input plumbing (fragile)

`main.c` replaces LVGL's SDL keyboard read callback with `sg_keyboard_read`
because the default emits one byte at a time, corrupting multi-byte UTF-8.
The `sg_sdl_kb_t` struct mirrors the layout of LVGL's private SDL keyboard
driver (`char buf[KEYBOARD_BUFFER_SIZE]; bool dummy_read;`). **If you bump
the LVGL pin in `CMakeLists.txt`, re-verify this layout** in
`build/_deps/lvgl-src/src/drivers/sdl/`. IME candidate-window placement uses
`SDL_SetTextInputRect` from `sg_update_ime_rect` in `lv_bindings.c`.

## Keyboard shortcuts (Ctrl+A/C/V/X, Home, End)

Ctrl-combo shortcuts for `<input>` fields are intercepted in
`sdl_event_watch` in [`src/main.c`](src/main.c) (NOT in `sg_keyboard_read`).
`sdl_event_watch` fires for every SDL event; Ctrl+key combos arrive as
`SDL_KEYDOWN` events (not `SDL_TEXTINPUT`), so `sg_keyboard_read` would never
see them. The focused textarea is retrieved via
`lv_group_get_focused_obj(g_kbd_group)` where `g_kbd_group` is set from
`main()` after `lv_bindings_set_group(group)`.

The watch reads **`SDL_GetModState()`**, not the event's `keysym.mod`. That is
why `lv.sendKey(key, ctrl)` wraps its `SDL_PushEvent` in a
`SDL_SetModState` / restore pair — `SDL_PushEvent` runs event watchers
synchronously, so the shortcut has already been applied when it returns. This
is what makes the Ctrl+A/C/V/X round-trip in `examples/test` a real test
rather than a mount smoke.

`lv.focus(node)` (`lv_group_focus_obj`) is the companion: tests and apps can
move focus without simulating a click.

## Colours, sizes, layout — small but easy-to-miss specifics

- Colours: `#rrggbb`, `#rrggbbaa`, or named (`black white red green blue
  yellow cyan magenta orange purple pink gray/grey silver lime maroon navy
  olive teal transparent`). See `parse_color_ex` for the canonical list.
- Sizes accept numbers (px), `"NN%"`, `"fill"` (= 100%), or `"auto"`
  (= `LV_SIZE_CONTENT`, hug the content). An unparseable string warns on
  stderr and falls back to `"auto"` — it used to silently resolve to 0, which
  looked like the widget had vanished.
- `View` widgets have their default LVGL theme stripped (`make_clean_container`)
  *and* `OVERFLOW_VISIBLE` + a 16px `ext_draw_size` so slider/arc knobs at
  track ends aren't clipped. Keep that for plain Views — but a View with
  `scrollable` must clip; see "Measuring layout" above.
- `flexFlow` is `"row" | "column"` only; `gap`, `alignItems`,
  `justifyContent` (CSS naming) all live in `js_setProperty`.

## Hot reload (inotify, Linux only)

`main.c` watches the bundle's **parent directory** with `IN_CLOSE_WRITE |
IN_MOVED_TO | IN_MODIFY` (not the file directly — most editors do
`write tmpfile + rename` and would orphan an inode watch). Events filtered by
filename. On change: `lv_obj_clean(scr) → js_std_free_handlers →
JS_FreeContext → JS_FreeRuntime → boot new runtime → load_and_run`.

**Order matters and is encoded in [`main.c`](src/main.c)**: `lv_obj_clean`
runs BEFORE `JS_FreeContext` because LV_EVENT_DELETE handlers (registered
in `lv_bindings.c` per widget) call back into the JS context to free
callback values. Reversing the order = use-after-free on first save.

After a reload all JS state is fresh: signals reset, `loadFont` handles are
leaked (intentional — Tiny-TTF references the buffer for its lifetime), the
bundle re-runs from the top. LVGL state survives: window, theme, input
devices, focus group.

Disable with `--no-watch` CLI flag.

## Composite widgets that bypass createNode/appendChild

`Tab`, `ListButton`, and `MenuPage` have no `js_createNode` branch. LVGL
builds them via composite calls (`lv_tabview_add_tab`, `lv_list_add_button`,
`lv_menu_page_create`) whose true parent is an internal sub-object you
cannot reach with `appendChild`.

All three are special-cased in `mountVNode` in `js/framework.js`: it pulls
one construction prop off the VNode (`title` for Tab/MenuPage, `text` for
ListButton), calls the dedicated bridge function (`lv.addTab` /
`lv.listAddButton` / `lv.menuAddPage`), then applies the remaining props
and mounts children into the returned handle.

`lv.menuAddPage` auto-activates the FIRST page added (uses `lv_obj_get_user_data`
on the menu as a "page already set" flag), so a static `<menu>` with pages
renders something without manual `setMenuPage`.

Msgbox does not have a declarative form at all — use the imperative
`showMsgbox({title, text, buttons}, onClose)` helper exported from
`js/framework.js`. Modal-on-demand doesn't fit the mount-once tree.

Chart series are also imperative: `chartAddSeries(chart, color)` returns
a handle, `chartSetData(chart, series, [...])` replaces points. Typical
usage is from a `ref={node => ...}` callback on the `<chart>` element.

## Owned-pointer lifetimes (LVGL keeps the pointer)

Five widgets receive data from JS that LVGL stores **by reference** (no
copy). Each allocates a heap struct, attaches it via `lv_obj_set_user_data`,
and frees on `LV_EVENT_DELETE`. **Do not "simplify" the strdup/delete
pattern** — the widget would dereference freed memory on next paint.

- ButtonMatrix `map` → `btnmatrix_map_t` (`""` terminator, NOT NULL — don't
  "clean up" that sentinel).
- Line `points` → `sg_line_pts_t` (parallel structure).
- Msgbox callbacks → `msgbox_ctx_t` (holds JSValue refs).
- AnimImg `src` array → `sg_animimg_ctx_t` (`const char **paths` + count;
  array stored as-is by `lv_animimg_set_src`, individual paths copied by the
  inner image on each frame swap, but the array itself must survive).
- ImageButton state srcs → `sg_imagebutton_ctx_t.srcs[STATE]` (one strdup'd
  string per `LV_IMAGEBUTTON_STATE_*` slot; replace-and-free on
  reassignment; all 6 freed on delete).

## Animation API

```js
createAnimation(node, {
    property:   "x" | "y" | "width" | "height" | "opacity"
              | "rotation" | "scale" | "value",
    from: 0, to: 100, duration: 200,
    easing: "ease-out", delay: 0, repeat: 1,
    onComplete: () => {},
});
```

`x/y/width/height` cast `lv_obj_set_*` directly to `lv_anim_exec_xcb_t`; the
rest go through `anim_exec_*` wrappers — opacity/rotation/scale need a
selector, `value` dispatches by widget class. `LV_ANIM_REPEAT_INFINITE` is
the string `"infinite"` from JS. Per-anim heap struct (`sg_anim_cb_t`) freed
in `lv_anim_set_deleted_cb` so callbacks survive both completion AND
cancellation.

## Bundle-relative assets

`main.c`'s `load_and_run` calls `js_module_set_import_meta(ctx, val, true,
true)` so the entry bundle gets `import.meta.url` (imported modules get it
from `js_module_loader`; a bundle compiled by hand with `JS_Eval` does not).
Bundles derive their asset directory with `moduleDir(import.meta.url)` —
never hardcode an absolute path, the repo moves.

## Stale / aspirational content (don't trust at face value)

- `doc/prompts.md` is the ORIGINAL design vision (then called *LVUI*), now
  carries an ARCHIVED banner at top. None of its prescriptions (TS runtime,
  `packages/` monorepo, hot reload as a future, etc.) reflect current code.
- README's component table now matches `HOST_TAGS` (31 lowercase host tags).
  See [`HOST_TAGS`](js/framework.js) and [`js_createNode`](src/lv_bindings.c)
  for the canonical list. All 31 are exercised across
  [`examples/showcase/app.js`](examples/showcase/app.js) (all 31 tags + the
  scripted smoke manifest asserts one handle per tag),
  [`examples/jsx/app.jsx`](examples/jsx/app.jsx) (23 widgets via JSX),
  [`examples/test/app.js`](examples/test/app.js) (Line, Table, Menu,
  MenuPage, pseudo-states).

## Known gaps (still open)

- **Wayland backend** — `build/_deps/lvgl-src/src/drivers/wayland/` ships,
  needs system `libwayland-client-dev` and a parallel `lv_wayland_window_create`
  branch in `main.c`.
- **Framework adapters (Solid/Preact)** — `framework.js` IS the renderer +
  the (only) adapter; no `HostRenderer` interface extracted yet.
- **Atom-interned `js_setProperty`** — currently a ~50-branch `strcmp` chain
  per call. Not yet profiled as a bottleneck.
- **DevTools / signal inspector** — none.
- **Formatter / linter / CI** — none. `.github/` was removed deliberately;
  `scripts/run_regression.sh` is the whole regression gate.
- **Still-unbound LVGL widgets** compiled into this build: `canvas`,
  `tileview`, `win`. Each needs an imperative shape that doesn't map
  cleanly to the declarative model (pixel buffer / page-management /
  window decoration).

## `js_createNode` hardcoded defaults

| Widget | Default | Prop override | Notes |
|---|---|---|---|
| `Spinner` | arc angle 270°, spin period 1 s | `arcAngle={N}` at create time; `spinTime={ms}` any time | The arc length MUST stay < 360: lv_spinner animates start and end angles in lockstep, so 360° is a closed ring that cannot be seen rotating. The create-time angle is stashed in `user_data` because LVGL has no getter and `lv_spinner_set_anim_params` takes period + length together |
| `Spinbox` | 5 digits, range ±99999, step 1 | `digits="N.M"`, `min`, `max`, `step` | All overridable via props |
| `Tabview` | 44 px tab bar | `tabBarSize={N}` | Overridable via prop |
| `Calendar` | arrow-header always created | `arrowHeader={false}` opts out | Pass via the `<calendar>` JSX prop |
| `LED` | full brightness (255) | `brightness={N}` | Overridable |
| `Dropdown` | empty options | `options="a\nb\nc"` | Set on mount or reactively |

## Hard rules from the design doc

- Renderer talks to LVGL via direct API calls. **No XML / HTML / serialized
  UI tree / runtime parser.**
- No CSS parsing, no CSS selectors. Style objects only, handled in
  `js_setProperty`.
- The tree is built once; reactivity drives property-only updates. Do not
  introduce a virtual DOM diff or remount-on-change. `<For>` / `<Show>` are
  the *only* sanctioned dynamic mounting points.
