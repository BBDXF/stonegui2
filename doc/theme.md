# stonegui theme specification

**Status: AUTHORITATIVE.** Every later theme task (`sg_theme.h` token struct,
`sg_theme.c` style registry, the widget style branches, `partStyles`, the test
matrix) reads *this* file for exact values. Do not re-derive colours elsewhere;
do not hand-edit the tables below.

Every hex value in this document was **copy-pasted from the output of**
`node tools/verify_theme_tokens.mjs --check` — never typed by hand. That script
re-derives all 30 ramp colours and all dark neutrals from the six Element Plus
base colours using the mixing formula documented in
[§2](#2-the-mixing-formula), so the document and the code cannot silently
diverge:

```sh
node tools/verify_theme_tokens.mjs --check                        # exit 0, 159 key=value lines
node tools/verify_theme_tokens.mjs --expect primary.light_9=#000000  # exit 1, names expected/actual
```

## 0. Sources of truth

Derived directly from Element Plus `dev` (fetched fresh, not from a summary):

| File | What was taken |
| --- | --- |
| `packages/theme-chalk/src/common/var.scss` | the 6 base colours, the `set-color-mix-level` loop, light text/border/fill/bg maps, radii, font sizes, `$common-component-size` |
| `packages/theme-chalk/src/dark/var.scss` | dark `$bg-color` map, the **inverted** dark mix loop, the rgba text/border/fill bases + alpha maps |
| `packages/theme-chalk/src/mixins/function.scss` | `roundColor()` — `math.round` per RGB channel |
| `packages/theme-chalk/src/color/index.scss` | `mix-overlay-color()` — rgba-over-opaque flattening used for every dark neutral |

Element Plus's own comment block in `common/var.scss` publishes the whole
primary ramp (`10% 53a8ff … 90% ecf5ff`). Our computed `primary.light_3/5/7/9`
match it exactly; four more published dark values (`#e5eaf3`, `#cfd3dc`,
`#4c4d4f`, `#303030`) and the pre-existing stonegui `primary_dark` (`#337ecc`)
are asserted as hard anchors inside the verifier. `--check` fails if any anchor
drifts.

**Out of scope by decision:** the several hundred component-specific
`--el-<component>-*` variables are *not* imported. Only the reduced role set
below plus the widget matrix in [§8](#8-widget--part--state-matrix) exists.
There is **no** `type` / `size` / `plain` / `round` variant token or API.

## 1. Base semantic colours

Six bases, verbatim from `common/var.scss` `$colors`. `error` is a documented
alias of `danger` (identical hex upstream), kept so the name resolves.

| Token | Hex |
| --- | --- |
| `primary` | `#409eff` |
| `success` | `#67c23a` |
| `warning` | `#e6a23c` |
| `danger` | `#f56c6c` |
| `error` (alias of `danger`) | `#f56c6c` |
| `info` | `#909399` |

## 2. The mixing formula

Element Plus generates `light-1..9` and `dark-2` with Sass `color.mix` wrapped
in `roundColor`. stonegui consumes only **`light-3`, `light-5`, `light-7`,
`light-9`, `dark-2`** (5 levels × 6 colours = 30 derived values).

### Pseudocode (normative)

```
round_sass(x)          = x < 0 ? -floor(-x + 0.5) : floor(x + 0.5)   # half AWAY FROM ZERO
mix_sass(c1, c2, w%)   = per channel: round_sass( (w/100)*c1 + (1 - w/100)*c2 )
mix_overlay(up, a, lo) = per channel: round_sass( up*a + lo*(1 - a) )

# LIGHT scheme
light_i(base) = mix_sass(WHITE,   base, i * 10)      for i in {3,5,7,9}
dark_2(base)  = mix_sass(BLACK,   base, 20)

# DARK scheme  — NOT the same formula. Both mix targets are inverted.
light_i(base) = mix_sass(#141414, base, i * 10)      for i in {3,5,7,9}
dark_2(base)  = mix_sass(WHITE,   base, 20)
```

Two properties are load-bearing and must be preserved bit-for-bit by any
reimplementation:

1. **Rounding is half-away-from-zero, not truncation and not banker's
   rounding.** `primary.light_5` red channel is `0.5*255 + 0.5*64 = 159.5`,
   which must become `160` (`0xa0`) to match the published `#a0cfff`.
2. **In dark mode `light-i` mixes toward `bg_base` (`#141414`), and `dark-2`
   mixes toward WHITE.** This is the whole point of `dark/var.scss`: "lighter"
   levels get *darker*, and `dark-2` becomes a *brighter* hover colour. Copying
   the light formula into dark mode is the single most likely bug here.

`tools/verify_theme_tokens.mjs` implements exactly the above
(`roundSass` / `mixSass` / `mixOverlay`).

## 3. Derived ramps — LIGHT

| Semantic | base | light-3 | light-5 | light-7 | light-9 | dark-2 |
| --- | --- | --- | --- | --- | --- | --- |
| `primary` | `#409eff` | `#79bbff` | `#a0cfff` | `#c6e2ff` | `#ecf5ff` | `#337ecc` |
| `success` | `#67c23a` | `#95d475` | `#b3e19d` | `#d1edc4` | `#f0f9eb` | `#529b2e` |
| `warning` | `#e6a23c` | `#eebe77` | `#f3d19e` | `#f8e3c5` | `#fdf6ec` | `#b88230` |
| `danger` | `#f56c6c` | `#f89898` | `#fab6b6` | `#fcd3d3` | `#fef0f0` | `#c45656` |
| `error` | `#f56c6c` | `#f89898` | `#fab6b6` | `#fcd3d3` | `#fef0f0` | `#c45656` |
| `info` | `#909399` | `#b1b3b8` | `#c8c9cc` | `#dedfe0` | `#f4f4f5` | `#73767a` |

## 4. Derived ramps — DARK

| Semantic | base | light-3 | light-5 | light-7 | light-9 | dark-2 |
| --- | --- | --- | --- | --- | --- | --- |
| `primary` | `#409eff` | `#3375b9` | `#2a598a` | `#213d5b` | `#18222b` | `#66b1ff` |
| `success` | `#67c23a` | `#4e8e2f` | `#3e6b27` | `#2d481f` | `#1c2518` | `#85ce61` |
| `warning` | `#e6a23c` | `#a77730` | `#7d5b28` | `#533f20` | `#292218` | `#ebb563` |
| `danger` | `#f56c6c` | `#b25252` | `#854040` | `#582e2e` | `#2a1d1d` | `#f78989` |
| `error` | `#f56c6c` | `#b25252` | `#854040` | `#582e2e` | `#2a1d1d` | `#f78989` |
| `info` | `#909399` | `#6b6d71` | `#525457` | `#393a3c` | `#202121` | `#a6a9ad` |

Note the intended consequences: in dark mode a `light-9` fill is a *tint-dark*
plate (e.g. `#18222b` for primary), and `dark-2` (`#66b1ff`) is the **brighter**
pressed/hover accent. Hover/pressed rules therefore swap direction between
schemes — see [§7](#7-interaction-recipe).

## 5. Neutral roles

Light values are literal upstream map entries. Dark values are computed by
`mix_overlay(rgba_base, alpha, #141414)`, matching `dark/var.scss`'s
"mix to hex to avoid overlay issues" pass. Dark rgba bases: text `#f0f5ff`,
border `#f5f8ff`, fill `#fafcff`.

| Role | light | dark |
| --- | --- | --- |
| `text_primary` | `#303133` | `#e5eaf3` |
| `text_regular` | `#606266` | `#cfd3dc` |
| `text_secondary` | `#909399` | `#a3a6ad` |
| `text_placeholder` | `#a8abb2` | `#8d9095` |
| `text_disabled` | `#c0c4cc` | `#6c6e72` |
| `border_base` | `#dcdfe6` | `#4c4d4f` |
| `border_light` | `#e4e7ed` | `#414243` |
| `border_lighter` | `#ebeef5` | `#363637` |
| `border_extra_light` | `#f2f6fc` | `#2b2b2c` |
| `border_dark` | `#d4d7de` | `#58585b` |
| `border_darker` | `#cdd0d6` | `#636466` |
| `fill_base` | `#f0f2f5` | `#303030` |
| `fill_light` | `#f5f7fa` | `#262727` |
| `fill_lighter` | `#fafafa` | `#1d1d1d` |
| `fill_extra_light` | `#fafcff` | `#191919` |
| `fill_dark` | `#ebedf0` | `#39393a` |
| `fill_darker` | `#e6e8eb` | `#424243` |
| `fill_blank` | `#ffffff` | `#141414` |
| `bg_page` | `#f2f3f5` | `#0a0a0a` |
| `bg_base` | `#ffffff` | `#141414` |
| `bg_overlay` | `#ffffff` | `#1d1e1f` |
| `overlay_mask` | `#000000` | `#000000` |
| `white` | `#ffffff` | `#ffffff` |
| `black` | `#000000` | `#000000` |

`overlay_mask` is opaque black paired with `metric.overlay_mask_opa = 128`
(Element's `$popup.modal-opacity: 0.5`), because LVGL carries opacity in a
separate style property rather than in the colour.

`fill_blank` is deliberately *not* overlay-mixed upstream — it is the raw
`bg_base` value in both schemes. Do not "fix" it.

### 5.1 Layering intent

`bg_page` (screen root) < `bg_base` < `bg_overlay` (cards, dropdown lists,
msgbox panels, popovers). In light mode `bg_base` and `bg_overlay` are both
white and the separation is carried by borders; in dark mode the three are
genuinely distinct (`#0a0a0a` / `#141414` / `#1d1e1f`), which is what makes a
dark card readable against the dark screen.

## 6. Locked geometry, metric and typography tokens

Copied **verbatim** from the plan's *Locked token and visual mapping* section.
These are decisions, not derivations — do not re-derive them from Element Plus.

| Token | Value | Note |
| --- | --- | --- |
| `radius_base` | `4` | Element `$border-radius.base` |
| `radius_small` | `2` | Element `$border-radius.small` |
| `radius_round` | `20` | Element `$border-radius.round` |
| `border_width` | `1` | Element `$border-width` |
| `space_xs` | `4` | |
| `space_sm` | `8` | |
| `space_md` | `12` | |
| `space_lg` | `16` | |
| `space_xl` | `20` | |
| `control_height` | `32` | Element `$common-component-size.default` |
| `slider_track_size` | `6` | Element `$slider.height` |
| `slider_knob_size` | `20` | Element `$slider.button-size` |
| `arc_width` | `10` | |
| `scrollbar_size` | `6` | |
| `shadow_small_width` | `6` | Element `$box-shadow.lighter` blur |
| `shadow_overlay_width` | `12` | Element `$box-shadow.light` blur |
| `shadow_opa` | `31` | `round(0.12 * 255)` from `rgba(0,0,0,0.12)` |
| `disabled_opa` | `128` | `LV_OPA_50`; dimming for image-family widgets only |
| `overlay_mask_opa` | `128` | `round(0.5 * 255)` from `$popup.modal-opacity` |
| `btn_pad_hor` | `16` | locked default-button padding |
| `btn_pad_ver` | `9` | locked default-button padding |

`radius_btn` and `radius_field` remain **aliases of `radius_base`** for
backward compatibility, individually patchable at runtime.

Colour-bearing widgets must express "disabled" with the explicit
`text_disabled` / `fill_light` / `border_light` roles, **not** by scaling
opacity. `disabled_opa` exists only for `image` / `animimg` / `imagebutton`,
where there is no colour to swap.

### 6.1 Typography — the 14/16/20/24 mapping

Only four Montserrat faces are compiled into `lv_conf.h`. Element Plus's six
font sizes collapse onto them:

| Role token | px face | Element source size | Applied to |
| --- | --- | --- | --- |
| `font.base` | **14** | `$font-size.base` 14px | default/inherited text, labels, button captions, input text, list/table/menu/tab/calendar items, checkbox/roller/dropdown text |
| `font.medium` | **16** | `$font-size.medium` 16px | card + notification + popover titles, alert title-with-description |
| `font.large` | **20** | `$font-size.extra-large` 20px | section/page titles; **also absorbs Element's 18px `large`** (msgbox/dialog title) because no 18px face exists |
| `font.display` | **24** | *(no Element equivalent)* | stonegui large display text only |

Element's `small` (13px) and `extra-small` (12px) have **no** face and map up to
`font.base` (14). Do not add a 12px face without an explicit decision.

Fonts live in theme state and the font API (`setDefaultFont`, `loadFont`,
`loadFontSizes`) — **never** as integer fields in `sg_theme_tokens_t`.

## 7. Interaction recipe

The default button stays **primary-filled with white text** — a deliberate
divergence from Element Plus, which renders an outlined neutral button unless
you pass `type="primary"`. This preserves the existing stonegui contract and is
locked; there is no variant API to opt out.

| Situation | Light | Dark |
| --- | --- | --- |
| Button fill / default | `primary.base` | `primary.base` |
| Button caption | `white` | `white` |
| Button hover fill | `primary.light_3` `#79bbff` | `primary.light_3` `#3375b9` |
| Button pressed fill | `primary.dark_2` `#337ecc` | `primary.dark_2` `#66b1ff` |
| Button padding / radius | `16` × `9`, `radius_base` | same |
| Disabled text | `text_disabled` | `text_disabled` |
| Disabled fill | `fill_light` | `fill_light` |
| Disabled border | `border_light` | `border_light` |
| Focus ring | `primary.light_5`, `border_width` ×2 | `primary.light_5` |
| Neutral hover surface (rows, list buttons, menu items) | `fill_light` | `fill_light` |
| Selected/active surface | `primary.light_9` | `primary.light_9` |
| Selected/active text | `primary.base` | `primary.base` |

Because `light_3` darkens and `dark_2` brightens in dark mode ([§4](#4-derived-ramps--dark)),
the *same* token names give correct-feeling hover/press in both schemes. Style
code must reference the tokens, never the literal hex.

## 8. Widget / part / state matrix

Row set = the canonical 31 host tags in `js/framework.js` `HOST_TAGS`
(lines 427-459), copied exactly and in source order. Part/state support was read
from pinned LVGL 9.2.2:
`build/_deps/lvgl-src/src/core/lv_obj.h:46-83` (enums) and
`build/_deps/lvgl-src/src/themes/default/lv_theme_default.c` (which parts the
reference theme actually addresses per class).

**Parts** (`lv_obj.h:71-83`): `MAIN` `0x000000`, `SCROLLBAR` `0x010000`,
`INDICATOR` `0x020000`, `KNOB` `0x030000`, `SELECTED` `0x040000`,
`ITEMS` `0x050000`, `CURSOR` `0x060000`, `CUSTOM_FIRST` `0x080000`
(= `LV_PART_TEXTAREA_PLACEHOLDER`), `ANY` `0x0F0000`.

**States** (`lv_obj.h:46-62`): `DEFAULT` `0x0000`, `CHECKED` `0x0001`,
`FOCUSED` `0x0002`, `FOCUS_KEY` `0x0004`, `EDITED` `0x0008`, `HOVERED` `0x0010`,
`PRESSED` `0x0020`, `SCROLLED` `0x0040`, `DISABLED` `0x0080`.

Every non-screen object additionally inherits the generic base pass:
`MAIN|DEFAULT` plus `SCROLLBAR|DEFAULT` and `SCROLLBAR|SCROLLED`
(`lv_theme_default.c:734-736,787-789`). Rows below list what stonegui must style
*deliberately on top of that*.

| # | Host tag | LVGL class | Part | States to style |
| --- | --- | --- | --- | --- |
| 1 | `view` | `lv_obj` | `MAIN` | `DEFAULT` (transparent, zero padding, no border — `make_clean_container` semantics; transparent `bg_color = bg_overlay` is a non-rendering branch witness) |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| 2 | `text` | `lv_label` | `MAIN` | `DEFAULT` (font + **inherited** text colour only; must NOT set a local `text_color`, or button captions stop inheriting) |
| 3 | `button` | `lv_button` | `MAIN` | `DEFAULT`, `HOVERED`, `PRESSED`, `FOCUS_KEY`, `CHECKED`, `DISABLED` |
| 4 | `image` | `lv_image` | `MAIN` | `DEFAULT` (transparent, borderless), `FOCUS_KEY` (outline), `PRESSED`/`DISABLED` (`disabled_opa`) |
| 5 | `input` | `lv_textarea` | `MAIN` | `DEFAULT`, `HOVERED`, `FOCUSED`, `FOCUS_KEY`, `EDITED`, `DISABLED` |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| | | | `CURSOR` | `FOCUSED` (LVGL only styles the cursor when focused) |
| | | | `TEXTAREA_PLACEHOLDER` (`CUSTOM_FIRST`) | `DEFAULT` → `text_placeholder` |
| 6 | `switch` | `lv_switch` | `MAIN` | `DEFAULT` (`border_base` track), `FOCUS_KEY`, `DISABLED` |
| | | | `INDICATOR` | `DEFAULT` (transparent), `CHECKED` (`primary.base`, opaque), `DISABLED` |
| | | | `KNOB` | `DEFAULT` (`white`), `DISABLED` |
| 7 | `progress` | `lv_bar` | `MAIN` | `DEFAULT` (`border_light` runway), `FOCUS_KEY`, `EDITED` |
| | | | `INDICATOR` | `DEFAULT` (`primary.base`) |
| 8 | `slider` | `lv_slider` | `MAIN` | `DEFAULT` (`slider_track_size` 6px, `border_light`), `FOCUS_KEY`, `EDITED`, `DISABLED` |
| | | | `INDICATOR` | `DEFAULT` (`primary.base`), `DISABLED` |
| | | | `KNOB` | `DEFAULT` (`slider_knob_size` 20px, `white` on `primary.base` border), `PRESSED`, `DISABLED` |
| 9 | `arc` | `lv_arc` | `MAIN` | `DEFAULT` (`arc_width` 10px, `border_light`) |
| | | | `INDICATOR` | `DEFAULT` (`primary.base`), `DISABLED` |
| | | | `KNOB` | `DEFAULT`, `PRESSED` |
| 10 | `spinner` | `lv_spinner` (an `lv_arc` subclass) | `MAIN` | `DEFAULT` (track) |
| | | | `INDICATOR` | `DEFAULT` (`primary.base`) |
| | | | `KNOB` | `DEFAULT` — must be made **invisible**; a spinner has no draggable handle |
| 11 | `checkbox` | `lv_checkbox` | `MAIN` | `DEFAULT` (text), `FOCUS_KEY`, `CHECKED` (text → `primary.base`), `DISABLED` |
| | | | `INDICATOR` | `DEFAULT` (`radius_small`, `border_base` box), `HOVERED`, `CHECKED` (`primary.base` fill + `white` tick), `PRESSED`, `DISABLED`, `CHECKED\|DISABLED` |
| 12 | `dropdown` | `lv_dropdown` | `MAIN` | `DEFAULT`, `HOVERED`, `PRESSED`, `FOCUS_KEY`, `EDITED`, `DISABLED` |
| | | | `INDICATOR` | `DEFAULT` (the `LV_SYMBOL_DOWN` arrow → `text_placeholder`) |
| | *(separate object)* | `lv_dropdownlist` | `MAIN` | `DEFAULT` (`bg_overlay` + `shadow_overlay_width`) |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| | | | `SELECTED` | `DEFAULT`, `CHECKED` (`primary.light_9` / `primary.base` text), `PRESSED` |
| 13 | `roller` | `lv_roller` | `MAIN` | `DEFAULT`, `FOCUS_KEY`, `EDITED`, `DISABLED` |
| | | | `SELECTED` | `DEFAULT` (`primary.base` band, `white` text), `CHECKED` |
| 14 | `tabview` | `lv_tabview` | `MAIN` | `DEFAULT` (container background) |
| | header (child idx 0) | anonymous `lv_obj` | `MAIN` | `DEFAULT` (`bg_base` + bottom `border_light`) |
| | content (child idx 1) | anonymous `lv_obj` | `MAIN`, `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| | tab buttons (children of header) | `lv_button` | `MAIN` | `DEFAULT`, `HOVERED`, `PRESSED`, `CHECKED` (active tab → `primary.base` text + bottom indicator), `FOCUS_KEY`, `DISABLED` |
| 15 | `tab` | `lv_obj` (page returned by `lv_tabview_add_tab`) | `MAIN` | `DEFAULT` (transparent page, `space_lg` padding) |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| 16 | `list` | `lv_list` | `MAIN` | `DEFAULT` (`bg_overlay`, `radius_base`, `border_light`) |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| 17 | `listButton` | `lv_button` (row from `lv_list_add_button`) | `MAIN` | `DEFAULT` (transparent, left-aligned), `HOVERED` (`fill_light`), `PRESSED`, `CHECKED` (`primary.light_9`), `FOCUS_KEY`, `DISABLED` |
| 18 | `spinbox` | `lv_spinbox` (an `lv_textarea` subclass) | `MAIN` | `DEFAULT`, `HOVERED`, `FOCUS_KEY`, `EDITED`, `DISABLED` |
| | | | `CURSOR` | `DEFAULT` (digit highlight — unlike `input`, LVGL styles this in `DEFAULT`) |
| 19 | `led` | `lv_led` | `MAIN` | `DEFAULT` (colour + `shadow_small_width`; existing contract pins shadow width `8`) |
| 20 | `chart` | `lv_chart` | `MAIN` | `DEFAULT` (`bg_overlay`, division lines → `border_lighter`) |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| | | | `ITEMS` | `DEFAULT` (series line/bar width) |
| | | | `INDICATOR` | `DEFAULT` (point markers) |
| | | | `CURSOR` | `DEFAULT` (cursor line) |
| 21 | `buttonMatrix` | `lv_buttonmatrix` | `MAIN` | `DEFAULT`, `FOCUS_KEY`, `EDITED`, `DISABLED` |
| | | | `ITEMS` | `DEFAULT`, `HOVERED`, `PRESSED`, `CHECKED`, `FOCUS_KEY`, `EDITED`, `DISABLED` |
| 22 | `calendar` | `lv_calendar` | `MAIN` | `DEFAULT` (`bg_overlay`, `radius_base`) |
| | day grid | `lv_buttonmatrix` (child of calendar — dispatch on the **parent class**, not on `lv_buttonmatrix` alone) | `MAIN` | `DEFAULT` (transparent, no border) |
| | | | `ITEMS` | `DEFAULT`, `HOVERED` (`fill_light`), `PRESSED`, `DISABLED` (out-of-month → `text_disabled`); `CUSTOM_1` today and `CUSTOM_2` highlighted-date control bits are painted imperatively by LVGL from `lv_theme_get_color_primary()` |
| | header arrow | `lv_calendar_header_arrow` (`lv_obj` + inner `lv_button`s) | `MAIN` | `DEFAULT`, `HOVERED`, `PRESSED` |
| | header dropdown | `lv_calendar_header_dropdown` (`lv_dropdown`s) | as row 12 | as row 12 |
| 23 | `scale` | `lv_scale` | `MAIN` | `DEFAULT` (main line → `border_base`, `border_width`) |
| | | | `INDICATOR` | `DEFAULT` (major ticks → `border_width` × `space_sm`; labels → `text_regular`) |
| | | | `ITEMS` | `DEFAULT` (minor ticks → `border_dark`, `border_width` × `space_xs`) |
| 24 | `span` | `lv_spangroup` | `MAIN` | `DEFAULT` (inherited text colour + `font.base`; per-span colour/size stays a JS prop) |
| 25 | `line` | `lv_line` | `MAIN` | `DEFAULT` (line colour `primary.base`, width, rounded caps) |
| 26 | `table` | `lv_table` | `MAIN` | `DEFAULT` (`bg_overlay`), `FOCUS_KEY`, `EDITED` |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| | | | `ITEMS` | `DEFAULT` (cell padding + `border_lighter` grid + `text_regular`), `HOVERED` (`fill_light`), `PRESSED`, `FOCUS_KEY`, `EDITED` |
| 27 | `menu` | `lv_menu` | `MAIN` | `DEFAULT` (`bg_base`) |
| | sidebar / main cont | `lv_menu_sidebar_cont` / `lv_menu_main_cont` | `MAIN` | `DEFAULT` (sidebar gets a right `border_light`) |
| | header | `lv_menu_sidebar_header_cont` / `lv_menu_main_header_cont` | `MAIN` | `DEFAULT` |
| | separator | `lv_menu_separator` | `MAIN` | `DEFAULT` (`border_lighter`) |
| | section | `lv_menu_section` | `MAIN` | `DEFAULT` (`bg_overlay`, `radius_base`) |
| | items | `lv_menu_cont` | `MAIN` | `DEFAULT`, `HOVERED` (`fill_light`), `PRESSED`, `CHECKED` (active → `primary.light_9` + `primary.base`) |
| 28 | `menuPage` | `lv_menu_page` | `MAIN` | `DEFAULT` (transparent page) |
| | | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| 29 | `keyboard` | `lv_keyboard` (an `lv_buttonmatrix` subclass) | `MAIN` | `DEFAULT` (`bg_page`), `FOCUS_KEY`, `EDITED` |
| | | | `ITEMS` | `DEFAULT` (`bg_overlay` key, `radius_base`), `HOVERED`, `PRESSED` (`fill_dark`), `CHECKED` (modifier keys → `primary.base`), `FOCUS_KEY`, `EDITED`, `DISABLED` |
| 30 | `animimg` | `lv_animimg` (an `lv_image` subclass) | `MAIN` | `DEFAULT` (transparent, borderless — must not alter frame pixels; transparent `bg_color = bg_overlay` is a non-rendering exact-class branch witness) |
| 31 | `imagebutton` | `lv_imagebutton` | `MAIN` | `DEFAULT` (transparent, borderless), `FOCUS_KEY` (outline), `PRESSED` / `CHECKED` / `DISABLED` (`disabled_opa` only; LVGL swaps the *source image* per state, styling must not fight it) |

Calendar `CUSTOM_1` / `CUSTOM_2` are names for buttonmatrix control bits, not
`lv_state_t` selectors. Pinned LVGL's draw callback replaces today/highlight
border and fill colours with `lv_theme_get_color_primary()`. `install_theme`
keeps that value synchronized with `primary.base`; rendered probing confirmed a
runtime patch from `#409eff` to `#67c23a` repaints the calendar decoration, so
stonegui deliberately adds no draw-task subsystem and claims no dead `CHECKED`
style.

### 8.1 Imperative msgbox (not a host tag, still in scope)

`showMsgbox()` builds `lv_msgbox`, which is reachable only imperatively.

| Object | LVGL class | Part | States |
| --- | --- | --- | --- |
| backdrop | `lv_msgbox_backdrop` | `MAIN` | `DEFAULT` → `overlay_mask` @ `overlay_mask_opa` |
| panel | `lv_msgbox` | `MAIN` | `DEFAULT` → `bg_overlay`, `radius_base`, `shadow_overlay_width` @ `shadow_opa` |
| header | `lv_msgbox_header` | `MAIN` | `DEFAULT` → bottom `border_lighter`, `font.large` title |
| content | `lv_msgbox_content` | `MAIN` | `DEFAULT` → `font.base`, `text_regular` |
| | | `SCROLLBAR` | `DEFAULT`, `SCROLLED` |
| footer | `lv_msgbox_footer` | `MAIN` | `DEFAULT` → top `border_lighter`, right-aligned |
| footer buttons | `lv_button` | `MAIN` | as row 3 |
| header close button | `lv_button` | `MAIN` | `DEFAULT` (transparent), `HOVERED`, `PRESSED` |

### 8.2 Explicitly NOT styled

`lv_canvas`, `lv_tileview`, `lv_win` are compiled into the build but have no
host tag and no binding. They are out of the theme-coverage promise.

## 9. Legacy alias table

The 12 existing `sg_theme_tokens_t` colour fields plus both radii stay valid as
aliases onto canonical roles. Chosen by comparing the *current* hex in
`src/sg_theme.c:27-59` against the fetched Element Plus maps and picking the
exact match wherever one exists.

| Legacy name | Canonical role | light | dark | Status |
| --- | --- | --- | --- | --- |
| `primary` | `primary.base` | `#409eff` | `#409eff` | exact, unchanged |
| `primary_dark` | `primary.dark_2` | `#337ecc` | `#66b1ff` | light exact (already `#337ECC` in `sg_theme.c`); dark **corrected** |
| `on_primary` | `white` | `#ffffff` | `#ffffff` | exact, unchanged |
| `secondary` | `success.base` | `#67c23a` | `#67c23a` | exact, unchanged |
| `bg` | `bg_page` | `#f2f3f5` | `#0a0a0a` | light exact; dark **corrected** from `#141414` |
| `surface` | `bg_overlay` | `#ffffff` | `#1d1e1f` | exact in **both**, unchanged |
| `on_surface` | `text_primary` | `#303133` | `#e5eaf3` | light exact; dark **corrected** from `#cfd3dc` |
| `on_variant` | `text_secondary` | `#909399` | `#a3a6ad` | exact in **both**, unchanged |
| `outline` | `border_base` | `#dcdfe6` | `#4c4d4f` | light exact; dark **corrected** from `#414243` |
| `track` | `border_light` | `#e4e7ed` | `#414243` | light exact; dark **corrected** from `#2c2c2c` |
| `danger` | `danger.base` | `#f56c6c` | `#f56c6c` | exact, unchanged |
| `warning` | `warning.base` | `#e6a23c` | `#e6a23c` | exact, unchanged |
| `radius_btn` | `radius_base` | `4` | `4` | alias, independently patchable |
| `radius_field` | `radius_base` | `4` | `4` | alias, independently patchable |

### 9.1 Reasoning for the contested choices

- **`surface` → `bg_overlay`, not `bg_base`.** `bg_overlay` matches the current
  value exactly in *both* schemes (`#ffffff` / `#1d1e1f`); `bg_base` would force
  dark `surface` to `#141414` and break the `#1d1e1f` compatibility floor
  pinned in Todo 1. Semantically correct too: `surface` is used for cards,
  dropdown lists and msgbox panels, which is exactly Element's `bg-color-overlay`.
- **`on_variant` → `text_secondary`, not `text_placeholder`.** `text_secondary`
  matches both schemes exactly (`#909399` / `#a3a6ad`); `text_placeholder`
  (`#a8abb2` / `#8d9095`) matches neither. The `input` placeholder part still
  uses `text_placeholder` directly — that is a *part* rule, not this alias.
- **`track` → `border_light`, not a fill role.** Light `#e4e7ed` is a byte-exact
  match for `border_light`; the nearest fill role (`fill_darker` `#e6e8eb`) is
  not exact. Element's own `$slider.runway-bg-color` and `$timeline.node-color`
  are `border-color-light`, so this is also semantically right.
- **`bg` → `bg_page`.** Light `#f2f3f5` is a byte-exact match for
  `bg-color-page` and *not* for `bg-color` (`#ffffff`).

### 9.2 Pinned corrections (four dark values change)

Per the plan, `doc/theme.md` may pin a corrected value; the affected assertions
in `examples/test/app.js` must be updated when the widget branches land.

| Token | Was (dark) | Now (dark) | Why |
| --- | --- | --- | --- |
| `bg` | `#141414` | `#0a0a0a` | restores Element's page < base < overlay layering |
| `on_surface` | `#cfd3dc` | `#e5eaf3` | `#cfd3dc` is Element's `text_regular`; primary body text is `text_primary` |
| `outline` | `#414243` | `#4c4d4f` | `#414243` is `border_light`; the base border role is `border_base` |
| `track` | `#2c2c2c` | `#414243` | `#2c2c2c` matched no Element role at all |

**Unchanged compatibility floor:** `#409eff` (primary), `#ffffff` (on_primary),
`#303133` (light on_surface), `#1d1e1f` (dark surface), LED shadow width `8`,
and `setThemeToken("primary", "#e74c3c")` all still hold.

## 10. Canonical key names (verifier vocabulary)

`tools/verify_theme_tokens.mjs` prints these namespaces, one `key=value` per
line. Light-scheme keys are unprefixed; dark-scheme keys carry a `dark.` prefix.

| Namespace | Example | Count |
| --- | --- | --- |
| light ramp | `primary.light_3` | 36 |
| dark ramp | `dark.primary.light_3` | 36 |
| light neutral | `text_primary`, `bg_page` | 24 |
| dark neutral | `dark.text_primary` | 24 |
| metric | `metric.radius_base` | 21 |
| font | `font.base` | 4 |
| alias | `alias.surface` (value is a canonical key, not a hex) | 14 |
| | **total** | **159** |
