#ifndef SG_THEME_H
#define SG_THEME_H

#include "lvgl.h"

/**
 * sg_theme — stonegui's default look & feel.
 *
 * Provides a modern, Element-Plus-inspired baseline for every widget by
 * installing a self-contained LVGL theme:
 *
 *     sg_theme  →  user inline styles
 *
 * sg_theme owns structural defaults, colours, typography, corner radii,
 * spacing and elevation. Per-widget `style={…}` props from JS always win.
 *
 * Colours and radii are no longer baked-in macros — they live in a
 * `sg_theme_tokens_t` value. Two presets ship: `sg_tokens_light` (Element
 * Plus light scheme, used by `sg_theme_init`) and `sg_tokens_dark` (the
 * matching dark scheme, ready for a future dark-mode switch).
 */

typedef struct {
    lv_color_t primary;
    lv_color_t primary_light_3;
    lv_color_t primary_light_5;
    lv_color_t primary_light_7;
    lv_color_t primary_light_9;
    union { lv_color_t primary_dark_2; lv_color_t primary_dark; };

    union { lv_color_t success; lv_color_t secondary; };
    lv_color_t success_light_3;
    lv_color_t success_light_5;
    lv_color_t success_light_7;
    lv_color_t success_light_9;
    lv_color_t success_dark_2;

    lv_color_t warning;
    lv_color_t warning_light_3;
    lv_color_t warning_light_5;
    lv_color_t warning_light_7;
    lv_color_t warning_light_9;
    lv_color_t warning_dark_2;

    union { lv_color_t danger; lv_color_t error; };
    union { lv_color_t danger_light_3; lv_color_t error_light_3; };
    union { lv_color_t danger_light_5; lv_color_t error_light_5; };
    union { lv_color_t danger_light_7; lv_color_t error_light_7; };
    union { lv_color_t danger_light_9; lv_color_t error_light_9; };
    union { lv_color_t danger_dark_2; lv_color_t error_dark_2; };

    lv_color_t info;
    lv_color_t info_light_3;
    lv_color_t info_light_5;
    lv_color_t info_light_7;
    lv_color_t info_light_9;
    lv_color_t info_dark_2;

    union { lv_color_t text_primary; lv_color_t on_surface; };
    lv_color_t text_regular;
    union { lv_color_t text_secondary; lv_color_t on_variant; };
    lv_color_t text_placeholder;
    lv_color_t text_disabled;
    union { lv_color_t border_base; lv_color_t outline; };
    union { lv_color_t border_light; lv_color_t track; };
    lv_color_t border_lighter;
    lv_color_t border_extra_light;
    lv_color_t border_dark;
    lv_color_t border_darker;
    lv_color_t fill_base;
    lv_color_t fill_light;
    lv_color_t fill_lighter;
    lv_color_t fill_extra_light;
    lv_color_t fill_dark;
    lv_color_t fill_darker;
    lv_color_t fill_blank;
    union { lv_color_t bg_page; lv_color_t bg; };
    lv_color_t bg_base;
    union { lv_color_t bg_overlay; lv_color_t surface; };
    lv_color_t overlay_mask;
    union { lv_color_t white; lv_color_t on_primary; };
    lv_color_t black;

    int32_t radius_base;
    int32_t radius_small;
    int32_t radius_round;
    int32_t border_width;
    int32_t space_xs;
    int32_t space_sm;
    int32_t space_md;
    int32_t space_lg;
    int32_t space_xl;
    int32_t control_height;
    int32_t slider_track_size;
    int32_t slider_knob_size;
    int32_t arc_width;
    int32_t scrollbar_size;
    int32_t shadow_small_width;
    int32_t shadow_overlay_width;
    int32_t shadow_opa;
    int32_t disabled_opa;
    int32_t overlay_mask_opa;
    int32_t btn_pad_hor;
    int32_t btn_pad_ver;
    int32_t radius_btn;
    int32_t radius_field;
} sg_theme_tokens_t;

typedef enum {
    SG_TOKEN_COLOR,
    SG_TOKEN_INT,
} sg_theme_token_kind_t;

typedef struct {
    sg_theme_token_kind_t kind;
    union {
        lv_color_t color;
        int32_t integer;
    };
} sg_theme_token_value_t;

typedef enum {
    SG_THEME_TOKEN_OK,
    SG_THEME_TOKEN_UNKNOWN,
    SG_THEME_TOKEN_WRONG_KIND,
} sg_theme_token_result_t;

extern const sg_theme_tokens_t sg_tokens_light;
extern const sg_theme_tokens_t sg_tokens_dark;

/* Install the theme on `disp` (NULL = default display). `font` is the default
 * text font (NULL keeps LVGL's built-in font). Call once at startup. */
void sg_theme_init(lv_display_t *disp, const lv_font_t *font);

/* Swap the default text font, keeping all sg_theme customizations.
 * Intended to be called before the UI tree is built (see setDefaultFont). */
void sg_theme_set_font(lv_display_t *disp, const lv_font_t *font);

/* Install the four typography-role faces. LVGL exposes only small, normal and
 * large theme slots, so small/normal use base and large uses large; medium and
 * display remain available to explicit per-widget theme styles. */
void sg_theme_set_role_fonts(lv_display_t *disp,
                             const lv_font_t *base,
                             const lv_font_t *medium,
                             const lv_font_t *large,
                             const lv_font_t *display);

/* Replace the entire live token set with one of the built-in schemes
 * ("light" or "dark"), rebuild every shared style in place and trigger a
 * full repaint. All existing widgets pick up the new colours immediately. */
void sg_theme_set_scheme(lv_display_t *disp, const char *scheme);

/* Canonical ramp names use dots (for example `primary.light_3`); neutral and
 * metric names use their specification spelling. Legacy names share storage
 * with their canonical colour roles; the two legacy radii remain independent. */
sg_theme_token_result_t sg_theme_set_token(lv_display_t *disp, const char *name,
                                             sg_theme_token_value_t value);

/* Read the current value through the same typed registry used by the setter.
 * Callers must inspect the returned kind; no token names are duplicated. */
sg_theme_token_result_t sg_theme_get_token(const char *name,
                                            sg_theme_token_value_t *value);

#endif /* SG_THEME_H */
