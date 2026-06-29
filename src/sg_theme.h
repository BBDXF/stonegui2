#ifndef SG_THEME_H
#define SG_THEME_H

#include "lvgl.h"

/**
 * sg_theme — stonegui's default look & feel.
 *
 * Provides a modern, Element-Plus-inspired baseline for every widget by
 * layering a custom LVGL theme on top of the built-in default theme:
 *
 *     default theme (base)  →  sg_theme (overrides)  →  user inline styles
 *
 * The base theme handles structural defaults; sg_theme restyles colours,
 * corner radii, spacing and elevation so widgets feel consistent out of the
 * box. Per-widget `style={…}` props from JS always win on top.
 *
 * Colours and radii are no longer baked-in macros — they live in a
 * `sg_theme_tokens_t` value. Two presets ship: `sg_tokens_light` (Element
 * Plus light scheme, used by `sg_theme_init`) and `sg_tokens_dark` (the
 * matching dark scheme, ready for a future dark-mode switch).
 */

typedef struct {
    lv_color_t primary;
    lv_color_t primary_dark;
    lv_color_t on_primary;
    lv_color_t secondary;
    lv_color_t bg;
    lv_color_t surface;
    lv_color_t on_surface;
    lv_color_t on_variant;
    lv_color_t outline;
    lv_color_t track;
    lv_color_t danger;
    lv_color_t warning;
    int32_t    radius_btn;
    int32_t    radius_field;
} sg_theme_tokens_t;

extern const sg_theme_tokens_t sg_tokens_light;
extern const sg_theme_tokens_t sg_tokens_dark;

/* Install the theme on `disp` (NULL = default display). `font` is the default
 * text font (NULL keeps LVGL's built-in font). Call once at startup. */
void sg_theme_init(lv_display_t *disp, const lv_font_t *font);

/* Swap the default text font, keeping all sg_theme customizations.
 * Intended to be called before the UI tree is built (see setDefaultFont). */
void sg_theme_set_font(lv_display_t *disp, const lv_font_t *font);

/* Replace the entire live token set with one of the built-in schemes
 * ("light" or "dark"), rebuild every shared style in place and trigger a
 * full repaint. All existing widgets pick up the new colours immediately. */
void sg_theme_set_scheme(lv_display_t *disp, const char *scheme);

/* Patch a single token in the live set by name (one of: primary, primary_dark,
 * on_primary, secondary, bg, surface, on_surface, on_variant, outline, track,
 * danger, warning), rebuild styles and repaint. Unknown names are no-ops. */
void sg_theme_set_token(lv_display_t *disp, const char *name, lv_color_t c);

#endif /* SG_THEME_H */
