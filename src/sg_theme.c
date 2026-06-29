/**
 * sg_theme.c — stonegui default theme
 *
 * A small, opinionated design system that makes the raw LVGL widgets look
 * closer to Element Plus / modern web defaults. Implemented as a custom LVGL
 * theme whose parent is the built-in default theme, so we only override what
 * matters (colours, radii, spacing, elevation) and inherit the rest.
 *
 * Layering (lowest → highest priority):
 *   1. default theme   — structural baseline
 *   2. sg_theme        — this file's overrides
 *   3. user styles     — per-widget style={…} props (local styles, always win)
 *
 * Colours and corner radii are no longer macros — they live in
 * `sg_theme_tokens_t` values (see sg_theme.h). `sg_tokens_light` is the
 * Element Plus light scheme used by `sg_theme_init`; `sg_tokens_dark` is the
 * matching dark scheme, ready for a future runtime switch.
 */

#include "sg_theme.h"
#include "src/themes/lv_theme_private.h"

#include <string.h>

/* ── Design tokens ──────────────────────────────────────────────────────── */

const sg_theme_tokens_t sg_tokens_light = {
    .primary      = LV_COLOR_MAKE(0x40, 0x9E, 0xFF),  /* Element Plus blue   */
    .primary_dark = LV_COLOR_MAKE(0x33, 0x7E, 0xCC),  /* hover/pressed       */
    .on_primary   = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .secondary    = LV_COLOR_MAKE(0x67, 0xC2, 0x3A),  /* success green       */
    .bg           = LV_COLOR_MAKE(0xF2, 0xF3, 0xF5),  /* page background     */
    .surface      = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),  /* card / input        */
    .on_surface   = LV_COLOR_MAKE(0x30, 0x31, 0x33),  /* primary text        */
    .on_variant   = LV_COLOR_MAKE(0x90, 0x93, 0x99),  /* placeholder         */
    .outline      = LV_COLOR_MAKE(0xDC, 0xDF, 0xE6),  /* borders             */
    .track        = LV_COLOR_MAKE(0xE4, 0xE7, 0xED),  /* switch/bar track    */
    .danger       = LV_COLOR_MAKE(0xF5, 0x6C, 0x6C),
    .warning      = LV_COLOR_MAKE(0xE6, 0xA2, 0x3C),
    .radius_btn   = 4,
    .radius_field = 4,
};

const sg_theme_tokens_t sg_tokens_dark = {
    .primary      = LV_COLOR_MAKE(0x40, 0x9E, 0xFF),
    .primary_dark = LV_COLOR_MAKE(0x33, 0x7E, 0xCC),
    .on_primary   = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .secondary    = LV_COLOR_MAKE(0x67, 0xC2, 0x3A),
    .bg           = LV_COLOR_MAKE(0x14, 0x14, 0x14),
    .surface      = LV_COLOR_MAKE(0x1D, 0x1E, 0x1F),
    .on_surface   = LV_COLOR_MAKE(0xCF, 0xD3, 0xDC),
    .on_variant   = LV_COLOR_MAKE(0xA3, 0xA6, 0xAD),
    .outline      = LV_COLOR_MAKE(0x41, 0x42, 0x43),
    .track        = LV_COLOR_MAKE(0x2C, 0x2C, 0x2C),
    .danger       = LV_COLOR_MAKE(0xF5, 0x6C, 0x6C),
    .warning      = LV_COLOR_MAKE(0xE6, 0xA2, 0x3C),
    .radius_btn   = 4,
    .radius_field = 4,
};

/* Mutable working copy — patched by set_scheme / set_token at runtime. */
static sg_theme_tokens_t g_tokens;

/* Padding stays static — not part of the visual identity that swaps with
 * a token set. */
#define SG_PAD_FIELD      12
#define SG_PAD_BTN_HOR    20
#define SG_PAD_BTN_VER    10

/* ── Shared styles (persistent — referenced by widgets for their lifetime) ── */

static bool                       g_styles_inited = false;
static const sg_theme_tokens_t   *g_active_tokens = NULL;
static lv_theme_t                 g_theme;        /* parent = default theme */

static lv_style_t st_screen;      /* root background + inherited text colour    */
static lv_style_t st_btn, st_btn_pressed;
static lv_style_t st_field, st_field_focused, st_field_placeholder;
static lv_style_t st_sw_bg, st_sw_bg_on, st_sw_knob;
static lv_style_t st_bar_main, st_bar_indic;
static lv_style_t st_knob;         /* slider / arc knob                          */
static lv_style_t st_arc_main, st_arc_indic;
static lv_style_t st_cb_box, st_cb_box_checked;
static lv_style_t st_dropdown_base, st_dropdown_sel;
static lv_style_t st_roller_sel;
static lv_style_t st_list;
static lv_style_t st_table, st_table_cell;
static lv_style_t st_calendar_main, st_calendar_items, st_calendar_today;
static lv_style_t st_menu;
static lv_style_t st_scale_main, st_scale_indic;
static lv_style_t st_chart;
static lv_style_t st_line;
static lv_style_t st_msgbox;

/* (Re)initialise every shared style from the given token set. The first call
 * builds the styles; subsequent calls with a different token pointer reset
 * each style in place (lv_style_reset → lv_style_init) so widgets that already
 * reference them pick up the new colours on the next `lv_obj_report_style_change`.
 * Same-pointer calls are no-ops thanks to `g_active_tokens`. */
static void styles_init(const sg_theme_tokens_t *t) {
    if (g_styles_inited && g_active_tokens == t) return;

    if (g_styles_inited) {
        lv_style_reset(&st_screen);
        lv_style_reset(&st_btn);
        lv_style_reset(&st_btn_pressed);
        lv_style_reset(&st_field);
        lv_style_reset(&st_field_focused);
        lv_style_reset(&st_field_placeholder);
        lv_style_reset(&st_sw_bg);
        lv_style_reset(&st_sw_bg_on);
        lv_style_reset(&st_sw_knob);
        lv_style_reset(&st_bar_main);
        lv_style_reset(&st_bar_indic);
        lv_style_reset(&st_knob);
        lv_style_reset(&st_arc_main);
        lv_style_reset(&st_arc_indic);
        lv_style_reset(&st_cb_box);
        lv_style_reset(&st_cb_box_checked);
        lv_style_reset(&st_dropdown_base);
        lv_style_reset(&st_dropdown_sel);
        lv_style_reset(&st_roller_sel);
        lv_style_reset(&st_list);
        lv_style_reset(&st_table);
        lv_style_reset(&st_table_cell);
        lv_style_reset(&st_calendar_main);
        lv_style_reset(&st_calendar_items);
        lv_style_reset(&st_calendar_today);
        lv_style_reset(&st_menu);
        lv_style_reset(&st_scale_main);
        lv_style_reset(&st_scale_indic);
        lv_style_reset(&st_chart);
        lv_style_reset(&st_line);
        lv_style_reset(&st_msgbox);
    }

    g_styles_inited = true;
    g_active_tokens = t;

    /* Screen: scaffold background + app-wide default text colour.
     * text_color is inheritable, so every label inherits on_surface unless
     * an ancestor (e.g. a Button) or the widget itself overrides it. */
    lv_style_init(&st_screen);
    lv_style_set_bg_color(&st_screen, t->bg);
    lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);
    lv_style_set_text_color(&st_screen, t->on_surface);

    /* Button: filled primary, rounded, flat (Element Plus = no shadow). */
    lv_style_init(&st_btn);
    lv_style_set_bg_color(&st_btn, t->primary);
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_text_color(&st_btn, t->on_primary);   /* inherited by label   */
    lv_style_set_border_width(&st_btn, 0);
    lv_style_set_radius(&st_btn, t->radius_btn);
    lv_style_set_pad_hor(&st_btn, SG_PAD_BTN_HOR);
    lv_style_set_pad_ver(&st_btn, SG_PAD_BTN_VER);

    /* Pressed: darken; cancel the default theme's grow. */
    lv_style_init(&st_btn_pressed);
    lv_style_set_bg_color(&st_btn_pressed, t->primary_dark);
    lv_style_set_transform_width(&st_btn_pressed, 0);
    lv_style_set_transform_height(&st_btn_pressed, 0);

    /* Text field (Input): outlined, rounded, comfortable padding. */
    lv_style_init(&st_field);
    lv_style_set_bg_color(&st_field, t->surface);
    lv_style_set_bg_opa(&st_field, LV_OPA_COVER);
    lv_style_set_text_color(&st_field, t->on_surface);
    lv_style_set_radius(&st_field, t->radius_field);
    lv_style_set_border_color(&st_field, t->outline);
    lv_style_set_border_width(&st_field, 1);
    lv_style_set_pad_all(&st_field, SG_PAD_FIELD);

    /* Focused field: primary 2px outline. */
    lv_style_init(&st_field_focused);
    lv_style_set_border_color(&st_field_focused, t->primary);
    lv_style_set_border_width(&st_field_focused, 2);

    /* Placeholder text colour. */
    lv_style_init(&st_field_placeholder);
    lv_style_set_text_color(&st_field_placeholder, t->on_variant);

    /* Switch: pill track (grey off / primary on) with a white circular knob. */
    lv_style_init(&st_sw_bg);
    lv_style_set_bg_color(&st_sw_bg, t->track);
    lv_style_set_bg_opa(&st_sw_bg, LV_OPA_COVER);
    lv_style_set_radius(&st_sw_bg, LV_RADIUS_CIRCLE);

    lv_style_init(&st_sw_bg_on);
    lv_style_set_bg_color(&st_sw_bg_on, t->primary);

    lv_style_init(&st_sw_knob);
    lv_style_set_bg_color(&st_sw_knob, lv_color_white());
    lv_style_set_radius(&st_sw_knob, LV_RADIUS_CIRCLE);

    /* Progress (bar): thin pill, light track, primary indicator. */
    lv_style_init(&st_bar_main);
    lv_style_set_bg_color(&st_bar_main, t->track);
    lv_style_set_bg_opa(&st_bar_main, LV_OPA_COVER);
    lv_style_set_radius(&st_bar_main, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&st_bar_main, 0);

    lv_style_init(&st_bar_indic);
    lv_style_set_bg_color(&st_bar_indic, t->primary);
    lv_style_set_bg_opa(&st_bar_indic, LV_OPA_COVER);
    lv_style_set_radius(&st_bar_indic, LV_RADIUS_CIRCLE);

    /* Slider / Arc knob: white circle with a soft shadow (Material thumb). */
    lv_style_init(&st_knob);
    lv_style_set_bg_color(&st_knob, lv_color_white());
    lv_style_set_bg_opa(&st_knob, LV_OPA_COVER);
    lv_style_set_radius(&st_knob, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&st_knob, 0);
    lv_style_set_shadow_color(&st_knob, lv_color_black());
    lv_style_set_shadow_opa(&st_knob, LV_OPA_30);
    lv_style_set_shadow_width(&st_knob, 6);
    lv_style_set_pad_all(&st_knob, 6);

    /* Arc: light track + primary indicator, rounded line caps. */
    lv_style_init(&st_arc_main);
    lv_style_set_arc_color(&st_arc_main, t->track);
    lv_style_set_arc_width(&st_arc_main, 10);
    lv_style_set_arc_rounded(&st_arc_main, true);

    lv_style_init(&st_arc_indic);
    lv_style_set_arc_color(&st_arc_indic, t->primary);
    lv_style_set_arc_width(&st_arc_indic, 10);
    lv_style_set_arc_rounded(&st_arc_indic, true);

    /* Checkbox indicator (the box): rounded, outlined; primary when checked. */
    lv_style_init(&st_cb_box);
    lv_style_set_radius(&st_cb_box, 4);
    lv_style_set_bg_color(&st_cb_box, t->surface);
    lv_style_set_bg_opa(&st_cb_box, LV_OPA_COVER);
    lv_style_set_border_color(&st_cb_box, t->outline);
    lv_style_set_border_width(&st_cb_box, 2);
    lv_style_set_pad_all(&st_cb_box, 4);

    lv_style_init(&st_cb_box_checked);
    lv_style_set_bg_color(&st_cb_box_checked, t->primary);
    lv_style_set_border_color(&st_cb_box_checked, t->primary);

    /* Dropdown / Roller main: surface bg, 1px outline border, field radius. */
    lv_style_init(&st_dropdown_base);
    lv_style_set_bg_color(&st_dropdown_base, t->surface);
    lv_style_set_bg_opa(&st_dropdown_base, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown_base, t->on_surface);
    lv_style_set_border_color(&st_dropdown_base, t->outline);
    lv_style_set_border_width(&st_dropdown_base, 1);
    lv_style_set_radius(&st_dropdown_base, t->radius_field);

    /* Dropdown popup-list highlighted row: track-coloured fill. */
    lv_style_init(&st_dropdown_sel);
    lv_style_set_bg_color(&st_dropdown_sel, t->track);
    lv_style_set_bg_opa(&st_dropdown_sel, LV_OPA_COVER);

    /* Roller centre row: filled primary with on_primary text. */
    lv_style_init(&st_roller_sel);
    lv_style_set_bg_color(&st_roller_sel, t->primary);
    lv_style_set_bg_opa(&st_roller_sel, LV_OPA_COVER);
    lv_style_set_text_color(&st_roller_sel, t->on_primary);

    /* List container: surface bg, 1px outline border, field radius. */
    lv_style_init(&st_list);
    lv_style_set_bg_color(&st_list, t->surface);
    lv_style_set_bg_opa(&st_list, LV_OPA_COVER);
    lv_style_set_border_color(&st_list, t->outline);
    lv_style_set_border_width(&st_list, 1);
    lv_style_set_radius(&st_list, t->radius_field);

    /* Table container: surface bg, 1px border, sharp corners. */
    lv_style_init(&st_table);
    lv_style_set_bg_color(&st_table, t->surface);
    lv_style_set_bg_opa(&st_table, LV_OPA_COVER);
    lv_style_set_border_color(&st_table, t->outline);
    lv_style_set_border_width(&st_table, 1);
    lv_style_set_radius(&st_table, 0);

    /* Table cell separator: 1px border per cell. */
    lv_style_init(&st_table_cell);
    lv_style_set_border_color(&st_table_cell, t->outline);
    lv_style_set_border_width(&st_table_cell, 1);

    /* Calendar background. */
    lv_style_init(&st_calendar_main);
    lv_style_set_bg_color(&st_calendar_main, t->surface);
    lv_style_set_bg_opa(&st_calendar_main, LV_OPA_COVER);

    /* Calendar day-cell text. */
    lv_style_init(&st_calendar_items);
    lv_style_set_text_color(&st_calendar_items, t->on_surface);

    /* Calendar today highlight: faint primary tint, circular badge.
     * 48/255 ≈ 19% primary mixed into surface — matches Element Plus
     * "today" cell intensity without overpowering the digit. */
    lv_color_t today_bg = lv_color_mix(t->primary, t->surface, 48);
    lv_style_init(&st_calendar_today);
    lv_style_set_bg_color(&st_calendar_today, today_bg);
    lv_style_set_bg_opa(&st_calendar_today, LV_OPA_COVER);
    lv_style_set_radius(&st_calendar_today, LV_RADIUS_CIRCLE);

    /* Menu container: surface bg, 1px outline border. */
    lv_style_init(&st_menu);
    lv_style_set_bg_color(&st_menu, t->surface);
    lv_style_set_bg_opa(&st_menu, LV_OPA_COVER);
    lv_style_set_border_color(&st_menu, t->outline);
    lv_style_set_border_width(&st_menu, 1);

    /* Scale background: transparent (lives over arbitrary parents). */
    lv_style_init(&st_scale_main);
    lv_style_set_bg_opa(&st_scale_main, LV_OPA_TRANSP);

    /* Scale tick marks: on_variant for both straight and arc scales. */
    lv_style_init(&st_scale_indic);
    lv_style_set_line_color(&st_scale_indic, t->on_variant);
    lv_style_set_arc_color(&st_scale_indic, t->on_variant);

    /* Chart container: surface bg, 1px outline border, field radius. */
    lv_style_init(&st_chart);
    lv_style_set_bg_color(&st_chart, t->surface);
    lv_style_set_bg_opa(&st_chart, LV_OPA_COVER);
    lv_style_set_border_color(&st_chart, t->outline);
    lv_style_set_border_width(&st_chart, 1);
    lv_style_set_radius(&st_chart, t->radius_field);

    /* Line: primary 2px stroke. */
    lv_style_init(&st_line);
    lv_style_set_line_color(&st_line, t->primary);
    lv_style_set_line_width(&st_line, 2);

    /* Msgbox panel: surface bg, larger 8px radius (modal). */
    lv_style_init(&st_msgbox);
    lv_style_set_bg_color(&st_msgbox, t->surface);
    lv_style_set_bg_opa(&st_msgbox, LV_OPA_COVER);
    lv_style_set_radius(&st_msgbox, 8);
}

/* ── apply callback: layer our styles by widget class ───────────────────── */

static void sg_theme_apply_cb(lv_theme_t *th, lv_obj_t *obj) {
    (void)th;

    /* Screens (no parent) get the scaffold background + default text colour. */
    if (lv_obj_get_parent(obj) == NULL) {
        lv_obj_add_style(obj, &st_screen, 0);
        return;
    }

    const lv_obj_class_t *cls = lv_obj_get_class(obj);

    if (cls == &lv_button_class) {
        lv_obj_add_style(obj, &st_btn, 0);
        lv_obj_add_style(obj, &st_btn_pressed, LV_STATE_PRESSED);
    }
    else if (cls == &lv_textarea_class) {
        lv_obj_add_style(obj, &st_field, 0);
        lv_obj_add_style(obj, &st_field_focused, LV_STATE_FOCUSED);
        lv_obj_add_style(obj, &st_field_placeholder,
                         LV_PART_TEXTAREA_PLACEHOLDER);
    }
    else if (cls == &lv_spinbox_class) {
        lv_obj_add_style(obj, &st_field, 0);
        lv_obj_add_style(obj, &st_field_focused, LV_STATE_FOCUSED);
    }
    else if (cls == &lv_switch_class) {
        lv_obj_add_style(obj, &st_sw_bg, 0);
        lv_obj_add_style(obj, &st_sw_bg_on, LV_STATE_CHECKED);
        lv_obj_add_style(obj, &st_sw_knob, LV_PART_KNOB);
    }
    else if (cls == &lv_bar_class) {
        lv_obj_add_style(obj, &st_bar_main, 0);
        lv_obj_add_style(obj, &st_bar_indic, LV_PART_INDICATOR);
    }
    else if (cls == &lv_slider_class) {
        lv_obj_add_style(obj, &st_bar_main, 0);
        lv_obj_add_style(obj, &st_bar_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_knob, LV_PART_KNOB);
    }
    else if (cls == &lv_arc_class) {
        lv_obj_add_style(obj, &st_arc_main, 0);
        lv_obj_add_style(obj, &st_arc_indic, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_knob, LV_PART_KNOB);
    }
    else if (cls == &lv_spinner_class) {
        lv_obj_add_style(obj, &st_arc_main, 0);
        lv_obj_add_style(obj, &st_arc_indic, LV_PART_INDICATOR);
    }
    else if (cls == &lv_checkbox_class) {
        lv_obj_add_style(obj, &st_cb_box, LV_PART_INDICATOR);
        lv_obj_add_style(obj, &st_cb_box_checked,
                         LV_PART_INDICATOR | LV_STATE_CHECKED);
    }
    else if (cls == &lv_dropdown_class) {
        lv_obj_add_style(obj, &st_dropdown_base, 0);
        lv_obj_add_style(obj, &st_dropdown_sel,
                         LV_PART_SELECTED | LV_STATE_DEFAULT);
    }
    else if (cls == &lv_roller_class) {
        lv_obj_add_style(obj, &st_dropdown_base, 0);
        lv_obj_add_style(obj, &st_roller_sel, LV_PART_SELECTED);
    }
    else if (cls == &lv_list_class) {
        lv_obj_add_style(obj, &st_list, 0);
    }
    else if (cls == &lv_table_class) {
        lv_obj_add_style(obj, &st_table, 0);
        lv_obj_add_style(obj, &st_table_cell, LV_PART_ITEMS);
    }
    else if (cls == &lv_calendar_class) {
        lv_obj_add_style(obj, &st_calendar_main, 0);
        lv_obj_add_style(obj, &st_calendar_items, LV_PART_ITEMS);
        lv_obj_add_style(obj, &st_calendar_today,
                         LV_PART_ITEMS | LV_STATE_FOCUSED);
    }
    else if (cls == &lv_menu_class) {
        lv_obj_add_style(obj, &st_menu, 0);
    }
    else if (cls == &lv_scale_class) {
        lv_obj_add_style(obj, &st_scale_main, 0);
        lv_obj_add_style(obj, &st_scale_indic, LV_PART_INDICATOR);
    }
    else if (cls == &lv_chart_class) {
        lv_obj_add_style(obj, &st_chart, 0);
    }
    else if (cls == &lv_line_class) {
        lv_obj_add_style(obj, &st_line, 0);
    }
    else if (cls == &lv_msgbox_class) {
        lv_obj_add_style(obj, &st_msgbox, 0);
    }
    /* Labels/Text intentionally have no style here: they inherit text_color
     * from their nearest ancestor (screen → dark, Button → white). */
}

/* ── public API ─────────────────────────────────────────────────────────── */

void sg_theme_init(lv_display_t *disp, const lv_font_t *font) {
    g_tokens = sg_tokens_light;
    styles_init(&g_tokens);

    lv_display_t *d = disp ? disp : lv_display_get_default();
    const sg_theme_tokens_t *t = g_active_tokens;

    /* (Re)initialise the built-in default theme — handles base structure and
     * carries the default font/colours. */
    lv_theme_t *base = lv_theme_default_init(
        d, t->primary, t->secondary,
        /* dark = */ false,
        font ? font : LV_FONT_DEFAULT);

    /* Copy fonts/colours from the base so runtime lookups on the active theme
     * (e.g. lv_theme_get_font_normal) resolve correctly, then chain + override. */
    g_theme = *base;
    g_theme.parent   = base;
    g_theme.apply_cb = sg_theme_apply_cb;

    lv_display_set_theme(d, &g_theme);
}

void sg_theme_set_font(lv_display_t *disp, const lv_font_t *font) {
    sg_theme_init(disp, font);
    /* Restyle any widgets already created so they pick up the new font. */
    lv_obj_report_style_change(NULL);
}

void sg_theme_set_scheme(lv_display_t *disp, const char *scheme) {
    (void)disp;
    g_tokens = (strcmp(scheme, "dark") == 0) ? sg_tokens_dark : sg_tokens_light;
    styles_init(&g_tokens);
    lv_obj_report_style_change(NULL);
}

void sg_theme_set_token(lv_display_t *disp, const char *name, lv_color_t c) {
    (void)disp;
    if      (!strcmp(name, "primary"))      g_tokens.primary      = c;
    else if (!strcmp(name, "primary_dark")) g_tokens.primary_dark = c;
    else if (!strcmp(name, "on_primary"))   g_tokens.on_primary   = c;
    else if (!strcmp(name, "secondary"))    g_tokens.secondary    = c;
    else if (!strcmp(name, "bg"))           g_tokens.bg           = c;
    else if (!strcmp(name, "surface"))      g_tokens.surface      = c;
    else if (!strcmp(name, "on_surface"))   g_tokens.on_surface   = c;
    else if (!strcmp(name, "on_variant"))   g_tokens.on_variant   = c;
    else if (!strcmp(name, "outline"))      g_tokens.outline      = c;
    else if (!strcmp(name, "track"))        g_tokens.track        = c;
    else if (!strcmp(name, "danger"))       g_tokens.danger       = c;
    else if (!strcmp(name, "warning"))      g_tokens.warning      = c;
    styles_init(&g_tokens);
    lv_obj_report_style_change(NULL);
}
