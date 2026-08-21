# stonegui — Phase III roadmap (theme / fonts / input / defaults / keyboard)

> **Status: A–E all shipped and verified.** Every phase below has landed;
> the tables are kept as a record of what was built and where it lives.
>
> | Phase | State | Evidence |
> |---|---|---|
> | A — Theme | done | `sg_tokens_light/dark`, `setTheme`, `setThemeToken`, [`examples/showcase`](../examples/showcase/app.js); A4 finished last (span + led were the final two unstyled widgets) |
> | B — CJK fonts | done | `findCjkFontPath`, `loadFontSizes`, zero-arg `setDefaultFont()` |
> | C — Input editor | done | 6 textarea props + `text`, clipboard API, Ctrl+A/C/V/X + Home/End in `sdl_event_watch` |
> | D — Widget defaults | done | `arrowHeader`, `arcAngle`, `spinTime`, `tabBarSize`, `brightness` |
> | E — On-screen keyboard | done | `Keyboard` host tag, `target` / `mode` props, `getProperty(kb,"target")` read-back |
>
> Regression coverage lives in [`examples/test/app.js`](../examples/test/app.js)
> (83 assertions, includes the A/C/E phase behaviour, not just mount smoke).
> Anything still open is tracked in [`AGENTS.md`](../AGENTS.md) "Known gaps" —
> this file is history, not a queue.

The author asked for the next wave of foundational GUI work to cover:
**theme**, **widget defaults**, **CJK fonts**, **input field
selection/copy**, and related basics. P0+P1+P2 (foundation / reactive core
/ DX) already shipped — see [`AGENTS.md`](../AGENTS.md) "Known gaps" for
what's still open across all phases.

---

## Investigation snapshot (when this plan was drafted)

| Area | Current state | Gap |
|---|---|---|
| **Theme** | [`src/sg_theme.c`](../src/sg_theme.c) 253 LOC, 10 hardcoded `SG_*` color tokens, light scheme only, 10 widget classes styled (button / textarea / spinbox / switch / bar / slider / arc / spinner / checkbox + screen) | No dark mode, no runtime swap, tokens not customisable; 12+ widgets unstyled (dropdown / roller / list / table / calendar / menu / span / scale / led / chart / line / msgbox) |
| **Widget defaults** | Hardcoded in `js_createNode`: spinner 10 s / 360°, spinbox 5 digits / range [-99999,99999], tabview 44 px tab bar, calendar arrow header always | Mostly already overridable per-instance; calendar header + spinner arc are the only true hardcodes |
| **CJK fonts** | `loadFont(path, size)` one-by-one; every example manually tries 4 paths; local machine has `wqy-microhei.ttc` / `wqy-zenhei.ttc` + 21 zh-tagged fonts | No system font auto-discovery, no default CJK fallback, no multi-size convenience |
| **Input editor** | Only `placeholder` + `oneLine` bound; LVGL textarea has 13 more `set_*` APIs unexposed | No `maxLength` / `acceptedChars` / `password` / `align` / `textSelection` / `cursorPos` / clipboard / Ctrl+A,C,V,X / Home/End |
| **On-screen keyboard** | `lv_keyboard` widget unbound (listed in AGENTS still-unbound list) | Touch-only targets need it |
| **Adjacent infrastructure** | SDL2 `SDL_GetClipboardText/SetClipboardText` available; `sg_keyboard_read` already intercepts SDL keys for UTF-8 (good wedge point for Ctrl-combo handling) | — |

---

## Phase A — Theme system (dark/light + runtime swap + token customisation)

**Goal:** dark mode, runtime scheme swap, single-token override, coverage
of the 12 currently-unstyled widgets.

| # | Task | Files | Scope |
|---|---|---|---|
| A1 | Split color tokens into `sg_theme_tokens_t` struct; ship light + dark instances | [`src/sg_theme.c`](../src/sg_theme.c) [`src/sg_theme.h`](../src/sg_theme.h) | ~80 LOC |
| A2 | `sg_theme_set_scheme(disp, "dark"\|"light")` C API + JS `setTheme(scheme)` | sg_theme + lv_bindings + framework.js | ~50 LOC |
| A3 | `sg_theme_set_token(disp, name, color)` single-token override + JS `setThemeToken(name, color)` | sg_theme + lv_bindings + framework.js | ~60 LOC |
| A4 | Style the 12 unstyled widgets (dark variants too) | sg_theme.c | ~150 LOC |
| A5 | [`examples/showcase/app.js`](../examples/showcase/app.js) — toggle button flips light↔dark, every widget reflows colours | new bundle | ~70 LOC |
| A6 | README + AGENTS — document theme API and the `lv_obj_report_style_change(NULL)` walk-the-tree cost | docs | docs |

**Verification:** smoke bundle exits 143 after SIGTERM; visual toggle works on hello / jsx / theme bundles.

**Total:** ~350 LOC + docs.

---

## Phase B — CJK font ergonomics (auto-discovery, multi-size, zero-config default)

**Goal:** apps that just want a working CJK default font shouldn't have to
hardcode `/usr/share/fonts/.../wqy-microhei.ttc`.

| # | Task | Files | Scope |
|---|---|---|---|
| B1 | C helper `sg_find_cjk_font_path()` — try WQY / Noto-CJK / PingFang / MS YaHei in order, return first existing | lv_bindings.c | ~30 LOC |
| B2 | JS `findCjkFont()` wrapper + `loadFontSizes(path, [14,16,20,24])` returning `{14, 16, 20, 24}` | framework.js | ~25 LOC |
| B3 | `setDefaultFont()` with no arg → call `findCjkFont() + loadFont(path, 18)` automatically | framework.js + lv_bindings.c | ~15 LOC |
| B4 | Simplify hello / jsx demos to drop the 4-path manual probing | examples/* | ~10 LOC each |
| B5 | README + AGENTS — document path probe order, Tiny-TTF leak still by design | docs | docs |

**Verification:** a bundle that calls `setDefaultFont()` with no arguments renders Chinese on the local machine.

**Total:** ~80 LOC + docs. **Zero dependencies on Phase A** — can run in parallel.

---

## Phase C — Input editor experience (selection / clipboard / shortcuts / validation)

**Goal:** `<input>` should feel like a normal text field — selection,
clipboard, common shortcuts, common validation hooks.

| # | Task | Files | Scope |
|---|---|---|---|
| C1 | Bind 6 textarea props: `maxLength` / `acceptedChars` / `password` / `align` / `textSelection` / `cursorPos` (both get and set for cursorPos) | lv_bindings.c `js_setProperty` + `js_getProperty` | ~70 LOC |
| C2 | SDL2 clipboard binding: `lv.clipboardRead()` / `lv.clipboardWrite(text)` + JS `clipboard` namespace | lv_bindings.c + framework.js | ~40 LOC |
| C3 | Intercept Ctrl+A/C/V/X/Home/End in `sg_keyboard_read` using `SDL_GetModState()`; dispatch to focused textarea via `lv_textarea_*` | main.c | ~80 LOC |
| C4 | framework.d.ts — full input prop typings + clipboard module; pass `tsc --strict` | framework.d.ts | ~25 LOC |
| C5 | Smoke test: type → Ctrl+A → Ctrl+C → focus second input → Ctrl+V → assert text round-tripped | examples/test | ~30 LOC |
| C6 | README + AGENTS — clipboard ties to SDL lifetime, Ctrl-combo interception lives at `sg_keyboard_read` not at the LVGL event layer | docs | docs |

**Verification:** programmatic `SDL_PushEvent` simulating Ctrl+C/V (or install `xdotool` if `sudo` is available); confirm round-trip + tsc clean.

**Total:** ~250 LOC + docs. **Largest block, highest user-facing value.**

---

## Phase D — Widget defaults surface (mostly documentation)

**Goal:** make the few remaining hardcoded `createNode` defaults
overridable, and document the ones that are already prop-driven.

| # | Task | Files | Scope |
|---|---|---|---|
| D1 | AGENTS — list every hardcoded default in `js_createNode`, mark which are already prop-overridable | AGENTS.md | docs |
| D2 | `<calendar arrowHeader={false}>` — let apps opt out of the always-created arrow header | lv_bindings.c | ~20 LOC |
| D3 | `<spinner arcAngle={N}>` — let apps change `SG_SPINNER_ARC_ANGLE` per-instance | lv_bindings.c | ~10 LOC |

**Verification:** smoke covers both opt-outs.

**Total:** ~30 LOC + docs.

---

## Phase E — On-screen keyboard (touch-only targets)

**Goal:** bind LVGL's `lv_keyboard` widget so touch-screen builds work
without a physical keyboard.

| # | Task | Files | Scope |
|---|---|---|---|
| E1 | `createNode "Keyboard"` → `lv_keyboard_create` | lv_bindings.c | ~5 LOC |
| E2 | Props: `target` (textarea ref) + `mode` (text-lower / text-upper / number / special / user) | lv_bindings.c | ~30 LOC |
| E3 | HOST_TAGS entry + `KeyboardProps` + JSX intrinsic | framework.js / framework.d.ts | ~10 LOC |
| E4 | Smoke test: on-screen kb attached to input, simulated touch on a key → input shows the character | examples/test | ~25 LOC |
| E5 | README + AGENTS — remove from still-unbound list | docs | docs |

**Total:** ~70 LOC + docs.

---

## Recommended execution order

```
A (Theme) ─┬─→ D (defaults surface)
B (CJK) ───┤
C (Input) ─┴─→ E (On-screen keyboard)
```

- **A → D:** D's "already overridable via props" wording lines up with A's
  token API mental model.
- **C → E:** E's `target` prop binds the keyboard to a textarea — leans on
  the cursor/selection work C exposes.
- **B is independent** — zero dependencies; do whenever.

**Suggested serial order:** B → A → C → D → E (fastest user-visible wins
first, biggest block in the middle).

**Suggested parallel order:** (B and A together) → C → (D and E together).

---

## Verification convention (inherited from P0–P2)

Each phase ships with:

1. One `examples/<phase>/app.js` smoke bundle.
2. Behavioural assertions in `examples/test/app.js` — `check(true, "…mounts
   without crash")` is NOT coverage; assert an observable value.
3. If `lv_conf.h` or a system library is touched, run a fresh-clone test:
   `rm -rf build/ && cmake -S . -B build && cmake --build build`.
4. After any `framework.d.ts` change, `tsc --strict --target ES2020 --lib ES2020,DOM js/framework.d.ts` must exit 0.
5. End-of-phase full regression: hello / jsx / test / anim / image / animimg /
   imagebutton / theme — all must exit 143 cleanly under `timeout
   --preserve-status -s TERM 3 ./build/stonegui --no-watch <bundle>`.
6. One commit per phase, only on the user's explicit request.

There is **no CI workflow** — `.github/` was removed deliberately. The
regression above is run by hand.

## History

This plan is complete. What tripped up the first pass, kept here so the next
one doesn't repeat it:

- `setTheme` / `setThemeToken` shipped as **silent no-ops** — `styles_init`
  early-returned on pointer identity, and the only pointer ever passed is the
  mutable working copy. Nothing under Phase A was actually observed running.
- Example bundles hardcoded an absolute `/home/<user>/…` asset path, so every
  image test broke the moment the checkout moved. Use
  `moduleDir(import.meta.url)`.
- The Phase C/E "tests" were `check(true, …)` — they passed with the keyboard
  bound to `null`. Real coverage needed `focus()` + `sendKey()` bindings.
- Dark mode was only half-built: `setTheme("dark")` swapped sg_theme's own
  styles but left the LVGL default theme in its light palette, so list /
  table / calendar / menu / chart kept `#212121` text on a `#1d1e1f` surface —
  contrast 1.04, i.e. invisible. Found by auditing resolved
  `backgroundColor` / `textColor` pairs, not by reading code.
