/**
 * sg_theme.c — stonegui default theme
 *
 * A small, opinionated design system that makes the raw LVGL widgets look
 * closer to Element Plus / modern web defaults. Implemented as a custom LVGL
 * parentless theme that owns the complete widget baseline.
 *
 * Layering (lowest → highest priority):
 *   1. sg_theme        — structural and visual baseline
 *   2. user styles     — per-widget style={…} props (local styles, always win)
 *
 * Colours and corner radii are no longer macros — they live in
 * `sg_theme_tokens_t` values (see sg_theme.h). `sg_tokens_light` is the
 * Element Plus light scheme used by `sg_theme_init`; `sg_tokens_dark` is the
 * matching dark scheme, ready for a future runtime switch.
 */

#include "sg_theme.h"
#include "src/themes/lv_theme_private.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ── Design tokens ──────────────────────────────────────────────────────── */

const sg_theme_tokens_t sg_tokens_light = {
    .primary             = LV_COLOR_MAKE(0x40, 0x9E, 0xFF),
    .primary_light_3     = LV_COLOR_MAKE(0x79, 0xBB, 0xFF),
    .primary_light_5     = LV_COLOR_MAKE(0xA0, 0xCF, 0xFF),
    .primary_light_7     = LV_COLOR_MAKE(0xC6, 0xE2, 0xFF),
    .primary_light_9     = LV_COLOR_MAKE(0xEC, 0xF5, 0xFF),
    .primary_dark_2      = LV_COLOR_MAKE(0x33, 0x7E, 0xCC),
    .success             = LV_COLOR_MAKE(0x67, 0xC2, 0x3A),
    .success_light_3     = LV_COLOR_MAKE(0x95, 0xD4, 0x75),
    .success_light_5     = LV_COLOR_MAKE(0xB3, 0xE1, 0x9D),
    .success_light_7     = LV_COLOR_MAKE(0xD1, 0xED, 0xC4),
    .success_light_9     = LV_COLOR_MAKE(0xF0, 0xF9, 0xEB),
    .success_dark_2      = LV_COLOR_MAKE(0x52, 0x9B, 0x2E),
    .warning             = LV_COLOR_MAKE(0xE6, 0xA2, 0x3C),
    .warning_light_3     = LV_COLOR_MAKE(0xEE, 0xBE, 0x77),
    .warning_light_5     = LV_COLOR_MAKE(0xF3, 0xD1, 0x9E),
    .warning_light_7     = LV_COLOR_MAKE(0xF8, 0xE3, 0xC5),
    .warning_light_9     = LV_COLOR_MAKE(0xFD, 0xF6, 0xEC),
    .warning_dark_2      = LV_COLOR_MAKE(0xB8, 0x82, 0x30),
    .danger              = LV_COLOR_MAKE(0xF5, 0x6C, 0x6C),
    .danger_light_3      = LV_COLOR_MAKE(0xF8, 0x98, 0x98),
    .danger_light_5      = LV_COLOR_MAKE(0xFA, 0xB6, 0xB6),
    .danger_light_7      = LV_COLOR_MAKE(0xFC, 0xD3, 0xD3),
    .danger_light_9      = LV_COLOR_MAKE(0xFE, 0xF0, 0xF0),
    .danger_dark_2       = LV_COLOR_MAKE(0xC4, 0x56, 0x56),
    .info                = LV_COLOR_MAKE(0x90, 0x93, 0x99),
    .info_light_3        = LV_COLOR_MAKE(0xB1, 0xB3, 0xB8),
    .info_light_5        = LV_COLOR_MAKE(0xC8, 0xC9, 0xCC),
    .info_light_7        = LV_COLOR_MAKE(0xDE, 0xDF, 0xE0),
    .info_light_9        = LV_COLOR_MAKE(0xF4, 0xF4, 0xF5),
    .info_dark_2         = LV_COLOR_MAKE(0x73, 0x76, 0x7A),
    .text_primary        = LV_COLOR_MAKE(0x30, 0x31, 0x33),
    .text_regular        = LV_COLOR_MAKE(0x60, 0x62, 0x66),
    .text_secondary      = LV_COLOR_MAKE(0x90, 0x93, 0x99),
    .text_placeholder    = LV_COLOR_MAKE(0xA8, 0xAB, 0xB2),
    .text_disabled       = LV_COLOR_MAKE(0xC0, 0xC4, 0xCC),
    .border_base         = LV_COLOR_MAKE(0xDC, 0xDF, 0xE6),
    .border_light        = LV_COLOR_MAKE(0xE4, 0xE7, 0xED),
    .border_lighter      = LV_COLOR_MAKE(0xEB, 0xEE, 0xF5),
    .border_extra_light  = LV_COLOR_MAKE(0xF2, 0xF6, 0xFC),
    .border_dark         = LV_COLOR_MAKE(0xD4, 0xD7, 0xDE),
    .border_darker       = LV_COLOR_MAKE(0xCD, 0xD0, 0xD6),
    .fill_base           = LV_COLOR_MAKE(0xF0, 0xF2, 0xF5),
    .fill_light          = LV_COLOR_MAKE(0xF5, 0xF7, 0xFA),
    .fill_lighter        = LV_COLOR_MAKE(0xFA, 0xFA, 0xFA),
    .fill_extra_light    = LV_COLOR_MAKE(0xFA, 0xFC, 0xFF),
    .fill_dark           = LV_COLOR_MAKE(0xEB, 0xED, 0xF0),
    .fill_darker         = LV_COLOR_MAKE(0xE6, 0xE8, 0xEB),
    .fill_blank          = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .bg_page             = LV_COLOR_MAKE(0xF2, 0xF3, 0xF5),
    .bg_base             = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .bg_overlay          = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .overlay_mask        = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .white               = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .black               = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .radius_base         = 4,
    .radius_small        = 2,
    .radius_round        = 20,
    .border_width        = 1,
    .space_xs            = 4,
    .space_sm            = 8,
    .space_md            = 12,
    .space_lg            = 16,
    .space_xl            = 20,
    .control_height      = 32,
    .slider_track_size   = 6,
    .slider_knob_size    = 20,
    .arc_width           = 10,
    .scrollbar_size      = 6,
    .shadow_small_width  = 6,
    .shadow_overlay_width = 12,
    .shadow_opa          = 31,
    .disabled_opa        = 128,
    .overlay_mask_opa    = 128,
    .btn_pad_hor         = 16,
    .btn_pad_ver         = 9,
    .radius_btn          = 4,
    .radius_field        = 4,
};

const sg_theme_tokens_t sg_tokens_dark = {
    .primary             = LV_COLOR_MAKE(0x40, 0x9E, 0xFF),
    .primary_light_3     = LV_COLOR_MAKE(0x33, 0x75, 0xB9),
    .primary_light_5     = LV_COLOR_MAKE(0x2A, 0x59, 0x8A),
    .primary_light_7     = LV_COLOR_MAKE(0x21, 0x3D, 0x5B),
    .primary_light_9     = LV_COLOR_MAKE(0x18, 0x22, 0x2B),
    .primary_dark_2      = LV_COLOR_MAKE(0x66, 0xB1, 0xFF),
    .success             = LV_COLOR_MAKE(0x67, 0xC2, 0x3A),
    .success_light_3     = LV_COLOR_MAKE(0x4E, 0x8E, 0x2F),
    .success_light_5     = LV_COLOR_MAKE(0x3E, 0x6B, 0x27),
    .success_light_7     = LV_COLOR_MAKE(0x2D, 0x48, 0x1F),
    .success_light_9     = LV_COLOR_MAKE(0x1C, 0x25, 0x18),
    .success_dark_2      = LV_COLOR_MAKE(0x85, 0xCE, 0x61),
    .warning             = LV_COLOR_MAKE(0xE6, 0xA2, 0x3C),
    .warning_light_3     = LV_COLOR_MAKE(0xA7, 0x77, 0x30),
    .warning_light_5     = LV_COLOR_MAKE(0x7D, 0x5B, 0x28),
    .warning_light_7     = LV_COLOR_MAKE(0x53, 0x3F, 0x20),
    .warning_light_9     = LV_COLOR_MAKE(0x29, 0x22, 0x18),
    .warning_dark_2      = LV_COLOR_MAKE(0xEB, 0xB5, 0x63),
    .danger              = LV_COLOR_MAKE(0xF5, 0x6C, 0x6C),
    .danger_light_3      = LV_COLOR_MAKE(0xB2, 0x52, 0x52),
    .danger_light_5      = LV_COLOR_MAKE(0x85, 0x40, 0x40),
    .danger_light_7      = LV_COLOR_MAKE(0x58, 0x2E, 0x2E),
    .danger_light_9      = LV_COLOR_MAKE(0x2A, 0x1D, 0x1D),
    .danger_dark_2       = LV_COLOR_MAKE(0xF7, 0x89, 0x89),
    .info                = LV_COLOR_MAKE(0x90, 0x93, 0x99),
    .info_light_3        = LV_COLOR_MAKE(0x6B, 0x6D, 0x71),
    .info_light_5        = LV_COLOR_MAKE(0x52, 0x54, 0x57),
    .info_light_7        = LV_COLOR_MAKE(0x39, 0x3A, 0x3C),
    .info_light_9        = LV_COLOR_MAKE(0x20, 0x21, 0x21),
    .info_dark_2         = LV_COLOR_MAKE(0xA6, 0xA9, 0xAD),
    .text_primary        = LV_COLOR_MAKE(0xE5, 0xEA, 0xF3),
    .text_regular        = LV_COLOR_MAKE(0xCF, 0xD3, 0xDC),
    .text_secondary      = LV_COLOR_MAKE(0xA3, 0xA6, 0xAD),
    .text_placeholder    = LV_COLOR_MAKE(0x8D, 0x90, 0x95),
    .text_disabled       = LV_COLOR_MAKE(0x6C, 0x6E, 0x72),
    .border_base         = LV_COLOR_MAKE(0x4C, 0x4D, 0x4F),
    .border_light        = LV_COLOR_MAKE(0x41, 0x42, 0x43),
    .border_lighter      = LV_COLOR_MAKE(0x36, 0x36, 0x37),
    .border_extra_light  = LV_COLOR_MAKE(0x2B, 0x2B, 0x2C),
    .border_dark         = LV_COLOR_MAKE(0x58, 0x58, 0x5B),
    .border_darker       = LV_COLOR_MAKE(0x63, 0x64, 0x66),
    .fill_base           = LV_COLOR_MAKE(0x30, 0x30, 0x30),
    .fill_light          = LV_COLOR_MAKE(0x26, 0x27, 0x27),
    .fill_lighter        = LV_COLOR_MAKE(0x1D, 0x1D, 0x1D),
    .fill_extra_light    = LV_COLOR_MAKE(0x19, 0x19, 0x19),
    .fill_dark           = LV_COLOR_MAKE(0x39, 0x39, 0x3A),
    .fill_darker         = LV_COLOR_MAKE(0x42, 0x42, 0x43),
    .fill_blank          = LV_COLOR_MAKE(0x14, 0x14, 0x14),
    .bg_page             = LV_COLOR_MAKE(0x0A, 0x0A, 0x0A),
    .bg_base             = LV_COLOR_MAKE(0x14, 0x14, 0x14),
    .bg_overlay          = LV_COLOR_MAKE(0x1D, 0x1E, 0x1F),
    .overlay_mask        = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .white               = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .black               = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .radius_base         = 4,
    .radius_small        = 2,
    .radius_round        = 20,
    .border_width        = 1,
    .space_xs            = 4,
    .space_sm            = 8,
    .space_md            = 12,
    .space_lg            = 16,
    .space_xl            = 20,
    .control_height      = 32,
    .slider_track_size   = 6,
    .slider_knob_size    = 20,
    .arc_width           = 10,
    .scrollbar_size      = 6,
    .shadow_small_width  = 6,
    .shadow_overlay_width = 12,
    .shadow_opa          = 31,
    .disabled_opa        = 128,
    .overlay_mask_opa    = 128,
    .btn_pad_hor         = 16,
    .btn_pad_ver         = 9,
    .radius_btn          = 4,
    .radius_field        = 4,
};

/* Mutable working copy — patched by set_scheme / set_token at runtime. */
static sg_theme_tokens_t g_tokens;

typedef struct {
    const char *name;
    sg_theme_token_kind_t kind;
    size_t offset;
} sg_theme_token_entry_t;

#define SG_COLOR_TOKEN(name_, field_) \
    { name_, SG_TOKEN_COLOR, offsetof(sg_theme_tokens_t, field_) }
#define SG_INT_TOKEN(name_, field_) \
    { name_, SG_TOKEN_INT, offsetof(sg_theme_tokens_t, field_) }

static const sg_theme_token_entry_t token_registry[] = {
    SG_COLOR_TOKEN("primary.base", primary),
    SG_COLOR_TOKEN("primary.light_3", primary_light_3),
    SG_COLOR_TOKEN("primary.light_5", primary_light_5),
    SG_COLOR_TOKEN("primary.light_7", primary_light_7),
    SG_COLOR_TOKEN("primary.light_9", primary_light_9),
    SG_COLOR_TOKEN("primary.dark_2", primary_dark_2),
    SG_COLOR_TOKEN("success.base", success),
    SG_COLOR_TOKEN("success.light_3", success_light_3),
    SG_COLOR_TOKEN("success.light_5", success_light_5),
    SG_COLOR_TOKEN("success.light_7", success_light_7),
    SG_COLOR_TOKEN("success.light_9", success_light_9),
    SG_COLOR_TOKEN("success.dark_2", success_dark_2),
    SG_COLOR_TOKEN("warning.base", warning),
    SG_COLOR_TOKEN("warning.light_3", warning_light_3),
    SG_COLOR_TOKEN("warning.light_5", warning_light_5),
    SG_COLOR_TOKEN("warning.light_7", warning_light_7),
    SG_COLOR_TOKEN("warning.light_9", warning_light_9),
    SG_COLOR_TOKEN("warning.dark_2", warning_dark_2),
    SG_COLOR_TOKEN("danger.base", danger),
    SG_COLOR_TOKEN("danger.light_3", danger_light_3),
    SG_COLOR_TOKEN("danger.light_5", danger_light_5),
    SG_COLOR_TOKEN("danger.light_7", danger_light_7),
    SG_COLOR_TOKEN("danger.light_9", danger_light_9),
    SG_COLOR_TOKEN("danger.dark_2", danger_dark_2),
    SG_COLOR_TOKEN("error.base", error),
    SG_COLOR_TOKEN("error.light_3", error_light_3),
    SG_COLOR_TOKEN("error.light_5", error_light_5),
    SG_COLOR_TOKEN("error.light_7", error_light_7),
    SG_COLOR_TOKEN("error.light_9", error_light_9),
    SG_COLOR_TOKEN("error.dark_2", error_dark_2),
    SG_COLOR_TOKEN("info.base", info),
    SG_COLOR_TOKEN("info.light_3", info_light_3),
    SG_COLOR_TOKEN("info.light_5", info_light_5),
    SG_COLOR_TOKEN("info.light_7", info_light_7),
    SG_COLOR_TOKEN("info.light_9", info_light_9),
    SG_COLOR_TOKEN("info.dark_2", info_dark_2),
    SG_COLOR_TOKEN("text_primary", text_primary),
    SG_COLOR_TOKEN("text_regular", text_regular),
    SG_COLOR_TOKEN("text_secondary", text_secondary),
    SG_COLOR_TOKEN("text_placeholder", text_placeholder),
    SG_COLOR_TOKEN("text_disabled", text_disabled),
    SG_COLOR_TOKEN("border_base", border_base),
    SG_COLOR_TOKEN("border_light", border_light),
    SG_COLOR_TOKEN("border_lighter", border_lighter),
    SG_COLOR_TOKEN("border_extra_light", border_extra_light),
    SG_COLOR_TOKEN("border_dark", border_dark),
    SG_COLOR_TOKEN("border_darker", border_darker),
    SG_COLOR_TOKEN("fill_base", fill_base),
    SG_COLOR_TOKEN("fill_light", fill_light),
    SG_COLOR_TOKEN("fill_lighter", fill_lighter),
    SG_COLOR_TOKEN("fill_extra_light", fill_extra_light),
    SG_COLOR_TOKEN("fill_dark", fill_dark),
    SG_COLOR_TOKEN("fill_darker", fill_darker),
    SG_COLOR_TOKEN("fill_blank", fill_blank),
    SG_COLOR_TOKEN("bg_page", bg_page),
    SG_COLOR_TOKEN("bg_base", bg_base),
    SG_COLOR_TOKEN("bg_overlay", bg_overlay),
    SG_COLOR_TOKEN("overlay_mask", overlay_mask),
    SG_COLOR_TOKEN("white", white),
    SG_COLOR_TOKEN("black", black),
    SG_INT_TOKEN("radius_base", radius_base),
    SG_INT_TOKEN("radius_small", radius_small),
    SG_INT_TOKEN("radius_round", radius_round),
    SG_INT_TOKEN("border_width", border_width),
    SG_INT_TOKEN("space_xs", space_xs),
    SG_INT_TOKEN("space_sm", space_sm),
    SG_INT_TOKEN("space_md", space_md),
    SG_INT_TOKEN("space_lg", space_lg),
    SG_INT_TOKEN("space_xl", space_xl),
    SG_INT_TOKEN("control_height", control_height),
    SG_INT_TOKEN("slider_track_size", slider_track_size),
    SG_INT_TOKEN("slider_knob_size", slider_knob_size),
    SG_INT_TOKEN("arc_width", arc_width),
    SG_INT_TOKEN("scrollbar_size", scrollbar_size),
    SG_INT_TOKEN("shadow_small_width", shadow_small_width),
    SG_INT_TOKEN("shadow_overlay_width", shadow_overlay_width),
    SG_INT_TOKEN("shadow_opa", shadow_opa),
    SG_INT_TOKEN("disabled_opa", disabled_opa),
    SG_INT_TOKEN("overlay_mask_opa", overlay_mask_opa),
    SG_INT_TOKEN("btn_pad_hor", btn_pad_hor),
    SG_INT_TOKEN("btn_pad_ver", btn_pad_ver),
    SG_COLOR_TOKEN("primary", primary),
    SG_COLOR_TOKEN("primary_dark", primary_dark_2),
    SG_COLOR_TOKEN("on_primary", white),
    SG_COLOR_TOKEN("secondary", success),
    SG_COLOR_TOKEN("bg", bg_page),
    SG_COLOR_TOKEN("surface", bg_overlay),
    SG_COLOR_TOKEN("on_surface", text_primary),
    SG_COLOR_TOKEN("on_variant", text_secondary),
    SG_COLOR_TOKEN("outline", border_base),
    SG_COLOR_TOKEN("track", border_light),
    SG_COLOR_TOKEN("danger", danger),
    SG_COLOR_TOKEN("warning", warning),
    SG_INT_TOKEN("radius_btn", radius_btn),
    SG_INT_TOKEN("radius_field", radius_field),
};

#undef SG_COLOR_TOKEN
#undef SG_INT_TOKEN

/* ── Shared styles (persistent — referenced by widgets for their lifetime) ── */

static bool                       g_styles_inited = false;
static lv_theme_t                 g_theme;
static const lv_font_t           *g_role_fonts[4];
static bool                       g_dark   = false;

#define SG_STYLE_LIST(X) \
    X(st_text) \
    X(st_screen) \
    X(st_btn) \
    X(st_btn_pressed) \
    X(st_field) \
    X(st_field_focused) \
    X(st_field_placeholder) \
    X(st_sw_bg) \
    X(st_sw_indic) \
    X(st_sw_bg_on) \
    X(st_sw_knob) \
    X(st_bar_main) \
    X(st_bar_indic) \
    X(st_knob) \
    X(st_arc_main) \
    X(st_arc_indic) \
    X(st_cb_box) \
    X(st_cb_box_checked) \
    X(st_dropdown_base) \
    X(st_dropdown_sel) \
    X(st_roller_sel) \
    X(st_list) \
    X(st_table) \
    X(st_table_cell) \
    X(st_calendar_main) \
    X(st_calendar_items) \
    X(st_calendar_today) \
    X(st_menu) \
    X(st_scale_main) \
    X(st_scale_indic) \
    X(st_chart) \
    X(st_line) \
    X(st_span) \
    X(st_led) \
    X(st_image) \
    X(st_animimg) \
    X(st_image_focus) \
    X(st_image_dim) \
    X(st_msgbox) \
    X(st_msgbox_backdrop) \
    X(st_msgbox_header) \
    X(st_msgbox_content) \
    X(st_msgbox_scrollbar) \
    X(st_msgbox_scrollbar_scrolled) \
    X(st_msgbox_footer) \
    X(st_msgbox_close_btn) \
    X(st_msgbox_close_btn_hover) \
    X(st_msgbox_close_btn_pressed) \
    X(st_hover_fill) \
    X(st_pressed_fill) \
    X(st_active_surface) \
    X(st_plain_page) \
    X(st_tabview_main) \
    X(st_tab_header) \
    X(st_tab_btn) \
    X(st_tab_btn_checked) \
    X(st_tab_page) \
    X(st_list_btn) \
    X(st_chart_items) \
    X(st_chart_indic) \
    X(st_chart_cursor) \
    X(st_btnm_main) \
    X(st_btnm_item) \
    X(st_btnm_item_checked) \
    X(st_cal_grid_main) \
    X(st_cal_item_disabled) \
    X(st_cal_header) \
    X(st_cal_header_btn) \
    X(st_scale_items) \
    X(st_table_cell_hover) \
    X(st_menu_sidebar) \
    X(st_menu_main_cont) \
    X(st_menu_header) \
    X(st_menu_separator) \
    X(st_menu_section) \
    X(st_menu_item) \
    X(st_kb_main) \
    X(st_kb_item) \
    X(st_kb_item_checked) \
    /* ── Todo 8: foundational controls (doc/theme.md §8 rows 1-3,5-13,18-19) ── */ \
    X(st_view) \
    X(st_scrollbar) \
    X(st_scrollbar_scrolled) \
    X(st_focus_ring) \
    X(st_edited_ring) \
    X(st_disabled_surface) \
    X(st_disabled_text) \
    X(st_disabled_accent) \
    X(st_btn_hover) \
    X(st_btn_checked) \
    X(st_btn_disabled) \
    X(st_field_hover) \
    X(st_field_cursor) \
    X(st_spinbox_cursor) \
    X(st_sw_knob_disabled) \
    X(st_slider_main) \
    X(st_knob_pressed) \
    X(st_knob_disabled) \
    X(st_spinner_knob) \
    X(st_cb_box_hover) \
    X(st_cb_box_pressed) \
    X(st_cb_box_checked_disabled) \
    X(st_cb_checked_text) \
    X(st_dropdown_hover) \
    X(st_dropdown_pressed) \
    X(st_dropdown_indic) \
    X(st_dropdown_list) \
    X(st_dropdown_sel_checked) \
    X(st_dropdown_sel_pressed) \
    X(st_roller_sel_checked)

#define X(name) static lv_style_t name;
SG_STYLE_LIST(X)
#undef X

static void styles_init_media(const sg_theme_tokens_t *t);
static void styles_init_controls(const sg_theme_tokens_t *t);
static void styles_init_composites(const sg_theme_tokens_t *t);

/* (Re)initialise every shared style from the given token set. The first call
 * builds the styles; every subsequent call resets each style in place
 * (lv_style_reset → lv_style_init) so widgets that already reference them pick
 * up the new colours on the next `lv_obj_report_style_change`.
 *
 * There is NO "same token set → skip" fast path: the only token set that is
 * ever passed in is the mutable working copy `g_tokens`, whose *address* never
 * changes while its *contents* do. A pointer-identity guard here would make
 * `sg_theme_set_scheme` / `sg_theme_set_token` silent no-ops. */
static void styles_init(const sg_theme_tokens_t *t) {
    if (g_styles_inited) {
#define X(name) lv_style_reset(&name);
        SG_STYLE_LIST(X)
#undef X
    }

    g_styles_inited = true;

    /* Every widget gets on_surface text. The parent (default) theme paints
     * its own `color_text` directly onto list/table/calendar/menu/chart, and a
     * local style beats inheritance from the screen — so without this those
     * widgets keep a light-scheme grey and turn invisible in dark mode.
     * Applied FIRST in apply_cb, so class styles (button → on_primary) win. */
    lv_style_init(&st_text);
    lv_style_set_text_color(&st_text, t->text_primary);

    /* Screen: scaffold background + app-wide default text colour.
     * text_color is inheritable, so every label inherits on_surface unless
     * an ancestor (e.g. a Button) or the widget itself overrides it. */
    lv_style_init(&st_screen);
    lv_style_set_bg_color(&st_screen, t->bg_page);
    lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);
    lv_style_set_text_color(&st_screen, t->text_primary);
    lv_style_set_text_font(&st_screen,
                           g_role_fonts[0] ? g_role_fonts[0] : LV_FONT_DEFAULT);

    /* Button: filled primary, rounded, flat (Element Plus = no shadow). */
    lv_style_init(&st_btn);
    lv_style_set_bg_color(&st_btn, t->primary);
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_text_color(&st_btn, t->white);   /* inherited by label   */
    lv_style_set_border_width(&st_btn, 0);
    lv_style_set_radius(&st_btn, t->radius_btn);
    lv_style_set_pad_hor(&st_btn, t->btn_pad_hor);
    lv_style_set_pad_ver(&st_btn, t->btn_pad_ver);

    /* Pressed: darken; cancel the default theme's grow. */
    lv_style_init(&st_btn_pressed);
    lv_style_set_bg_color(&st_btn_pressed, t->primary_dark_2);
    lv_style_set_transform_width(&st_btn_pressed, 0);
    lv_style_set_transform_height(&st_btn_pressed, 0);

    /* Text field (Input): outlined, rounded, comfortable padding. */
    lv_style_init(&st_field);
    lv_style_set_bg_color(&st_field, t->bg_overlay);
    lv_style_set_bg_opa(&st_field, LV_OPA_COVER);
    lv_style_set_text_color(&st_field, t->text_primary);
    lv_style_set_radius(&st_field, t->radius_field);
    lv_style_set_border_color(&st_field, t->border_base);
    lv_style_set_border_width(&st_field, t->border_width);
    lv_style_set_pad_all(&st_field, t->space_md);

    /* Focused field: primary 2px outline. */
    lv_style_init(&st_field_focused);
    lv_style_set_border_color(&st_field_focused, t->primary);
    lv_style_set_border_width(&st_field_focused, t->border_width * 2);

    /* Placeholder text colour. */
    lv_style_init(&st_field_placeholder);
    lv_style_set_text_color(&st_field_placeholder, t->text_placeholder);

    /* Switch: pill track (grey off / primary on) with a white circular knob. */
    lv_style_init(&st_sw_bg);
    lv_style_set_bg_color(&st_sw_bg, t->border_light);
    lv_style_set_bg_opa(&st_sw_bg, LV_OPA_COVER);
    lv_style_set_radius(&st_sw_bg, t->radius_round);

    lv_style_init(&st_sw_indic);
    lv_style_set_bg_color(&st_sw_indic, t->primary);
    lv_style_set_bg_opa(&st_sw_indic, LV_OPA_TRANSP);

    lv_style_init(&st_sw_bg_on);
    lv_style_set_bg_color(&st_sw_bg_on, t->primary);
    lv_style_set_bg_opa(&st_sw_bg_on, LV_OPA_COVER);

    lv_style_init(&st_sw_knob);
    lv_style_set_bg_color(&st_sw_knob, t->white);
    lv_style_set_radius(&st_sw_knob, t->radius_round);

    /* Progress (bar): thin pill, light track, primary indicator. */
    lv_style_init(&st_bar_main);
    lv_style_set_bg_color(&st_bar_main, t->border_light);
    lv_style_set_bg_opa(&st_bar_main, LV_OPA_COVER);
    lv_style_set_radius(&st_bar_main, t->radius_round);
    lv_style_set_border_width(&st_bar_main, 0);

    lv_style_init(&st_bar_indic);
    lv_style_set_bg_color(&st_bar_indic, t->primary);
    lv_style_set_bg_opa(&st_bar_indic, LV_OPA_COVER);
    lv_style_set_radius(&st_bar_indic, t->radius_round);

    /* Slider / Arc knob: white circle on a primary rim (doc/theme.md row 8),
     * with a soft shadow. The pad is what makes a 6px slider track carry a
     * `slider_knob_size` (20px) handle. */
    lv_style_init(&st_knob);
    lv_style_set_bg_color(&st_knob, t->white);
    lv_style_set_bg_opa(&st_knob, LV_OPA_COVER);
    lv_style_set_radius(&st_knob, t->radius_round);
    lv_style_set_border_color(&st_knob, t->primary);
    lv_style_set_border_width(&st_knob, t->border_width * 2);
    lv_style_set_shadow_color(&st_knob, t->black);
    lv_style_set_shadow_opa(&st_knob, t->shadow_opa);
    lv_style_set_shadow_width(&st_knob, t->shadow_small_width);
    lv_style_set_pad_all(&st_knob,
                         (t->slider_knob_size - t->slider_track_size) / 2);

    /* Arc: light track + primary indicator, rounded line caps. */
    lv_style_init(&st_arc_main);
    lv_style_set_arc_color(&st_arc_main, t->border_light);
    lv_style_set_arc_width(&st_arc_main, t->arc_width);
    lv_style_set_arc_rounded(&st_arc_main, true);

    lv_style_init(&st_arc_indic);
    lv_style_set_arc_color(&st_arc_indic, t->primary);
    lv_style_set_arc_width(&st_arc_indic, t->arc_width);
    lv_style_set_arc_rounded(&st_arc_indic, true);

    /* Checkbox indicator (the box): rounded, outlined; primary when checked. */
    lv_style_init(&st_cb_box);
    lv_style_set_radius(&st_cb_box, t->radius_small);
    lv_style_set_bg_color(&st_cb_box, t->bg_overlay);
    lv_style_set_bg_opa(&st_cb_box, LV_OPA_COVER);
    lv_style_set_border_color(&st_cb_box, t->border_base);
    lv_style_set_border_width(&st_cb_box, t->border_width);
    lv_style_set_pad_all(&st_cb_box, t->space_xs);

    lv_style_init(&st_cb_box_checked);
    lv_style_set_bg_color(&st_cb_box_checked, t->primary);
    lv_style_set_border_color(&st_cb_box_checked, t->primary);
    lv_style_set_bg_image_src(&st_cb_box_checked, LV_SYMBOL_OK);
    lv_style_set_text_color(&st_cb_box_checked, t->white);

    /* Dropdown / Roller main: surface bg, 1px outline border, field radius. */
    lv_style_init(&st_dropdown_base);
    lv_style_set_bg_color(&st_dropdown_base, t->bg_overlay);
    lv_style_set_bg_opa(&st_dropdown_base, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown_base, t->text_primary);
    lv_style_set_border_color(&st_dropdown_base, t->border_base);
    lv_style_set_border_width(&st_dropdown_base, t->border_width);
    lv_style_set_radius(&st_dropdown_base, t->radius_field);

    /* Dropdown popup-list row under the pointer: §7 neutral hover surface. */
    lv_style_init(&st_dropdown_sel);
    lv_style_set_bg_color(&st_dropdown_sel, t->fill_light);
    lv_style_set_bg_opa(&st_dropdown_sel, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown_sel, t->text_primary);

    /* Roller centre row: filled primary with on_primary text. */
    lv_style_init(&st_roller_sel);
    lv_style_set_bg_color(&st_roller_sel, t->primary);
    lv_style_set_bg_opa(&st_roller_sel, LV_OPA_COVER);
    lv_style_set_text_color(&st_roller_sel, t->white);

    /* List container: doc/theme.md §8 row 16 — bg_overlay, radius_base and a
     * border_light hairline (was border_base, which is the field/input role). */
    lv_style_init(&st_list);
    lv_style_set_bg_color(&st_list, t->bg_overlay);
    lv_style_set_bg_opa(&st_list, LV_OPA_COVER);
    lv_style_set_border_color(&st_list, t->border_light);
    lv_style_set_border_width(&st_list, t->border_width);
    lv_style_set_radius(&st_list, t->radius_base);

    /* Table container: surface bg, 1px border, sharp corners. */
    lv_style_init(&st_table);
    lv_style_set_bg_color(&st_table, t->bg_overlay);
    lv_style_set_bg_opa(&st_table, LV_OPA_COVER);
    lv_style_set_border_color(&st_table, t->border_base);
    lv_style_set_border_width(&st_table, t->border_width);
    lv_style_set_radius(&st_table, 0);

    /* Table cell: doc/theme.md §8 row 26 — border_lighter grid (was
     * border_base) plus cell padding and text_regular body copy. */
    lv_style_init(&st_table_cell);
    lv_style_set_border_color(&st_table_cell, t->border_lighter);
    lv_style_set_border_width(&st_table_cell, t->border_width);
    lv_style_set_text_color(&st_table_cell, t->text_regular);
    lv_style_set_pad_hor(&st_table_cell, t->space_md);
    lv_style_set_pad_ver(&st_table_cell, t->space_sm);

    /* Calendar background. */
    lv_style_init(&st_calendar_main);
    lv_style_set_bg_color(&st_calendar_main, t->bg_overlay);
    lv_style_set_bg_opa(&st_calendar_main, LV_OPA_COVER);
    lv_style_set_radius(&st_calendar_main, t->radius_base);

    /* Calendar day-cell text. */
    lv_style_init(&st_calendar_items);
    lv_style_set_text_color(&st_calendar_items, t->text_primary);

    /* Calendar's CUSTOM_1 (today) / CUSTOM_2 (highlight) are buttonmatrix
     * control bits, not lv_state_t selectors. LVGL 9.2.2 consumes both in its
     * draw-task callback and substitutes g_theme.color_primary directly. */
    lv_style_init(&st_calendar_today);
    lv_style_set_bg_color(&st_calendar_today, t->primary_light_9);
    lv_style_set_bg_opa(&st_calendar_today, LV_OPA_COVER);
    lv_style_set_text_color(&st_calendar_today, t->primary);

    /* Menu container: doc/theme.md §8 row 27 — bg_base (was bg_overlay). */
    lv_style_init(&st_menu);
    lv_style_set_bg_color(&st_menu, t->bg_base);
    lv_style_set_bg_opa(&st_menu, LV_OPA_COVER);
    lv_style_set_border_color(&st_menu, t->border_light);
    lv_style_set_border_width(&st_menu, t->border_width);

    /* Scale main line: doc/theme.md §8 row 23 — border_base. */
    lv_style_init(&st_scale_main);
    lv_style_set_bg_opa(&st_scale_main, LV_OPA_TRANSP);
    lv_style_set_line_color(&st_scale_main, t->border_base);
    lv_style_set_line_width(&st_scale_main, t->border_width);
    lv_style_set_arc_color(&st_scale_main, t->border_base);
    lv_style_set_arc_width(&st_scale_main, t->border_width);

    /* Scale major ticks + labels: text_regular (was text_secondary). */
    lv_style_init(&st_scale_indic);
    lv_style_set_line_color(&st_scale_indic, t->text_regular);
    lv_style_set_line_width(&st_scale_indic, t->border_width);
    lv_style_set_length(&st_scale_indic, t->space_sm);
    lv_style_set_arc_color(&st_scale_indic, t->text_regular);
    lv_style_set_arc_width(&st_scale_indic, t->border_width);
    lv_style_set_text_color(&st_scale_indic, t->text_regular);

    /* Chart container. The MAIN part's line_color is what lv_chart's
     * draw_div_lines() uses for the division grid (doc/theme.md §8 row 20). */
    lv_style_init(&st_chart);
    lv_style_set_bg_color(&st_chart, t->bg_overlay);
    lv_style_set_bg_opa(&st_chart, LV_OPA_COVER);
    lv_style_set_border_color(&st_chart, t->border_base);
    lv_style_set_border_width(&st_chart, t->border_width);
    lv_style_set_radius(&st_chart, t->radius_field);
    lv_style_set_line_color(&st_chart, t->border_lighter);

    /* Line: primary 2px stroke. */
    lv_style_init(&st_line);
    lv_style_set_line_color(&st_line, t->primary);
    /* The locked metric table has no semantic line-stroke token; this 2px
     * widget contract is independent of the border_width hairline. */
    lv_style_set_line_width(&st_line, 2);
    lv_style_set_line_rounded(&st_line, true);

    /* Spangroup: transparent inline rich-text box. The text colour is set
     * explicitly (not left to inheritance) because a span with no per-span
     * colour otherwise keeps whatever the parent had when it was created —
     * which would freeze at the old scheme's colour after setTheme(). */
    lv_style_init(&st_span);
    lv_style_set_bg_opa(&st_span, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_span, 0);
    lv_style_set_text_color(&st_span, t->text_primary);

    /* LED: status dot. lv_led uses each resolved style colour's brightness as
     * a scale factor for led->color. White therefore preserves the assigned
     * colour exactly for the fill, gradient, border, shadow and outline. */
    lv_style_init(&st_led);
    lv_style_set_bg_color(&st_led, t->white);
    lv_style_set_bg_opa(&st_led, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&st_led, t->white);
    lv_style_set_shadow_color(&st_led, t->white);
    lv_style_set_border_color(&st_led, t->white);
    lv_style_set_outline_color(&st_led, t->white);
    lv_style_set_radius(&st_led, t->radius_round);
    lv_style_set_border_width(&st_led, 0);
    /* LED geometry is a separately locked 8px/2px glow contract, not the
     * shadow_small_width token used for ordinary elevation. */
    lv_style_set_shadow_width(&st_led, 8);
    lv_style_set_shadow_spread(&st_led, 2);

    styles_init_media(t);
    styles_init_controls(t);
}

/* doc/theme.md §8 rows 4, 30, 31 (the image family) + §8.1 (the imperative
 * msgbox). Split out of styles_init for the same size-budget reason as
 * styles_init_controls; identical lv_style_init/lv_style_set_* contract. */
static void styles_init_media(const sg_theme_tokens_t *t) {
    /* ── Image family (rows 4, 30, 31) ──────────────────────────────────
     * These widgets carry no colour of their own, so the theme owns only
     * their object-level chrome — the decoded pixels are never touched.
     * LVGL's default theme styles NONE of lv_image / lv_animimg /
     * lv_imagebutton (verified against the pinned lv_theme_default.c), so
     * this is their whole appearance beyond the generic base pass. */
    lv_style_init(&st_image);
    lv_style_set_bg_opa(&st_image, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_image, 0);
    lv_style_set_outline_width(&st_image, 0);
    lv_style_set_shadow_width(&st_image, 0);

    lv_style_init(&st_animimg);
    lv_style_set_bg_color(&st_animimg, t->bg_overlay);
    lv_style_set_bg_opa(&st_animimg, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_animimg, 0);

    /* A keyboard-navigated image must stay distinguishable; §7 pins the focus
     * ring to primary.light_5 at 2× border_width. Pointer FOCUSED is
     * deliberately NOT styled — rows 4 and 31 name FOCUS_KEY only. */
    lv_style_init(&st_image_focus);
    lv_style_set_outline_color(&st_image_focus, t->primary_light_5);
    lv_style_set_outline_width(&st_image_focus, t->border_width * 2);
    lv_style_set_outline_opa(&st_image_focus, LV_OPA_COVER);
    lv_style_set_outline_pad(&st_image_focus, t->space_xs);

    /* §6 reserves `disabled_opa` for image / animimg / imagebutton, "where
     * there is no colour to swap". image_opa is the property that actually
     * dims them: lv_obj_init_draw_image_dsc (lv_obj_draw.c:181) seeds
     * draw_dsc->opa from lv_obj_get_style_image_opa, and both lv_image
     * (lv_image.c:755) and lv_imagebutton (lv_imagebutton.c:204) build their
     * descriptor through it. Nothing else belongs here: row 31 warns that
     * LVGL swaps the *source image* per state, so a background fill would
     * fight the artwork. */
    lv_style_init(&st_image_dim);
    lv_style_set_image_opa(&st_image_dim, (lv_opa_t)t->disabled_opa);

    /* ── Imperative msgbox (§8.1) ───────────────────────────────────────── */

    /* radius_base (4) is what §8.1 spells out and §6 lists no msgbox-specific
     * override, so the previous hardcoded 8 was wrong. */
    lv_style_init(&st_msgbox);
    lv_style_set_bg_color(&st_msgbox, t->bg_overlay);
    lv_style_set_bg_opa(&st_msgbox, LV_OPA_COVER);
    lv_style_set_radius(&st_msgbox, t->radius_base);
    lv_style_set_shadow_color(&st_msgbox, t->black);
    lv_style_set_shadow_width(&st_msgbox, t->shadow_overlay_width);
    lv_style_set_shadow_opa(&st_msgbox, (lv_opa_t)t->shadow_opa);

    /* overlay_mask is opaque black because LVGL carries the 50% scrim in a
     * separate opacity property (§5). */
    lv_style_init(&st_msgbox_backdrop);
    lv_style_set_bg_color(&st_msgbox_backdrop, t->overlay_mask);
    lv_style_set_bg_opa(&st_msgbox_backdrop, (lv_opa_t)t->overlay_mask_opa);
    lv_style_set_border_width(&st_msgbox_backdrop, 0);
    lv_style_set_radius(&st_msgbox_backdrop, 0);

    /* The title face is font.large (20). Montserrat 20 is the safe fallback
     * until an application installs a CJK-capable role map. */
    lv_style_init(&st_msgbox_header);
    lv_style_set_bg_opa(&st_msgbox_header, LV_OPA_TRANSP);
    lv_style_set_border_color(&st_msgbox_header, t->border_lighter);
    lv_style_set_border_width(&st_msgbox_header, t->border_width);
    lv_style_set_border_side(&st_msgbox_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&st_msgbox_header, 0);
    lv_style_set_pad_hor(&st_msgbox_header, t->space_lg);
    lv_style_set_pad_ver(&st_msgbox_header, t->space_sm);
    lv_style_set_text_color(&st_msgbox_header, t->text_primary);
    lv_style_set_text_font(&st_msgbox_header,
                           g_role_fonts[2] ? g_role_fonts[2] : &lv_font_montserrat_20);

    /* font.base must be stated, NOT inherited. `lv_msgbox_create(NULL)` roots
     * the modal at `lv_layer_top()` (lv_msgbox.c:113) and `install_theme` only
     * adds `st_screen` to `lv_display_get_screen_active()`, so the inheritance
     * walk from the body label runs label → content → panel → backdrop →
     * layer_top → nothing and lands on LV_FONT_DEFAULT — a Latin-only face
     * that renders every CJK body string as tofu while the header (which does
     * state a font) rendered correctly. */
    lv_style_init(&st_msgbox_content);
    lv_style_set_bg_opa(&st_msgbox_content, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_msgbox_content, 0);
    lv_style_set_text_color(&st_msgbox_content, t->text_regular);
    lv_style_set_text_font(&st_msgbox_content,
                           g_role_fonts[0] ? g_role_fonts[0] : LV_FONT_DEFAULT);
    lv_style_set_pad_all(&st_msgbox_content, t->space_lg);

    /* Content scrollbar. The default theme claims SCROLLBAR|SCROLLED with an
     * opaque grey, so the scrolled variant must be re-stated to outrank it. */
    lv_style_init(&st_msgbox_scrollbar);
    lv_style_set_bg_color(&st_msgbox_scrollbar, t->text_secondary);
    lv_style_set_bg_opa(&st_msgbox_scrollbar, LV_OPA_30);
    lv_style_set_border_width(&st_msgbox_scrollbar, 0);
    lv_style_set_radius(&st_msgbox_scrollbar, t->radius_round);
    lv_style_set_width(&st_msgbox_scrollbar, t->scrollbar_size);

    lv_style_init(&st_msgbox_scrollbar_scrolled);
    lv_style_set_bg_opa(&st_msgbox_scrollbar_scrolled, LV_OPA_50);

    /* The "right-aligned" half of the §8.1 footer row is a LAYOUT decision and
     * is applied as one in apply_cb — see the lv_msgbox_footer_button_class
     * branch for why it cannot live in this style struct.
     *
     * font.base is stated here for the same reason it is stated on
     * st_msgbox_content: the caption of a footer button is a child LABEL whose
     * inheritance walk runs label → footer_button → footer → panel → backdrop →
     * layer_top. st_btn (which the footer button reuses, apply_cb below) sets
     * text_color but deliberately NO text_font, so without this line the walk
     * runs off the end of the tree into LV_FONT_DEFAULT and both captions
     * render as tofu. The footer is the nearest ancestor both buttons share. */
    lv_style_init(&st_msgbox_footer);
    lv_style_set_bg_opa(&st_msgbox_footer, LV_OPA_TRANSP);
    lv_style_set_border_color(&st_msgbox_footer, t->border_lighter);
    lv_style_set_border_width(&st_msgbox_footer, t->border_width);
    lv_style_set_border_side(&st_msgbox_footer, LV_BORDER_SIDE_TOP);
    lv_style_set_radius(&st_msgbox_footer, 0);
    lv_style_set_pad_hor(&st_msgbox_footer, t->space_lg);
    lv_style_set_pad_ver(&st_msgbox_footer, t->space_sm);
    lv_style_set_pad_column(&st_msgbox_footer, t->space_sm);
    lv_style_set_text_font(&st_msgbox_footer,
                           g_role_fonts[0] ? g_role_fonts[0] : LV_FONT_DEFAULT);

    /* The parent theme paints every msgbox header AND footer button
     * primary-filled (lv_theme_default.c:1150-1160); §8.1 wants the close
     * affordance transparent until hovered or pressed.
     *
     * font.base is stated for the third time here. The close affordance is an
     * lv_image holding LV_SYMBOL_CLOSE (lv_msgbox.c:173-183) and LVGL renders a
     * symbol source through the resolved text_font, so the glyph size would
     * otherwise be whatever the header's font.large happens to be — including
     * the &lv_font_montserrat_20 fallback taken when an app installs a partial
     * role map. Stating font.base pins the icon to the body face independently
     * of the title face, which is what §8.1 means by "as row 3". */
    lv_style_init(&st_msgbox_close_btn);
    lv_style_set_bg_opa(&st_msgbox_close_btn, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_msgbox_close_btn, 0);
    lv_style_set_shadow_width(&st_msgbox_close_btn, 0);
    lv_style_set_radius(&st_msgbox_close_btn, t->radius_base);
    lv_style_set_text_color(&st_msgbox_close_btn, t->text_regular);
    lv_style_set_text_font(&st_msgbox_close_btn,
                           g_role_fonts[0] ? g_role_fonts[0] : LV_FONT_DEFAULT);
    lv_style_set_pad_all(&st_msgbox_close_btn, t->space_xs);

    lv_style_init(&st_msgbox_close_btn_hover);
    lv_style_set_bg_color(&st_msgbox_close_btn_hover, t->fill_light);
    lv_style_set_bg_opa(&st_msgbox_close_btn_hover, LV_OPA_COVER);

    lv_style_init(&st_msgbox_close_btn_pressed);
    lv_style_set_bg_color(&st_msgbox_close_btn_pressed, t->fill_dark);
    lv_style_set_bg_opa(&st_msgbox_close_btn_pressed, LV_OPA_COVER);
    lv_style_set_transform_width(&st_msgbox_close_btn_pressed, 0);
    lv_style_set_transform_height(&st_msgbox_close_btn_pressed, 0);
}

/* doc/theme.md §7 (interaction recipe) + §8 rows 1-3, 5-13, 18-19. Split out of
 * styles_init purely to keep either function under the file's size budget; it
 * is called unconditionally from there and follows the identical
 * lv_style_init/lv_style_set_* contract. */
static void styles_init_controls(const sg_theme_tokens_t *t) {
    /* Row 1 — plain lv_obj container. `make_clean_container` already strips
     * decoration per instance with LOCAL styles (which outrank any theme
     * style), so this exists so the THEME alone still yields a transparent
     * container for lv_objs stonegui does not build itself. */
    lv_style_init(&st_view);
    lv_style_set_bg_color(&st_view, t->bg_overlay);
    lv_style_set_bg_opa(&st_view, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_view, 0);
    lv_style_set_outline_width(&st_view, 0);
    lv_style_set_radius(&st_view, 0);
    lv_style_set_pad_all(&st_view, 0);

    /* Generic SCROLLBAR pass. Every scrollable widget was previously left to
     * the parent theme, i.e. undefined once Todo 11 removes that parent. */
    lv_style_init(&st_scrollbar);
    lv_style_set_bg_color(&st_scrollbar, t->border_dark);
    lv_style_set_bg_opa(&st_scrollbar, LV_OPA_40);
    lv_style_set_radius(&st_scrollbar, t->radius_round);
    lv_style_set_width(&st_scrollbar, t->scrollbar_size);
    lv_style_set_pad_right(&st_scrollbar, t->space_xs);
    lv_style_set_pad_top(&st_scrollbar, t->space_xs);

    lv_style_init(&st_scrollbar_scrolled);
    lv_style_set_bg_opa(&st_scrollbar_scrolled, LV_OPA_COVER);

    /* §7 focus ring: primary.light_5 at border_width x2. */
    lv_style_init(&st_focus_ring);
    lv_style_set_outline_color(&st_focus_ring, t->primary_light_5);
    lv_style_set_outline_width(&st_focus_ring, t->border_width * 2);
    lv_style_set_outline_pad(&st_focus_ring, t->border_width);
    lv_style_set_outline_opa(&st_focus_ring, LV_OPA_COVER);

    /* EDITED (encoder "now editing") has no §7 entry; it mirrors the reference
     * theme's `outline_secondary`, which is the secondary/success accent. */
    lv_style_init(&st_edited_ring);
    lv_style_set_outline_color(&st_edited_ring, t->success);
    lv_style_set_outline_width(&st_edited_ring, t->border_width * 2);
    lv_style_set_outline_pad(&st_edited_ring, t->border_width);
    lv_style_set_outline_opa(&st_edited_ring, LV_OPA_COVER);

    /* §6: colour-bearing widgets express "disabled" with explicit roles, never
     * by scaling opacity. Border colour only — the base style owns the width,
     * so a borderless track stays borderless. */
    lv_style_init(&st_disabled_surface);
    lv_style_set_bg_color(&st_disabled_surface, t->fill_light);
    lv_style_set_bg_opa(&st_disabled_surface, LV_OPA_COVER);
    lv_style_set_text_color(&st_disabled_surface, t->text_disabled);
    lv_style_set_border_color(&st_disabled_surface, t->border_light);

    lv_style_init(&st_disabled_text);
    lv_style_set_text_color(&st_disabled_text, t->text_disabled);

    /* A disabled *filled* part (switch/slider/arc indicator, checked checkbox
     * box) must stay legible against its track, so it takes the disabled grey
     * as an accent rather than the near-invisible `fill_light`. */
    lv_style_init(&st_disabled_accent);
    lv_style_set_bg_color(&st_disabled_accent, t->text_disabled);
    lv_style_set_bg_opa(&st_disabled_accent, LV_OPA_COVER);
    lv_style_set_arc_color(&st_disabled_accent, t->text_disabled);
    lv_style_set_border_color(&st_disabled_accent, t->text_disabled);

    /* Row 3 — button. Default/pressed already live in styles_init. */
    lv_style_init(&st_btn_hover);
    lv_style_set_bg_color(&st_btn_hover, t->primary_light_3);

    /* §7 has no button CHECKED entry; a latched button reuses the engaged
     * accent that §7 gives "pressed", which brightens in dark mode. */
    lv_style_init(&st_btn_checked);
    lv_style_set_bg_color(&st_btn_checked, t->primary_dark_2);

    lv_style_init(&st_btn_disabled);
    lv_style_set_bg_color(&st_btn_disabled, t->fill_light);
    lv_style_set_bg_opa(&st_btn_disabled, LV_OPA_COVER);
    lv_style_set_text_color(&st_btn_disabled, t->text_disabled);
    lv_style_set_border_color(&st_btn_disabled, t->border_light);
    lv_style_set_border_width(&st_btn_disabled, t->border_width);

    /* Row 5 — input/textarea. */
    lv_style_init(&st_field_hover);
    lv_style_set_border_color(&st_field_hover, t->border_darker);

    lv_style_init(&st_field_cursor);
    lv_style_set_bg_color(&st_field_cursor, t->primary);
    lv_style_set_bg_opa(&st_field_cursor, LV_OPA_COVER);
    lv_style_set_border_color(&st_field_cursor, t->primary);

    /* Row 18 — the spinbox digit highlight is the CURSOR part in DEFAULT, not
     * in FOCUSED as for textarea (lv_theme_default.c applies
     * `bg_color_primary` to LV_PART_CURSOR with no state for lv_spinbox). */
    lv_style_init(&st_spinbox_cursor);
    lv_style_set_bg_color(&st_spinbox_cursor, t->primary);
    lv_style_set_bg_opa(&st_spinbox_cursor, LV_OPA_COVER);
    lv_style_set_text_color(&st_spinbox_cursor, t->white);

    /* Row 6 — switch knob when disabled. */
    lv_style_init(&st_sw_knob_disabled);
    lv_style_set_bg_color(&st_sw_knob_disabled, t->fill_base);
    lv_style_set_bg_opa(&st_sw_knob_disabled, LV_OPA_COVER);
    lv_style_set_border_color(&st_sw_knob_disabled, t->border_light);

    /* Row 8 — slider track thickness (`slider_track_size`). Kept separate from
     * st_bar_main so <progress> keeps its own height. */
    lv_style_init(&st_slider_main);
    lv_style_set_height(&st_slider_main, t->slider_track_size);

    lv_style_init(&st_knob_pressed);
    lv_style_set_border_color(&st_knob_pressed, t->primary_dark_2);
    lv_style_set_border_width(&st_knob_pressed, t->border_width * 2);
    lv_style_set_shadow_width(&st_knob_pressed, t->shadow_overlay_width);

    lv_style_init(&st_knob_disabled);
    lv_style_set_bg_color(&st_knob_disabled, t->fill_base);
    lv_style_set_bg_opa(&st_knob_disabled, LV_OPA_COVER);
    lv_style_set_border_color(&st_knob_disabled, t->border_light);
    lv_style_set_shadow_opa(&st_knob_disabled, LV_OPA_TRANSP);

    /* Row 10 — a spinner has no draggable handle, so its inherited arc knob is
     * zeroed out explicitly instead of relying on the parent theme happening to
     * skip LV_PART_KNOB for lv_spinner_class. */
    lv_style_init(&st_spinner_knob);
    lv_style_set_bg_opa(&st_spinner_knob, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_spinner_knob, 0);
    lv_style_set_shadow_opa(&st_spinner_knob, LV_OPA_TRANSP);
    lv_style_set_shadow_width(&st_spinner_knob, 0);
    lv_style_set_pad_all(&st_spinner_knob, 0);

    /* Row 11 — checkbox indicator states. */
    lv_style_init(&st_cb_box_hover);
    lv_style_set_border_color(&st_cb_box_hover, t->primary);

    lv_style_init(&st_cb_box_pressed);
    lv_style_set_bg_color(&st_cb_box_pressed, t->primary_light_9);
    lv_style_set_bg_opa(&st_cb_box_pressed, LV_OPA_COVER);
    lv_style_set_border_color(&st_cb_box_pressed, t->primary_dark_2);

    lv_style_init(&st_cb_box_checked_disabled);
    lv_style_set_bg_color(&st_cb_box_checked_disabled, t->text_disabled);
    lv_style_set_bg_opa(&st_cb_box_checked_disabled, LV_OPA_COVER);
    lv_style_set_border_color(&st_cb_box_checked_disabled, t->text_disabled);

    lv_style_init(&st_cb_checked_text);
    lv_style_set_text_color(&st_cb_checked_text, t->primary);

    /* Row 12 — dropdown button states + the LV_SYMBOL_DOWN arrow. */
    lv_style_init(&st_dropdown_hover);
    lv_style_set_border_color(&st_dropdown_hover, t->border_darker);

    lv_style_init(&st_dropdown_pressed);
    lv_style_set_bg_color(&st_dropdown_pressed, t->fill_light);
    lv_style_set_bg_opa(&st_dropdown_pressed, LV_OPA_COVER);
    lv_style_set_border_color(&st_dropdown_pressed, t->primary);

    lv_style_init(&st_dropdown_indic);
    lv_style_set_text_color(&st_dropdown_indic, t->text_placeholder);

    /* Row 12 (separate object) — the popup list is its own lv_dropdownlist
     * object, created eagerly by lv_dropdown_constructor, and is where LVGL
     * actually reads LV_PART_SELECTED and LV_PART_SCROLLBAR from. */
    lv_style_init(&st_dropdown_list);
    lv_style_set_bg_color(&st_dropdown_list, t->bg_overlay);
    lv_style_set_bg_opa(&st_dropdown_list, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown_list, t->text_primary);
    lv_style_set_border_color(&st_dropdown_list, t->border_light);
    lv_style_set_border_width(&st_dropdown_list, t->border_width);
    lv_style_set_radius(&st_dropdown_list, t->radius_base);
    lv_style_set_pad_all(&st_dropdown_list, t->space_xs);
    lv_style_set_shadow_color(&st_dropdown_list, t->black);
    lv_style_set_shadow_opa(&st_dropdown_list, t->shadow_opa);
    lv_style_set_shadow_width(&st_dropdown_list, t->shadow_overlay_width);

    lv_style_init(&st_dropdown_sel_checked);
    lv_style_set_bg_color(&st_dropdown_sel_checked, t->primary_light_9);
    lv_style_set_bg_opa(&st_dropdown_sel_checked, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown_sel_checked, t->primary);

    lv_style_init(&st_dropdown_sel_pressed);
    lv_style_set_bg_color(&st_dropdown_sel_pressed, t->primary_light_7);
    lv_style_set_bg_opa(&st_dropdown_sel_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown_sel_pressed, t->primary);

    /* Row 13 — a roller never enters CHECKED through stonegui, but the matrix
     * requires SELECTED|CHECKED to be deliberate rather than inherited, so the
     * engaged accent distinguishes it from the plain SELECTED band. */
    lv_style_init(&st_roller_sel_checked);
    lv_style_set_bg_color(&st_roller_sel_checked, t->primary_dark_2);
    lv_style_set_bg_opa(&st_roller_sel_checked, LV_OPA_COVER);
    lv_style_set_text_color(&st_roller_sel_checked, t->white);

    styles_init_composites(t);
}

/* doc/theme.md §8 rows 14-17, 20-29. Split out of styles_init for the same
 * file-size reason as styles_init_controls; identical lv_style_init contract. */
static void styles_init_composites(const sg_theme_tokens_t *t) {
    /* §7 shared row/item recipe: neutral hover surface, pressed surface, and
     * the selected/active surface reused by listButton, table, calendar day
     * cells, menu items and tab buttons. */
    lv_style_init(&st_hover_fill);
    lv_style_set_bg_color(&st_hover_fill, t->fill_light);
    lv_style_set_bg_opa(&st_hover_fill, LV_OPA_COVER);

    lv_style_init(&st_pressed_fill);
    lv_style_set_bg_color(&st_pressed_fill, t->fill_dark);
    lv_style_set_bg_opa(&st_pressed_fill, LV_OPA_COVER);
    lv_style_set_transform_width(&st_pressed_fill, 0);
    lv_style_set_transform_height(&st_pressed_fill, 0);

    lv_style_init(&st_active_surface);
    lv_style_set_bg_color(&st_active_surface, t->primary_light_9);
    lv_style_set_bg_opa(&st_active_surface, LV_OPA_COVER);
    lv_style_set_text_color(&st_active_surface, t->primary);

    lv_style_init(&st_plain_page);
    lv_style_set_bg_opa(&st_plain_page, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_plain_page, 0);
    lv_style_set_radius(&st_plain_page, 0);

    lv_style_init(&st_tabview_main);
    lv_style_set_bg_color(&st_tabview_main, t->bg_base);
    lv_style_set_bg_opa(&st_tabview_main, LV_OPA_COVER);
    lv_style_set_border_width(&st_tabview_main, 0);
    lv_style_set_pad_all(&st_tabview_main, 0);

    lv_style_init(&st_tab_header);
    lv_style_set_bg_color(&st_tab_header, t->bg_base);
    lv_style_set_bg_opa(&st_tab_header, LV_OPA_COVER);
    lv_style_set_border_color(&st_tab_header, t->border_light);
    lv_style_set_border_width(&st_tab_header, t->border_width);
    lv_style_set_border_side(&st_tab_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&st_tab_header, 0);
    lv_style_set_pad_all(&st_tab_header, 0);

    lv_style_init(&st_tab_btn);
    lv_style_set_bg_opa(&st_tab_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&st_tab_btn, t->text_regular);
    lv_style_set_border_width(&st_tab_btn, 0);
    lv_style_set_radius(&st_tab_btn, 0);
    lv_style_set_shadow_width(&st_tab_btn, 0);
    lv_style_set_pad_hor(&st_tab_btn, t->space_lg);
    lv_style_set_pad_ver(&st_tab_btn, t->space_md);

    /* Active tab: primary caption plus the Element Plus bottom indicator bar. */
    lv_style_init(&st_tab_btn_checked);    lv_style_set_bg_opa(&st_tab_btn_checked, LV_OPA_TRANSP);
    lv_style_set_text_color(&st_tab_btn_checked, t->primary);
    lv_style_set_border_color(&st_tab_btn_checked, t->primary);
    lv_style_set_border_width(&st_tab_btn_checked, t->border_width * 2);
    lv_style_set_border_side(&st_tab_btn_checked, LV_BORDER_SIDE_BOTTOM);

    lv_style_init(&st_tab_page);
    lv_style_set_bg_opa(&st_tab_page, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_tab_page, 0);
    lv_style_set_radius(&st_tab_page, 0);
    lv_style_set_pad_all(&st_tab_page, t->space_lg);

    lv_style_init(&st_list_btn);
    lv_style_set_bg_opa(&st_list_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&st_list_btn, t->text_regular);
    lv_style_set_border_color(&st_list_btn, t->border_lighter);
    lv_style_set_border_width(&st_list_btn, t->border_width);
    lv_style_set_border_side(&st_list_btn, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&st_list_btn, 0);
    lv_style_set_shadow_width(&st_list_btn, 0);
    lv_style_set_pad_hor(&st_list_btn, t->space_md);
    lv_style_set_pad_ver(&st_list_btn, t->space_sm);

    lv_style_init(&st_chart_items);
    /* Series strokes are a separately locked 2px chart contract; they do not
     * represent a border and therefore must not follow border_width. */
    lv_style_set_line_width(&st_chart_items, 2);
    lv_style_set_line_rounded(&st_chart_items, true);
    lv_style_set_pad_column(&st_chart_items, t->space_xs);

    lv_style_init(&st_chart_indic);
    lv_style_set_radius(&st_chart_indic, t->radius_round);
    lv_style_set_bg_color(&st_chart_indic, t->primary);
    lv_style_set_bg_opa(&st_chart_indic, LV_OPA_COVER);
    lv_style_set_width(&st_chart_indic, t->space_sm);
    lv_style_set_height(&st_chart_indic, t->space_sm);

    lv_style_init(&st_chart_cursor);
    lv_style_set_line_color(&st_chart_cursor, t->primary);
    lv_style_set_line_width(&st_chart_cursor, t->border_width);

    lv_style_init(&st_btnm_main);
    lv_style_set_bg_color(&st_btnm_main, t->bg_overlay);
    lv_style_set_bg_opa(&st_btnm_main, LV_OPA_COVER);
    lv_style_set_border_color(&st_btnm_main, t->border_light);
    lv_style_set_border_width(&st_btnm_main, t->border_width);
    lv_style_set_radius(&st_btnm_main, t->radius_base);
    lv_style_set_pad_all(&st_btnm_main, t->space_sm);

    lv_style_init(&st_btnm_item);
    lv_style_set_bg_color(&st_btnm_item, t->bg_overlay);
    lv_style_set_bg_opa(&st_btnm_item, LV_OPA_COVER);
    lv_style_set_text_color(&st_btnm_item, t->text_regular);
    lv_style_set_border_color(&st_btnm_item, t->border_base);
    lv_style_set_border_width(&st_btnm_item, t->border_width);
    lv_style_set_radius(&st_btnm_item, t->radius_base);

    lv_style_init(&st_btnm_item_checked);
    lv_style_set_bg_color(&st_btnm_item_checked, t->primary);
    lv_style_set_bg_opa(&st_btnm_item_checked, LV_OPA_COVER);
    lv_style_set_text_color(&st_btnm_item_checked, t->white);
    lv_style_set_border_color(&st_btnm_item_checked, t->primary);

    lv_style_init(&st_cal_grid_main);
    lv_style_set_bg_opa(&st_cal_grid_main, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_cal_grid_main, 0);
    lv_style_set_pad_all(&st_cal_grid_main, t->space_sm);

    lv_style_init(&st_cal_item_disabled);
    lv_style_set_text_color(&st_cal_item_disabled, t->text_disabled);
    lv_style_set_bg_opa(&st_cal_item_disabled, LV_OPA_TRANSP);

    lv_style_init(&st_cal_header);
    lv_style_set_bg_opa(&st_cal_header, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_cal_header, 0);
    lv_style_set_pad_hor(&st_cal_header, t->space_sm);
    lv_style_set_pad_ver(&st_cal_header, t->space_xs);

    lv_style_init(&st_cal_header_btn);
    lv_style_set_bg_opa(&st_cal_header_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&st_cal_header_btn, t->text_regular);
    lv_style_set_border_width(&st_cal_header_btn, 0);
    lv_style_set_shadow_width(&st_cal_header_btn, 0);
    lv_style_set_radius(&st_cal_header_btn, t->radius_base);
    lv_style_set_pad_all(&st_cal_header_btn, t->space_xs);

    lv_style_init(&st_scale_items);
    lv_style_set_line_color(&st_scale_items, t->border_dark);
    lv_style_set_line_width(&st_scale_items, t->border_width);
    lv_style_set_length(&st_scale_items, t->space_xs);
    lv_style_set_arc_color(&st_scale_items, t->border_dark);
    lv_style_set_arc_width(&st_scale_items, t->border_width);

    lv_style_init(&st_table_cell_hover);
    lv_style_set_bg_color(&st_table_cell_hover, t->fill_light);
    lv_style_set_bg_opa(&st_table_cell_hover, LV_OPA_COVER);

    lv_style_init(&st_menu_sidebar);
    lv_style_set_bg_color(&st_menu_sidebar, t->bg_base);
    lv_style_set_bg_opa(&st_menu_sidebar, LV_OPA_COVER);
    lv_style_set_border_color(&st_menu_sidebar, t->border_light);
    lv_style_set_border_width(&st_menu_sidebar, t->border_width);
    lv_style_set_border_side(&st_menu_sidebar, LV_BORDER_SIDE_RIGHT);
    lv_style_set_pad_all(&st_menu_sidebar, 0);

    lv_style_init(&st_menu_main_cont);
    lv_style_set_bg_opa(&st_menu_main_cont, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_menu_main_cont, 0);
    lv_style_set_pad_all(&st_menu_main_cont, 0);

    lv_style_init(&st_menu_header);
    lv_style_set_bg_opa(&st_menu_header, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_menu_header, 0);
    lv_style_set_pad_hor(&st_menu_header, t->space_sm);
    lv_style_set_pad_ver(&st_menu_header, t->space_xs);

    lv_style_init(&st_menu_separator);
    lv_style_set_bg_color(&st_menu_separator, t->border_lighter);
    lv_style_set_bg_opa(&st_menu_separator, LV_OPA_COVER);
    lv_style_set_border_color(&st_menu_separator, t->border_lighter);
    lv_style_set_pad_ver(&st_menu_separator, t->space_xs);

    lv_style_init(&st_menu_section);
    lv_style_set_bg_color(&st_menu_section, t->bg_overlay);
    lv_style_set_bg_opa(&st_menu_section, LV_OPA_COVER);
    lv_style_set_radius(&st_menu_section, t->radius_base);
    lv_style_set_border_width(&st_menu_section, 0);

    lv_style_init(&st_menu_item);
    lv_style_set_bg_opa(&st_menu_item, LV_OPA_TRANSP);
    lv_style_set_text_color(&st_menu_item, t->text_regular);
    lv_style_set_border_width(&st_menu_item, 0);
    lv_style_set_radius(&st_menu_item, t->radius_base);
    lv_style_set_pad_hor(&st_menu_item, t->space_md);
    lv_style_set_pad_ver(&st_menu_item, t->space_sm);

    lv_style_init(&st_kb_main);
    lv_style_set_bg_color(&st_kb_main, t->bg_page);
    lv_style_set_bg_opa(&st_kb_main, LV_OPA_COVER);
    lv_style_set_border_width(&st_kb_main, 0);
    lv_style_set_radius(&st_kb_main, 0);
    lv_style_set_pad_all(&st_kb_main, t->space_xs);

    lv_style_init(&st_kb_item);
    lv_style_set_bg_color(&st_kb_item, t->bg_overlay);
    lv_style_set_bg_opa(&st_kb_item, LV_OPA_COVER);
    lv_style_set_text_color(&st_kb_item, t->text_primary);
    lv_style_set_border_width(&st_kb_item, 0);
    lv_style_set_radius(&st_kb_item, t->radius_base);
    lv_style_set_shadow_width(&st_kb_item, 0);

    /* The built-in keymaps tag modifier keys with LV_BUTTONMATRIX_CTRL_CHECKED,
     * so CHECKED is what paints them (lv_keyboard.c default_kb_ctrl_*_map). */
    lv_style_init(&st_kb_item_checked);
    lv_style_set_bg_color(&st_kb_item_checked, t->primary);
    lv_style_set_bg_opa(&st_kb_item_checked, LV_OPA_COVER);
    lv_style_set_text_color(&st_kb_item_checked, t->white);
}

/* ── apply callback: layer our styles by widget class ───────────────────── */

/* LVGL 9.2.2 builds a tabview from two anonymous lv_obj children: index 0 is
 * the tab bar, index 1 the content container (lv_tabview.c constructor +
 * lv_tabview_get_tab_bar/_get_content). Tab buttons are real lv_button
 * children of the tab bar and the page from lv_tabview_add_tab is a plain
 * lv_obj child of the content container — there is NO internal buttonmatrix,
 * contrary to doc/theme.md row 14. Having no class of their own, these three
 * can only be recognised positionally, exactly as lv_theme_default.c does. */
static bool sg_is_tabview_child(lv_obj_t *obj, uint32_t index) {
    lv_obj_t *parent = lv_obj_get_parent(obj);
    return parent != NULL &&
           lv_obj_check_type(parent, &lv_tabview_class) &&
           lv_obj_get_child(parent, index) == obj;
}

static bool sg_is_tabview_grandchild(lv_obj_t *obj, uint32_t index) {
    lv_obj_t *parent = lv_obj_get_parent(obj);
    return parent != NULL && sg_is_tabview_child(parent, index);
}

static bool sg_parent_is(lv_obj_t *obj, const lv_obj_class_t *cls) {
    lv_obj_t *parent = lv_obj_get_parent(obj);
    return parent != NULL && lv_obj_check_type(parent, cls);
}

static void sg_add_scrollbar(lv_obj_t *obj) {
    lv_obj_add_style(obj, &st_scrollbar, LV_PART_SCROLLBAR);
    lv_obj_add_style(obj, &st_scrollbar_scrolled,
                     LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
}

static void sg_theme_apply_cb(lv_theme_t *th, lv_obj_t *obj) {
    (void)th;

    /* Screens (no parent) get the scaffold background + default text colour. */
    if (lv_obj_get_parent(obj) == NULL) {
        lv_obj_add_style(obj, &st_screen, 0);
        sg_add_scrollbar(obj);
        return;
    }

    const lv_obj_class_t *cls = lv_obj_get_class(obj);

    /* on_surface text for every widget EXCEPT labels — both halves of this
     * condition have regressed before, in opposite directions:
     *   - drop the style   → the default theme's own `color_text` (via
     *     `styles.card`, and on list/table/calendar/menu/chart) survives a
     *     scheme swap and goes invisible in dark mode;
     *   - apply it to labels → a Button's caption IS a child label, and a
     *     style on the label beats the on_primary it inherits from the
     *     button, painting captions grey over the primary fill.
     * Labels must inherit; their ancestor is what carries the colour. */
    if (cls != &lv_label_class) lv_obj_add_style(obj, &st_text, 0);

    /* Row 1. Exact-class match, so composite sub-objects that merely descend
     * from lv_obj (dropdown popup list, menu conts, …) keep their own branch.
     * The tabview's three anonymous lv_obj parts and the calendar's own
     * children are checked first — they ARE exact lv_objs, so without this
     * they would take the plain-View styling and lose rows 14/15. */
    if (cls == &lv_obj_class) {
        if (sg_is_tabview_child(obj, 0)) {
            lv_obj_add_style(obj, &st_tab_header, 0);
            lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        }
        else if (sg_is_tabview_child(obj, 1)) {
            lv_obj_add_style(obj, &st_plain_page, 0);
            sg_add_scrollbar(obj);
        }
        else if (sg_is_tabview_grandchild(obj, 1)) {
            lv_obj_add_style(obj, &st_tab_page, 0);
            sg_add_scrollbar(obj);
        }
        else if (sg_parent_is(obj, &lv_calendar_class)) {
            lv_obj_add_style(obj, &st_plain_page, 0);
        }
        else {
            lv_obj_add_style(obj, &st_view, 0);
            sg_add_scrollbar(obj);
        }
    }
    else if (cls == &lv_button_class) {
        /* Tab buttons and the calendar header's month arrows are ordinary
         * lv_buttons; both must lose the filled-primary look before the
         * generic row 3 styling is applied. */
        if (sg_is_tabview_grandchild(obj, 0)) {
            lv_obj_add_style(obj, &st_tab_btn, 0);
            lv_obj_add_style(obj, &st_hover_fill, LV_STATE_HOVERED);
            lv_obj_add_style(obj, &st_pressed_fill, LV_STATE_PRESSED);
            lv_obj_add_style(obj, &st_tab_btn_checked, LV_STATE_CHECKED);
            lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
            lv_obj_add_style(obj, &st_disabled_text, LV_STATE_DISABLED);
        }
        else if (sg_parent_is(obj, &lv_calendar_header_arrow_class)) {
            lv_obj_add_style(obj, &st_cal_header_btn, 0);
            lv_obj_add_style(obj, &st_hover_fill, LV_STATE_HOVERED);
            lv_obj_add_style(obj, &st_pressed_fill, LV_STATE_PRESSED);
        }
        else {
            lv_obj_add_style(obj, &st_btn, 0);
            lv_obj_add_style(obj, &st_btn_hover, LV_STATE_HOVERED);
            lv_obj_add_style(obj, &st_btn_pressed, LV_STATE_PRESSED);
            lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
            lv_obj_add_style(obj, &st_btn_checked, LV_STATE_CHECKED);
            lv_obj_add_style(obj, &st_btn_disabled, LV_STATE_DISABLED);
        }
    }
    else if (cls == &lv_textarea_class) {
        lv_obj_add_style(obj, &st_field, 0);
        lv_obj_add_style(obj, &st_field_hover, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_field_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_scrollbar, LV_PART_SCROLLBAR);
        lv_obj_add_style(obj, &st_scrollbar_scrolled,
                         LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
        lv_obj_add_style(obj, &st_field_cursor,
                         LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_add_style(obj, &st_field_placeholder,
                         LV_PART_TEXTAREA_PLACEHOLDER);
    }
    else if (cls == &lv_spinbox_class) {
        lv_obj_add_style(obj, &st_field, 0);
        lv_obj_add_style(obj, &st_field_hover, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_field_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_spinbox_cursor, LV_PART_CURSOR);
    }
    else if (cls == &lv_switch_class) {
        lv_obj_add_style(obj, &st_sw_bg, 0);
        lv_obj_add_style(obj, &st_sw_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_sw_bg_on,
                         LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_disabled_accent,
                         LV_PART_INDICATOR | LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_sw_knob, LV_PART_KNOB);
        lv_obj_add_style(obj, &st_sw_knob_disabled,
                         LV_PART_KNOB | LV_STATE_DISABLED);
    }
    else if (cls == &lv_bar_class) {
        lv_obj_add_style(obj, &st_bar_main, 0);
        /* lv_bar inherits group_def/editable FALSE from lv_obj_class and
         * js_createNode does not add <progress> to the focus group, so these
         * two are unreachable through today's JS API. They match row 7 and the
         * reference theme, and become the only definition once Todo 11 drops
         * the parent theme. */
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_bar_indic, LV_PART_INDICATOR);
    }
    else if (cls == &lv_slider_class) {
        lv_obj_add_style(obj, &st_bar_main, 0);
        lv_obj_add_style(obj, &st_slider_main, 0);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_bar_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_disabled_accent,
                         LV_PART_INDICATOR | LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_knob, LV_PART_KNOB);
        lv_obj_add_style(obj, &st_knob_pressed,
                         LV_PART_KNOB | LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_knob_disabled,
                         LV_PART_KNOB | LV_STATE_DISABLED);
    }
    else if (cls == &lv_arc_class) {
        lv_obj_add_style(obj, &st_arc_main, 0);
        lv_obj_add_style(obj, &st_arc_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_disabled_accent,
                         LV_PART_INDICATOR | LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_knob, LV_PART_KNOB);
        lv_obj_add_style(obj, &st_knob_pressed,
                         LV_PART_KNOB | LV_STATE_PRESSED);
    }
    else if (cls == &lv_spinner_class) {
        lv_obj_add_style(obj, &st_arc_main, 0);
        lv_obj_add_style(obj, &st_arc_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_spinner_knob, LV_PART_KNOB);
    }
    else if (cls == &lv_checkbox_class) {
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_cb_checked_text, LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_disabled_text, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_cb_box, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_cb_box_hover,
                         LV_PART_INDICATOR | LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_cb_box_pressed,
                         LV_PART_INDICATOR | LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_cb_box_checked,
                         LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_disabled_surface,
                         LV_PART_INDICATOR | LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_cb_box_checked_disabled,
                         LV_PART_INDICATOR | LV_STATE_CHECKED | LV_STATE_DISABLED);
    }
    else if (cls == &lv_dropdown_class) {
        lv_obj_add_style(obj, &st_dropdown_base, 0);
        lv_obj_add_style(obj, &st_dropdown_hover, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_dropdown_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_dropdown_indic, LV_PART_INDICATOR);
    }
    else if (cls == &lv_dropdownlist_class) {
        lv_obj_add_style(obj, &st_dropdown_list, 0);
        lv_obj_add_style(obj, &st_scrollbar, LV_PART_SCROLLBAR);
        lv_obj_add_style(obj, &st_scrollbar_scrolled,
                         LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
        lv_obj_add_style(obj, &st_dropdown_sel, LV_PART_SELECTED);
        lv_obj_add_style(obj, &st_dropdown_sel_checked,
                         LV_PART_SELECTED | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_dropdown_sel_pressed,
                         LV_PART_SELECTED | LV_STATE_PRESSED);
    }
    else if (cls == &lv_roller_class) {
        lv_obj_add_style(obj, &st_dropdown_base, 0);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
        lv_obj_add_style(obj, &st_roller_sel, LV_PART_SELECTED);
        lv_obj_add_style(obj, &st_roller_sel_checked,
                         LV_PART_SELECTED | LV_STATE_CHECKED);
    }
    else if (cls == &lv_tabview_class) {
        lv_obj_add_style(obj, &st_tabview_main, 0);
    }
    else if (cls == &lv_list_class) {
        lv_obj_add_style(obj, &st_list, 0);
        sg_add_scrollbar(obj);
    }
    else if (cls == &lv_list_button_class) {
        /* Row 17. lv_list_add_button creates lv_list_button_class (base
         * lv_button_class) parented directly to the list, so exact-class
         * dispatch already keeps it clear of the row 3 button branch. */
        lv_obj_add_style(obj, &st_list_btn, 0);
        lv_obj_add_style(obj, &st_hover_fill, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_pressed_fill, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_active_surface, LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_disabled_text, LV_STATE_DISABLED);
    }
    else if (cls == &lv_table_class) {
        lv_obj_add_style(obj, &st_table, 0);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        sg_add_scrollbar(obj);
        lv_obj_add_style(obj, &st_table_cell, LV_PART_ITEMS);
        lv_obj_add_style(obj, &st_table_cell_hover,
                         LV_PART_ITEMS | LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_pressed_fill,
                         LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_focus_ring,
                         LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring,
                         LV_PART_ITEMS | LV_STATE_EDITED);
    }
    else if (cls == &lv_buttonmatrix_class) {
        /* The calendar's day grid is an exact lv_buttonmatrix child of the
         * calendar, so the parent-aware case MUST precede the standalone
         * row 21 styling (same predicate lv_theme_default.c uses). */
        if (sg_parent_is(obj, &lv_calendar_class)) {
            lv_obj_add_style(obj, &st_cal_grid_main, 0);
            lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
            lv_obj_add_style(obj, &st_calendar_items, LV_PART_ITEMS);
            lv_obj_add_style(obj, &st_hover_fill,
                             LV_PART_ITEMS | LV_STATE_HOVERED);
            lv_obj_add_style(obj, &st_pressed_fill,
                             LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_add_style(obj, &st_cal_item_disabled,
                             LV_PART_ITEMS | LV_STATE_DISABLED);
        }
        else {
            lv_obj_add_style(obj, &st_btnm_main, 0);
            lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
            lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
            lv_obj_add_style(obj, &st_disabled_surface, LV_STATE_DISABLED);
            lv_obj_add_style(obj, &st_btnm_item, LV_PART_ITEMS);
            lv_obj_add_style(obj, &st_hover_fill,
                             LV_PART_ITEMS | LV_STATE_HOVERED);
            lv_obj_add_style(obj, &st_pressed_fill,
                             LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_add_style(obj, &st_btnm_item_checked,
                             LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_add_style(obj, &st_focus_ring,
                             LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
            lv_obj_add_style(obj, &st_edited_ring,
                             LV_PART_ITEMS | LV_STATE_EDITED);
            lv_obj_add_style(obj, &st_disabled_surface,
                             LV_PART_ITEMS | LV_STATE_DISABLED);
        }
    }
    else if (cls == &lv_keyboard_class) {
        /* lv_keyboard_class derives from lv_buttonmatrix_class, but
         * lv_obj_get_class returns the exact class, so this never collides
         * with the branch above and needs no ordering guarantee. */
        lv_obj_add_style(obj, &st_kb_main, 0);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring, LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_kb_item, LV_PART_ITEMS);
        lv_obj_add_style(obj, &st_hover_fill,
                         LV_PART_ITEMS | LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_pressed_fill,
                         LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_kb_item_checked,
                         LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_focus_ring,
                         LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_edited_ring,
                         LV_PART_ITEMS | LV_STATE_EDITED);
        lv_obj_add_style(obj, &st_disabled_surface,
                         LV_PART_ITEMS | LV_STATE_DISABLED);
    }
    else if (cls == &lv_calendar_class) {
        lv_obj_add_style(obj, &st_calendar_main, 0);
    }
    else if (cls == &lv_calendar_header_arrow_class ||
             cls == &lv_calendar_header_dropdown_class) {
        /* Row 22: the dropdown header's inner lv_dropdowns are picked up by
         * the row 12 branch, so only the container needs styling here. */
        lv_obj_add_style(obj, &st_cal_header, 0);
    }
    else if (cls == &lv_menu_class) {
        lv_obj_add_style(obj, &st_menu, 0);
    }
    else if (cls == &lv_menu_sidebar_cont_class) {
        lv_obj_add_style(obj, &st_menu_sidebar, 0);
        sg_add_scrollbar(obj);
    }
    else if (cls == &lv_menu_main_cont_class) {
        lv_obj_add_style(obj, &st_menu_main_cont, 0);
        sg_add_scrollbar(obj);
    }
    else if (cls == &lv_menu_sidebar_header_cont_class ||
             cls == &lv_menu_main_header_cont_class) {
        lv_obj_add_style(obj, &st_menu_header, 0);
    }
    else if (cls == &lv_menu_page_class) {
        lv_obj_add_style(obj, &st_plain_page, 0);
        sg_add_scrollbar(obj);
    }
    else if (cls == &lv_menu_section_class) {
        lv_obj_add_style(obj, &st_menu_section, 0);
    }
    else if (cls == &lv_menu_separator_class) {
        lv_obj_add_style(obj, &st_menu_separator, 0);
    }
    else if (cls == &lv_menu_cont_class) {
        lv_obj_add_style(obj, &st_menu_item, 0);
        lv_obj_add_style(obj, &st_hover_fill, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_pressed_fill, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_active_surface, LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
    }
    else if (cls == &lv_scale_class) {
        lv_obj_add_style(obj, &st_scale_main, 0);
        lv_obj_add_style(obj, &st_scale_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_scale_items, LV_PART_ITEMS);
    }
    else if (cls == &lv_chart_class) {
        lv_obj_add_style(obj, &st_chart, 0);
        sg_add_scrollbar(obj);
        lv_obj_add_style(obj, &st_chart_items, LV_PART_ITEMS);
        lv_obj_add_style(obj, &st_chart_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_chart_cursor, LV_PART_CURSOR);
    }
    else if (cls == &lv_line_class) {
        lv_obj_add_style(obj, &st_line, 0);
    }
    else if (cls == &lv_spangroup_class) {
        lv_obj_add_style(obj, &st_span, 0);
    }
    else if (cls == &lv_led_class) {
        lv_obj_add_style(obj, &st_led, 0);
    }
    else if (cls == &lv_image_class) {
        lv_obj_add_style(obj, &st_image, 0);
        lv_obj_add_style(obj, &st_image_focus, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_image_dim, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_image_dim, LV_STATE_DISABLED);
    }
    else if (cls == &lv_animimg_class) {
        /* Row 30 lists DEFAULT only. lv_animimg subclasses lv_image, but
         * lv_obj_get_class returns the exact class, so it never falls into the
         * branch above — and it must not: dimming a running frame loop would
         * alter the very pixels row 30 says to leave alone. */
        lv_obj_add_style(obj, &st_animimg, 0);
    }
    else if (cls == &lv_imagebutton_class) {
        lv_obj_add_style(obj, &st_image, 0);
        lv_obj_add_style(obj, &st_image_focus, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_image_dim, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_image_dim, LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_image_dim, LV_STATE_DISABLED);
    }
    else if (cls == &lv_msgbox_class) {
        lv_obj_add_style(obj, &st_msgbox, 0);
    }
    else if (cls == &lv_msgbox_backdrop_class) {
        lv_obj_add_style(obj, &st_msgbox_backdrop, 0);
    }
    else if (cls == &lv_msgbox_header_class) {
        lv_obj_add_style(obj, &st_msgbox_header, 0);
    }
    else if (cls == &lv_msgbox_content_class) {
        lv_obj_add_style(obj, &st_msgbox_content, 0);
        lv_obj_add_style(obj, &st_msgbox_scrollbar, LV_PART_SCROLLBAR);
        lv_obj_add_style(obj, &st_msgbox_scrollbar_scrolled,
                         LV_PART_SCROLLBAR | LV_STATE_SCROLLED);
    }
    else if (cls == &lv_msgbox_footer_class) {
        lv_obj_add_style(obj, &st_msgbox_footer, 0);
    }
    else if (cls == &lv_msgbox_footer_button_class) {
        /* §8.1 says footer buttons look "as row 3", so they reuse the standalone
         * button styles verbatim rather than growing a parallel set. They need
         * their own branch because lv_msgbox_footer_button_class derives from
         * lv_obj_class, NOT lv_button_class (lv_msgbox.c:73-80) — the generic
         * lv_button_class branch above never sees them.
         *
         * Right-aligning the footer is settled here too. It cannot be a style:
         * lv_msgbox_add_footer_button calls lv_obj_set_flex_align on the footer
         * (lv_msgbox.c:209), which writes LOCAL styles that outrank any theme
         * style. That call has already run by the time this first button is
        * initialised, so re-stating the alignment now is what actually sticks. */
        lv_obj_add_style(obj, &st_btn, 0);
        lv_obj_add_style(obj, &st_btn_hover, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(obj, &st_focus_ring, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(obj, &st_btn_checked, LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_btn_disabled, LV_STATE_DISABLED);

        lv_obj_t *footer = lv_obj_get_parent(obj);
        if (footer && lv_obj_get_class(footer) == &lv_msgbox_footer_class) {
            lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        }
    }
    else if (cls == &lv_msgbox_header_button_class) {
        lv_obj_add_style(obj, &st_msgbox_close_btn, 0);
        lv_obj_add_style(obj, &st_msgbox_close_btn_hover, LV_STATE_HOVERED);
        lv_obj_add_style(obj, &st_msgbox_close_btn_pressed, LV_STATE_PRESSED);
    }
    /* Labels/Text intentionally have no style here: they inherit text_color
     * from their nearest ancestor (screen → dark, Button → white). */
}

/* ── public API ─────────────────────────────────────────────────────────── */

static void install_theme(lv_display_t *disp) {
    lv_display_t *d = disp ? disp : lv_display_get_default();
    const sg_theme_tokens_t *t = &g_tokens;

    g_theme.apply_cb        = sg_theme_apply_cb;
    g_theme.parent          = NULL;
    g_theme.user_data       = NULL;
    g_theme.disp            = d;
    g_theme.color_primary   = t->primary;
    g_theme.color_secondary = t->success;
    g_theme.font_small      = g_role_fonts[0] ? g_role_fonts[0] : LV_FONT_DEFAULT;
    g_theme.font_normal     = g_role_fonts[0] ? g_role_fonts[0] : LV_FONT_DEFAULT;
    g_theme.font_large      = g_role_fonts[2] ? g_role_fonts[2] : LV_FONT_DEFAULT;
    g_theme.flags           = 0;

    lv_display_set_theme(d, &g_theme);
    lv_obj_add_style(lv_display_get_screen_active(d), &st_screen, 0);
}

void sg_theme_init(lv_display_t *disp, const lv_font_t *font) {
    /* Only the FIRST call establishes the light preset. `sg_theme_set_font`
     * re-enters here to swap the default font; clobbering g_tokens there would
     * silently revert an app that already called setTheme("dark"). */
    if (!g_styles_inited) g_tokens = sg_tokens_light;
    if (font) {
        for (size_t i = 0; i < 4; i++) g_role_fonts[i] = font;
    }
    styles_init(&g_tokens);
    install_theme(disp);
}

void sg_theme_set_font(lv_display_t *disp, const lv_font_t *font) {
    sg_theme_init(disp, font);
    /* Restyle any widgets already created so they pick up the new font. */
    lv_obj_report_style_change(NULL);
}

void sg_theme_set_role_fonts(lv_display_t *disp,
                             const lv_font_t *base,
                             const lv_font_t *medium,
                             const lv_font_t *large,
                             const lv_font_t *display) {
    g_role_fonts[0] = base;
    g_role_fonts[1] = medium;
    g_role_fonts[2] = large;
    g_role_fonts[3] = display;
    styles_init(&g_tokens);
    install_theme(disp);
    lv_obj_report_style_change(NULL);
}

void sg_theme_set_scheme(lv_display_t *disp, const char *scheme) {
    g_dark   = (strcmp(scheme, "dark") == 0);
    g_tokens = g_dark ? sg_tokens_dark : sg_tokens_light;
    styles_init(&g_tokens);
    install_theme(disp);
    lv_obj_report_style_change(NULL);
}

sg_theme_token_result_t sg_theme_set_token(lv_display_t *disp, const char *name,
                                             sg_theme_token_value_t value) {
    for (size_t i = 0; i < sizeof(token_registry) / sizeof(token_registry[0]); i++) {
        const sg_theme_token_entry_t *entry = &token_registry[i];
        if (strcmp(name, entry->name) != 0) continue;
        if (value.kind != entry->kind) return SG_THEME_TOKEN_WRONG_KIND;

        uint8_t *target = (uint8_t *)&g_tokens + entry->offset;
        if (entry->kind == SG_TOKEN_COLOR) {
            *(lv_color_t *)target = value.color;
        } else {
            *(int32_t *)target = value.integer;
        }
        styles_init(&g_tokens);
        install_theme(disp);
        lv_obj_report_style_change(NULL);
        return SG_THEME_TOKEN_OK;
    }
    return SG_THEME_TOKEN_UNKNOWN;
}

sg_theme_token_result_t sg_theme_get_token(const char *name,
                                            sg_theme_token_value_t *value) {
    for (size_t i = 0; i < sizeof(token_registry) / sizeof(token_registry[0]); i++) {
        const sg_theme_token_entry_t *entry = &token_registry[i];
        if (strcmp(name, entry->name) != 0) continue;

        const uint8_t *source = (const uint8_t *)&g_tokens + entry->offset;
        value->kind = entry->kind;
        if (entry->kind == SG_TOKEN_COLOR) {
            value->color = *(const lv_color_t *)source;
        } else {
            value->integer = *(const int32_t *)source;
        }
        return SG_THEME_TOKEN_OK;
    }
    return SG_THEME_TOKEN_UNKNOWN;
}
