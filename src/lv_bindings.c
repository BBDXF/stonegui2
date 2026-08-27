/**
 * lv_bindings.c
 *
 * QuickJS native module "lvgl" — exposes LVGL widget operations to JS.
 *
 * JS API (HostRenderer):
 *   createNode(type)            → opaque node handle (int ptr as JS int)
 *   appendChild(parent, child)
 *   removeChild(parent, child)
 *   setProperty(node, key, val)
 *   addEvent(node, event, cb)   → event name strings: "click","longpress"
 *   dispose(node)
 *   getScreen()                 → root screen node
 */

#include "lv_bindings.h"
#include "sg_theme.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "lvgl.h"
#include "quickjs-libc.h"
#include "src/libs/tiny_ttf/lv_tiny_ttf.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/widgets/calendar/lv_calendar_header_arrow.h"

#include <SDL.h>

/* ── input group (keyboard focus) ───────────────────────────────────────── */

static lv_group_t *g_group = NULL;

void lv_bindings_set_group(lv_group_t *group) {
    g_group = group;
}

/* ── IME candidate-window positioning ───────────────────────────────────────
 *
 * Tell SDL where the focused text field is so the OS IME places its candidate
 * (composition) window next to the caret instead of at the screen origin.
 * LVGL screen coordinates are scaled by the SDL window zoom to get pixels.
 */
static void sg_update_ime_rect(lv_obj_t *ta) {
    lv_area_t a;
    lv_obj_get_coords(ta, &a);

    uint8_t zoom = 1;
    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        uint8_t z = lv_sdl_window_get_zoom(disp);
        if (z) zoom = z;
    }

    SDL_Rect rect;
    rect.x = a.x1 * zoom;
    rect.y = a.y1 * zoom;
    rect.w = (a.x2 - a.x1 + 1) * zoom;
    rect.h = (a.y2 - a.y1 + 1) * zoom;
    SDL_SetTextInputRect(&rect);
}

static void sg_ime_event_cb(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    sg_update_ime_rect(ta);
}

/* ── helpers ────────────────────────────────────────────────────────────── */

static lv_obj_t *js_to_obj(JSContext *ctx, JSValueConst v) {
    int64_t ptr;
    JS_ToInt64(ctx, &ptr, v);
    return (lv_obj_t *)(uintptr_t)ptr;
}

static JSValue obj_to_js(JSContext *ctx, lv_obj_t *obj) {
    return JS_NewInt64(ctx, (int64_t)(uintptr_t)obj);
}

typedef struct sg_image_handle {
    char *path;   /* owned, e.g. "A:/abs/path.png". Never freed (catalog pattern). */
} sg_image_handle_t;

/* Resolve a JS argument (string OR int handle) into a path.
 * If *is_cstr is set to 1, caller MUST JS_FreeCString(ctx, returned).
 * If *is_cstr is set to 0, returned pointer is owned by a handle — DO NOT free.
 * Returns NULL on invalid input. */
static const char *resolve_image_src(JSContext *ctx, JSValueConst v, int *is_cstr) {
    *is_cstr = 0;
    if (JS_IsString(v)) {
        *is_cstr = 1;
        return JS_ToCString(ctx, v);
    }
    if (JS_IsNumber(v)) {
        int64_t h = 0;
        if (JS_ToInt64(ctx, &h, v) == 0 && h) {
            sg_image_handle_t *hdl = (sg_image_handle_t *)(uintptr_t)h;
            return hdl->path;
        }
    }
    return NULL;
}

/* colour string → { lv_color_t, lv_opa_t }
 *   "#rrggbb"       — opaque
 *   "#rrggbbaa"     — with alpha (00=transparent, ff=opaque)
 *   named colours   — black white red green blue yellow cyan magenta
 *                     orange purple pink gray/grey silver lime maroon
 *                     navy olive teal transparent
 */
typedef struct { lv_color_t color; lv_opa_t opa; } sg_color_t;

static sg_color_t parse_color_ex(const char *s) {
    sg_color_t out = { lv_color_black(), LV_OPA_COVER };
    if (!s) return out;

    if (s[0] == '#') {
        unsigned int r = 0, g = 0, b = 0, a = 255;
        size_t len = strlen(s + 1);
        if (len >= 8)
            sscanf(s + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
        else
            sscanf(s + 1, "%02x%02x%02x", &r, &g, &b);
        out.color = lv_color_make(r, g, b);
        out.opa   = (lv_opa_t)a;
        return out;
    }

    static const struct { const char *name; uint8_t r, g, b; lv_opa_t opa; } table[] = {
        { "transparent", 0,   0,   0,   LV_OPA_TRANSP },
        { "black",       0,   0,   0,   LV_OPA_COVER  },
        { "white",       255, 255, 255, LV_OPA_COVER  },
        { "red",         255, 0,   0,   LV_OPA_COVER  },
        { "green",       0,   128, 0,   LV_OPA_COVER  },
        { "blue",        0,   0,   255, LV_OPA_COVER  },
        { "yellow",      255, 255, 0,   LV_OPA_COVER  },
        { "cyan",        0,   255, 255, LV_OPA_COVER  },
        { "magenta",     255, 0,   255, LV_OPA_COVER  },
        { "orange",      255, 165, 0,   LV_OPA_COVER  },
        { "purple",      128, 0,   128, LV_OPA_COVER  },
        { "pink",        255, 192, 203, LV_OPA_COVER  },
        { "gray",        128, 128, 128, LV_OPA_COVER  },
        { "grey",        128, 128, 128, LV_OPA_COVER  },
        { "silver",      192, 192, 192, LV_OPA_COVER  },
        { "lime",        0,   255, 0,   LV_OPA_COVER  },
        { "maroon",      128, 0,   0,   LV_OPA_COVER  },
        { "navy",        0,   0,   128, LV_OPA_COVER  },
        { "olive",       128, 128, 0,   LV_OPA_COVER  },
        { "teal",        0,   128, 128, LV_OPA_COVER  },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(s, table[i].name) == 0) {
            out.color = lv_color_make(table[i].r, table[i].g, table[i].b);
            out.opa   = table[i].opa;
            return out;
        }
    }
    return out;
}

/* Convenience wrapper — colour only (alpha ignored) */
static lv_color_t parse_color(const char *s) {
    return parse_color_ex(s).color;
}

/* size value: number → px; "NN%" → lv_pct(NN); "fill" → 100%; "auto" → hug
 * content. An unrecognised string warns and falls back to "auto" — returning 0
 * silently collapsed the widget, which reads as "my element vanished". */
static int32_t parse_size(JSContext *ctx, JSValueConst v) {
    if (JS_IsString(v)) {
        const char *s = JS_ToCString(ctx, v);
        int32_t out = LV_SIZE_CONTENT;
        if (s) {
            int n = 0;
            if (strcmp(s, "fill") == 0) {
                out = lv_pct(100);
            } else if (strcmp(s, "auto") == 0) {
                out = LV_SIZE_CONTENT;
            } else if (sscanf(s, "%d", &n) == 1) {
                out = (strchr(s, '%') != NULL) ? lv_pct(n) : n;
            } else {
                fprintf(stderr, "stonegui: bad size '%s' (want a number, "
                                "\"NN%%\", \"fill\" or \"auto\"); using auto\n", s);
            }
        }
        JS_FreeCString(ctx, s);
        return out;
    }
    int32_t n; JS_ToInt32(ctx, &n, v);
    return n;
}

/* Parse "YYYY-MM-DD" or "YYYY-MM" → (y,m,d). Returns true on success.
 * d may be NULL (for "YYYY-MM"); on success without a day component, *d is 1. */
static bool parse_ymd(const char *s, uint32_t *y, uint32_t *m, uint32_t *d) {
    if (!s) return false;
    int yr = 0, mo = 0, dy = 1;
    int n = sscanf(s, "%d-%d-%d", &yr, &mo, &dy);
    if (n < 2 || yr <= 0 || mo <= 0) return false;
    if (y) *y = (uint32_t)yr;
    if (m) *m = (uint32_t)mo;
    if (d) *d = (uint32_t)dy;
    return true;
}

/* Buttonmatrix map storage
 *
 * lv_buttonmatrix_set_map() keeps the pointer; we must own the strings and the
 * pointer array for the widget's lifetime, then free them on LV_EVENT_DELETE.
 * LVGL terminates the map with an empty string "" (NOT NULL). "\n" entries
 * separate rows. We attach the storage via lv_obj_set_user_data so we can find
 * and free it later (buttonmatrix doesn't otherwise use user_data).
 */
typedef struct {
    char       **strs;   /* owned: n entries, each strdup'd                 */
    size_t       n;      /* number of map entries (excludes "" terminator)  */
    const char **map;    /* n+1 entries; map[n] = "" (LVGL terminator)      */
} btnmatrix_map_t;

static void btnmatrix_map_free(btnmatrix_map_t *m) {
    if (!m) return;
    if (m->strs) {
        for (size_t i = 0; i < m->n; i++) lv_free(m->strs[i]);
        lv_free(m->strs);
    }
    lv_free(m->map);
    lv_free(m);
}

static void btnmatrix_map_delete_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    btnmatrix_map_free((btnmatrix_map_t *)lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, NULL);
}

/* Line points storage — `lv_line_set_points` keeps the array pointer it is
 * given (no copy), so we must own the buffer for the widget's lifetime and
 * free it on LV_EVENT_DELETE. Same shape as btnmatrix_map_t above. */
typedef struct {
    lv_point_precise_t *points;
    uint32_t            count;
} sg_line_pts_t;

static void sg_line_pts_free(sg_line_pts_t *p) {
    if (!p) return;
    lv_free(p->points);
    lv_free(p);
}

static void sg_line_pts_delete_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    sg_line_pts_free((sg_line_pts_t *)lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, NULL);
}

typedef struct sg_animimg_ctx {
    const char **paths;   /* owned: array + each entry strdup'd */
    int count;
} sg_animimg_ctx_t;

static void sg_animimg_delete_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    sg_animimg_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (ctx) {
        for (int i = 0; i < ctx->count; i++)
            if (ctx->paths[i]) lv_free((void *)ctx->paths[i]);
        lv_free(ctx->paths);
        lv_free(ctx);
    }
}

typedef struct sg_imagebutton_ctx {
    char *srcs[LV_IMAGEBUTTON_STATE_NUM];   /* owned strdup per state (NULL if unset) */
} sg_imagebutton_ctx_t;

static void sg_imagebutton_delete_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    sg_imagebutton_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (ctx) {
        for (int i = 0; i < LV_IMAGEBUTTON_STATE_NUM; i++)
            if (ctx->srcs[i]) lv_free(ctx->srcs[i]);
        lv_free(ctx);
    }
}

/* Set/replace one state's src on an imagebutton, taking ownership via strdup. */
static void imagebutton_set_state(JSContext *ctx_js, lv_obj_t *obj,
                                   lv_imagebutton_state_t state,
                                   JSValueConst val) {
    sg_imagebutton_ctx_t *ictx = lv_obj_get_user_data(obj);
    if (!ictx) {
        ictx = lv_malloc(sizeof(sg_imagebutton_ctx_t));
        memset(ictx, 0, sizeof(sg_imagebutton_ctx_t));
        lv_obj_set_user_data(obj, ictx);
    }
    int is_cstr = 0;
    const char *p = resolve_image_src(ctx_js, val, &is_cstr);
    if (!p) return;

    char *owned = lv_strdup(p);
    if (is_cstr) JS_FreeCString(ctx_js, p);

    if (ictx->srcs[state]) lv_free(ictx->srcs[state]);
    ictx->srcs[state] = owned;
    lv_imagebutton_set_src(obj, state, owned, NULL, NULL);
}

/* ── event callback bridge ──────────────────────────────────────────────── */

typedef struct {
    JSContext *ctx;
    JSValue    fn;
} EventCb;

/* Read a widget's "current value" as a JS value, so event handlers can be
 * called as onChange(value) in the React/Vue style:
 *   Switch / Checkbox      → bool   (checked)
 *   Slider / Bar / Arc     → number (value)
 *   Dropdown / Roller      → number (selected index)
 *   Input (textarea)       → string (text)
 *   otherwise              → undefined
 */
static JSValue widget_value_to_js(JSContext *ctx, lv_obj_t *obj) {
    if (!obj) return JS_UNDEFINED;
    const lv_obj_class_t *cls = lv_obj_get_class(obj);

    if (cls == &lv_switch_class || cls == &lv_checkbox_class)
        return JS_NewBool(ctx, lv_obj_has_state(obj, LV_STATE_CHECKED));
    if (cls == &lv_slider_class)
        return JS_NewInt32(ctx, lv_slider_get_value(obj));
    if (cls == &lv_bar_class)
        return JS_NewInt32(ctx, lv_bar_get_value(obj));
    if (cls == &lv_arc_class)
        return JS_NewInt32(ctx, lv_arc_get_value(obj));
    if (cls == &lv_dropdown_class)
        return JS_NewInt32(ctx, (int32_t)lv_dropdown_get_selected(obj));
    if (cls == &lv_roller_class)
        return JS_NewInt32(ctx, (int32_t)lv_roller_get_selected(obj));
    if (cls == &lv_textarea_class)
        return JS_NewString(ctx, lv_textarea_get_text(obj));
    if (cls == &lv_spinbox_class)
        return JS_NewInt32(ctx, lv_spinbox_get_value(obj));
    if (cls == &lv_buttonmatrix_class)
        return JS_NewInt32(ctx, (int32_t)lv_buttonmatrix_get_selected_button(obj));
    if (cls == &lv_calendar_class) {
        lv_calendar_date_t d;
        if (lv_calendar_get_pressed_date(obj, &d) == LV_RESULT_OK) {
            JSValue o = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, o, "year",  JS_NewInt32(ctx, d.year));
            JS_SetPropertyStr(ctx, o, "month", JS_NewInt32(ctx, d.month));
            JS_SetPropertyStr(ctx, o, "day",   JS_NewInt32(ctx, d.day));
            return o;
        }
        return JS_UNDEFINED;
    }
    if (cls == &lv_tabview_class)
        return JS_NewInt32(ctx, (int32_t)lv_tabview_get_tab_active(obj));

    return JS_UNDEFINED;
}

static void lv_event_dispatch(lv_event_t *e) {
    EventCb *ecb = (EventCb *)lv_event_get_user_data(e);
    if (!ecb) return;
    lv_obj_t *target = lv_event_get_current_target_obj(e);
    JSValue arg = widget_value_to_js(ecb->ctx, target);
    JSValue ret = JS_Call(ecb->ctx, ecb->fn, JS_UNDEFINED, 1, &arg);
    if (JS_IsException(ret))
        js_std_dump_error(ecb->ctx);
    JS_FreeValue(ecb->ctx, ret);
    JS_FreeValue(ecb->ctx, arg);
}

/* Released when the owning widget is deleted — frees the JS handler + struct */
static void lv_event_free(lv_event_t *e) {
    EventCb *ecb = (EventCb *)lv_event_get_user_data(e);
    if (!ecb) return;
    JS_FreeValue(ecb->ctx, ecb->fn);
    lv_free(ecb);
}

/* Strip the default theme's decorations from a generic container so a "View"
 * behaves like a React-Native View: a transparent, borderless, zero-padding
 * layout box. User styles (backgroundColor, borderRadius, padding…) layer on
 * top of this clean slate. */

/* Extra drawing margin (px) a layout container reserves around itself so child
 * decorations that spill past the box (e.g. a slider/arc knob at the track's
 * ends) are not clipped. LV_OBJ_FLAG_OVERFLOW_VISIBLE only lifts the clip up to
 * the parent's own ext_draw_size, so a plain (ext_draw_size == 0) container
 * would still clip — we must contribute a non-zero size here. */
#define SG_CONTAINER_EXT_DRAW 16

static void sg_container_ext_draw_cb(lv_event_t *e) {
    /* lv_event_set_ext_draw_size() already keeps the running maximum. */
    lv_event_set_ext_draw_size(e, SG_CONTAINER_EXT_DRAW);
}

/* Spinbox is built on lv_textarea, and the focus group forwards every
 * keypress (digits, letters, …) to the textarea, which dutifully appends
 * the character — busting the digit format. Spinbox only consumes the
 * arrow keys itself. Block all character insertions on the underlying
 * textarea so the display stays inside the configured digit format; the
 * user steps the value with ←/→/↑/↓ (or by clicking the digits). */
static void sg_spinbox_block_insert_cb(lv_event_t *e) {
    lv_textarea_set_insert_replace(lv_event_get_target(e), "");
}

static void make_clean_container(lv_obj_t *obj) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_pad_row(obj, 0, 0);
    lv_obj_set_style_pad_column(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    /* Behave like CSS `overflow: visible` (the default for flex boxes): let
     * child decorations that extend past the box draw outside it instead of
     * being clipped. Without this, a slider/arc knob (which is larger than the
     * track and sits at the track's ends/edges) gets cut off at the start
     * position and along the top/bottom.
     *
     * The flag alone is not enough: LVGL only widens a child's clip area by the
     * *parent's* ext_draw_size, which is 0 for a plain transparent container.
     * So we also reserve a small ext_draw_size via the event below. */
    lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(obj, sg_container_ext_draw_cb,
                        LV_EVENT_REFR_EXT_DRAW_SIZE, NULL);
    lv_obj_refresh_ext_draw_size(obj);
}

/* ── module functions ───────────────────────────────────────────────────── */

/* Spinner defaults. The arc length must stay BELOW 360: lv_spinner animates
 * the start angle 0→360 and the end angle `angle`→360+angle in lockstep, so a
 * 360° arc is a closed ring whose rotation is invisible. Both are overridable
 * per instance (`arcAngle` at create time, `spinTime` any time). */
#define SG_SPINNER_ARC_ANGLE 270
#define SG_SPINNER_TIME_MS   1000

static JSValue js_getScreen(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    lv_obj_t *scr = lv_scr_act();
    /* Make the root reach the screen edges (theme adds padding by default) */
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return obj_to_js(ctx, scr);
}

/* getParent(node) / getChild(node, index) — read-only tree introspection,
 * null when absent. Composite widgets (msgbox) build sub-objects LVGL never
 * hands back, so this is the only way to reach a backdrop / header / footer. */
static JSValue js_getParent(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    lv_obj_t *parent = lv_obj_get_parent(js_to_obj(ctx, argv[0]));
    return parent ? obj_to_js(ctx, parent) : JS_NULL;
}

static JSValue js_getChild(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    int32_t index = 0;
    JS_ToInt32(ctx, &index, argv[1]);
    lv_obj_t *child = lv_obj_get_child(js_to_obj(ctx, argv[0]), index);
    return child ? obj_to_js(ctx, child) : JS_NULL;
}

static JSValue js_createThemeCoverageMenuInternals(JSContext *ctx, JSValueConst this_val,
                                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    lv_obj_t *page = js_to_obj(ctx, argv[0]);
    lv_obj_t *section = lv_menu_section_create(page);
    lv_menu_separator_create(page);
    lv_menu_cont_create(section);
    return obj_to_js(ctx, section);
}

static JSValue js_createNode(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;

    const char *type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_EXCEPTION;

    lv_obj_t *parent = lv_scr_act();
    lv_obj_t *obj    = NULL;

    if      (strcmp(type, "View")     == 0) { obj = lv_obj_create(parent); make_clean_container(obj); }
    else if (strcmp(type, "Text")     == 0) obj = lv_label_create(parent);
    else if (strcmp(type, "Button")   == 0) {
        obj = lv_button_create(parent);
        /* Every Button gets a child label by default */
        lv_obj_t *lbl = lv_label_create(obj);
        lv_label_set_text(lbl, "");
        lv_obj_center(lbl);
    }
    else if (strcmp(type, "Image")    == 0) obj = lv_image_create(parent);
    else if (strcmp(type, "AnimImg")  == 0) {
        obj = lv_animimg_create(parent);
        lv_obj_add_event_cb(obj, sg_animimg_delete_cb, LV_EVENT_DELETE, NULL);
    }
    else if (strcmp(type, "ImageButton") == 0) {
        obj = lv_imagebutton_create(parent);
        lv_obj_add_event_cb(obj, sg_imagebutton_delete_cb, LV_EVENT_DELETE, NULL);
    }
    else if (strcmp(type, "Input")    == 0) {
        obj = lv_textarea_create(parent);
        /* Keep the IME candidate window anchored to this field */
        lv_obj_add_event_cb(obj, sg_ime_event_cb, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(obj, sg_ime_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    else if (strcmp(type, "Switch")   == 0) obj = lv_switch_create(parent);
    else if (strcmp(type, "Progress") == 0) obj = lv_bar_create(parent);
    else if (strcmp(type, "Slider")   == 0) obj = lv_slider_create(parent);
    else if (strcmp(type, "Arc")      == 0) obj = lv_arc_create(parent);
    else if (strcmp(type, "Spinner")  == 0) {
        obj = lv_spinner_create(parent);
        int32_t arc_angle = SG_SPINNER_ARC_ANGLE;
        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue jaa = JS_GetPropertyStr(ctx, argv[1], "arcAngle");
            if (JS_IsNumber(jaa)) JS_ToInt32(ctx, &arc_angle, jaa);
            JS_FreeValue(ctx, jaa);
        }
        lv_spinner_set_anim_params(obj, SG_SPINNER_TIME_MS, (uint32_t)arc_angle);
        /* LVGL exposes no getter for the arc length, and set_anim_params takes
         * period and length together — stash the length so the `spinTime`
         * setter can change one without silently resetting the other. */
        lv_obj_set_user_data(obj, (void *)(uintptr_t)arc_angle);
    }
    else if (strcmp(type, "Checkbox") == 0) {
        obj = lv_checkbox_create(parent);
        lv_checkbox_set_text(obj, "");
    }
    else if (strcmp(type, "Dropdown") == 0) obj = lv_dropdown_create(parent);
    else if (strcmp(type, "Roller")   == 0)
        obj = lv_roller_create(parent);
    else if (strcmp(type, "Tabview")  == 0) {
        obj = lv_tabview_create(parent);
        /* Reasonable default tab bar height; can override via `tabBarSize`. */
        lv_tabview_set_tab_bar_size(obj, 44);
    }
    else if (strcmp(type, "List")     == 0) obj = lv_list_create(parent);
    else if (strcmp(type, "Spinbox")  == 0) {
        obj = lv_spinbox_create(parent);
        lv_spinbox_set_digit_format(obj, 5, 0);
        lv_spinbox_set_range(obj, -99999, 99999);
        lv_spinbox_set_step(obj, 1);
        lv_obj_add_event_cb(obj, sg_spinbox_block_insert_cb,
                            LV_EVENT_INSERT, NULL);
        /* Each step calls lv_textarea_set_cursor_pos, which scrolls the
         * cursor into view with LV_ANIM_ON, producing a visible jitter
         * on every increment. LV_ANIM_ON honours LV_PART_MAIN's
         * anim_duration; zeroing it disables the scroll animation while
         * leaving the cursor blink (LV_PART_CURSOR) intact. */
        lv_obj_set_style_anim_duration(obj, 0, LV_PART_MAIN);
    }
    else if (strcmp(type, "LED")      == 0) {
        obj = lv_led_create(parent);
        lv_led_on(obj);                /* default brightness = 255 (full on) */
    }
    else if (strcmp(type, "Chart")    == 0) {
        obj = lv_chart_create(parent);
    }
    else if (strcmp(type, "ButtonMatrix") == 0) {
        obj = lv_buttonmatrix_create(parent);
        lv_obj_add_event_cb(obj, btnmatrix_map_delete_cb, LV_EVENT_DELETE, NULL);
    }
    else if (strcmp(type, "Calendar") == 0) {
        obj = lv_calendar_create(parent);
        /* Built-in month-nav arrow header — most apps want it.
         * Pass `arrowHeader: false` from JS to opt out. */
        bool arrow = true;
        if (argc >= 2 && JS_IsObject(argv[1])) {
            JSValue jah = JS_GetPropertyStr(ctx, argv[1], "arrowHeader");
            if (!JS_IsUndefined(jah))
                arrow = JS_ToBool(ctx, jah);
            JS_FreeValue(ctx, jah);
        }
        if (arrow) lv_calendar_header_arrow_create(obj);
    }
    else if (strcmp(type, "Scale")    == 0) obj = lv_scale_create(parent);
    else if (strcmp(type, "Span")     == 0) obj = lv_spangroup_create(parent);
    else if (strcmp(type, "Line")     == 0) {
        obj = lv_line_create(parent);
        lv_obj_add_event_cb(obj, sg_line_pts_delete_cb, LV_EVENT_DELETE, NULL);
    }
    else if (strcmp(type, "Table")    == 0) obj = lv_table_create(parent);
    else if (strcmp(type, "Menu")     == 0) obj = lv_menu_create(parent);
    else if (strcmp(type, "Keyboard") == 0) obj = lv_keyboard_create(parent);
    else                                    { obj = lv_obj_create(parent); make_clean_container(obj); }

    /* Make interactive widgets reachable by the keyboard/encoder group */
    if (g_group && obj &&
        (strcmp(type, "Input")        == 0 ||
         strcmp(type, "Button")       == 0 ||
         strcmp(type, "Switch")       == 0 ||
         strcmp(type, "Slider")       == 0 ||
         strcmp(type, "Arc")          == 0 ||
         strcmp(type, "Checkbox")     == 0 ||
         strcmp(type, "Dropdown")     == 0 ||
         strcmp(type, "Roller")       == 0 ||
         strcmp(type, "Spinbox")      == 0 ||
         strcmp(type, "ButtonMatrix") == 0 ||
         strcmp(type, "Calendar")     == 0 ||
         strcmp(type, "Table")        == 0 ||
         strcmp(type, "Menu")         == 0 ||
         strcmp(type, "ImageButton")  == 0)) {
        lv_group_add_obj(g_group, obj);
    }

    JS_FreeCString(ctx, type);
    return obj_to_js(ctx, obj);
}

static JSValue js_appendChild(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val; (void)ctx;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t *parent = js_to_obj(ctx, argv[0]);
    lv_obj_t *child  = js_to_obj(ctx, argv[1]);
    lv_obj_set_parent(child, parent);
    return JS_UNDEFINED;
}

static JSValue js_removeChild(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val; (void)ctx;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t *child = js_to_obj(ctx, argv[1]);
    lv_obj_del(child);
    return JS_UNDEFINED;
}

static JSValue js_dispose(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)this_val; (void)ctx;
    if (argc < 1) return JS_EXCEPTION;
    lv_obj_del(js_to_obj(ctx, argv[0]));
    return JS_UNDEFINED;
}

/* updateLayout(node) — flush pending layout so the geometry getters
 * ("width"/"height"/"x"/"y"/"visible") read final coordinates. LVGL only
 * recomputes positions during lv_timer_handler, so anything that measures
 * right after mount would otherwise see every object still at 0,0. */
static JSValue js_updateLayout(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    lv_obj_update_layout(js_to_obj(ctx, argv[0]));
    return JS_UNDEFINED;
}

static int32_t parse_pseudo_state(const char *state) {
    if (strcmp(state, "default") == 0)  return LV_STATE_DEFAULT;
    if (strcmp(state, "hover") == 0)    return LV_STATE_HOVERED;
    if (strcmp(state, "focus") == 0)    return LV_STATE_FOCUSED;
    if (strcmp(state, "pressed") == 0)  return LV_STATE_PRESSED;
    if (strcmp(state, "checked") == 0)  return LV_STATE_CHECKED;
    if (strcmp(state, "disabled") == 0) return LV_STATE_DISABLED;
    return -1;
}

static int32_t parse_resolved_state(const char *state) {
    int32_t parsed = parse_pseudo_state(state);
    if (parsed >= 0) return parsed;
    if (strcmp(state, "focusKey") == 0) return LV_STATE_FOCUS_KEY;
    if (strcmp(state, "edited") == 0)   return LV_STATE_EDITED;
    if (strcmp(state, "scrolled") == 0) return LV_STATE_SCROLLED;
    return -1;
}

static int32_t parse_part(const char *part) {
    if (strcmp(part, "main") == 0)        return LV_PART_MAIN;
    if (strcmp(part, "scrollbar") == 0)   return LV_PART_SCROLLBAR;
    if (strcmp(part, "indicator") == 0)   return LV_PART_INDICATOR;
    if (strcmp(part, "knob") == 0)        return LV_PART_KNOB;
    if (strcmp(part, "selected") == 0)    return LV_PART_SELECTED;
    if (strcmp(part, "items") == 0)       return LV_PART_ITEMS;
    if (strcmp(part, "cursor") == 0)      return LV_PART_CURSOR;
    if (strcmp(part, "placeholder") == 0) return LV_PART_TEXTAREA_PLACEHOLDER;
    return -1;
}

static JSValue js_setProperty(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) return JS_EXCEPTION;

    lv_obj_t   *obj = js_to_obj(ctx, argv[0]);
    const char *key = JS_ToCString(ctx, argv[1]);
    if (!key) return JS_EXCEPTION;

    lv_style_selector_t selector = LV_PART_MAIN | LV_STATE_DEFAULT;
    int32_t part = LV_PART_MAIN;
    int32_t state = LV_STATE_DEFAULT;
    if (argc >= 4 && JS_IsString(argv[3])) {
        const char *state_string = JS_ToCString(ctx, argv[3]);
        if (!state_string) {
            JS_FreeCString(ctx, key);
            return JS_EXCEPTION;
        }
        state = parse_pseudo_state(state_string);
        JS_FreeCString(ctx, state_string);
        if (state < 0) {
            JS_FreeCString(ctx, key);
            return JS_ThrowTypeError(ctx, "setProperty: unknown state");
        }
    } else if (argc >= 4 && JS_IsObject(argv[3])) {
        JSValue part_value = JS_GetPropertyStr(ctx, argv[3], "part");
        JSValue state_value = JS_GetPropertyStr(ctx, argv[3], "state");
        if (JS_IsString(part_value)) {
            const char *part_string = JS_ToCString(ctx, part_value);
            if (part_string) {
                part = parse_part(part_string);
                JS_FreeCString(ctx, part_string);
            }
        }
        if (JS_IsString(state_value)) {
            const char *state_string = JS_ToCString(ctx, state_value);
            if (state_string) {
                state = parse_pseudo_state(state_string);
                JS_FreeCString(ctx, state_string);
            }
        }
        JS_FreeValue(ctx, part_value);
        JS_FreeValue(ctx, state_value);
        if (part < 0 || state < 0) {
            JS_FreeCString(ctx, key);
            return JS_ThrowTypeError(ctx, part < 0
                ? "setProperty: unknown part" : "setProperty: unknown state");
        }
    } else if (argc >= 4 && !JS_IsUndefined(argv[3])) {
        JS_FreeCString(ctx, key);
        return JS_ThrowTypeError(ctx, "setProperty: selector must be a state string or object");
    }
    selector = (lv_style_selector_t)(part | state);
    if ((part == LV_PART_SELECTED || part == LV_PART_SCROLLBAR) &&
        lv_obj_check_type(obj, &lv_dropdown_class)) {
        lv_obj_t *list = lv_dropdown_get_list(obj);
        if (list) obj = list;
    }

    /* ── layout / size ── */
    if (strcmp(key, "width") == 0) {
        lv_obj_set_width(obj, parse_size(ctx, argv[2]));
    } else if (strcmp(key, "height") == 0) {
        lv_obj_set_height(obj, parse_size(ctx, argv[2]));
    } else if (strcmp(key, "x") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_x(obj, v);
    } else if (strcmp(key, "y") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_y(obj, v);
    }
    /* ── flex layout ── */
    else if (strcmp(key, "flexFlow") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
        if (v && strcmp(v, "row") == 0)
            lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
        else
            lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "flexGrow") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_flex_grow(obj, (uint8_t)v);
    }
    /* ── flex alignment ── */
    else if (strcmp(key, "alignItems") == 0 ||
             strcmp(key, "justifyContent") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        lv_flex_align_t a = LV_FLEX_ALIGN_START;
        if (v) {
            if      (strcmp(v, "center")  == 0) a = LV_FLEX_ALIGN_CENTER;
            else if (strcmp(v, "end")     == 0) a = LV_FLEX_ALIGN_END;
            else if (strcmp(v, "between") == 0) a = LV_FLEX_ALIGN_SPACE_BETWEEN;
            else if (strcmp(v, "around")  == 0) a = LV_FLEX_ALIGN_SPACE_AROUND;
            else if (strcmp(v, "evenly")  == 0) a = LV_FLEX_ALIGN_SPACE_EVENLY;
            else                                a = LV_FLEX_ALIGN_START;
        }
        /* alignItems → cross axis, justifyContent → main axis (CSS naming) */
        if (strcmp(key, "alignItems") == 0)
            lv_obj_set_style_flex_cross_place(obj, a, selector);
        else
            lv_obj_set_style_flex_main_place(obj, a, selector);
        JS_FreeCString(ctx, v);
    }
    /* ── flex gap (CSS `gap`): spacing between children, both axes ── */
    else if (strcmp(key, "gap") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_style_pad_row(obj, v, selector);
        lv_obj_set_style_pad_column(obj, v, selector);
    }
    /* ── padding / margin ── */
    else if (strcmp(key, "padding") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_style_pad_all(obj, v, selector);
    } else if (strcmp(key, "margin") == 0) {
        /* LVGL uses padding on parent; approximate via translation */
        (void)0;
    }
    /* ── background ── */
    else if (strcmp(key, "backgroundColor") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        sg_color_t c = parse_color_ex(v);
        lv_obj_set_style_bg_color(obj, c.color, selector);
        lv_obj_set_style_bg_opa(obj, c.opa, selector);
        JS_FreeCString(ctx, v);
    }
    /* ── border / radius ── */
    else if (strcmp(key, "borderRadius") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_style_radius(obj, v, selector);
    } else if (strcmp(key, "borderWidth") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        lv_obj_set_style_border_width(obj, v, selector);
    } else if (strcmp(key, "borderColor") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        sg_color_t c = parse_color_ex(v);
        lv_obj_set_style_border_color(obj, c.color, selector);
        lv_obj_set_style_border_opa(obj, c.opa, selector);
        JS_FreeCString(ctx, v);
    }
    /* ── text / label ── */
    else if (strcmp(key, "text") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        /* Support both lv_label and lv_button (update inner label) */
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_label_class) {
            lv_label_set_text(obj, v ? v : "");
        } else if (cls == &lv_button_class) {
            lv_obj_t *lbl = lv_obj_get_child(obj, 0);
            if (lbl) lv_label_set_text(lbl, v ? v : "");
        } else if (cls == &lv_checkbox_class) {
            lv_checkbox_set_text(obj, v ? v : "");
        } else if (cls == &lv_textarea_class) {
            lv_textarea_set_text(obj, v ? v : "");
        }
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "textColor") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        sg_color_t c = parse_color_ex(v);
        lv_obj_set_style_text_color(obj, c.color, selector);
        lv_obj_set_style_text_opa(obj, c.opa, selector);
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "fontSize") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        /* Map to the nearest font compiled into lv_conf.h */
        const lv_font_t *f = &lv_font_montserrat_14;
        if      (v >= 24) f = &lv_font_montserrat_24;
        else if (v >= 20) f = &lv_font_montserrat_20;
        else if (v >= 16) f = &lv_font_montserrat_16;
        lv_obj_set_style_text_font(obj, f, selector);
    } else if (strcmp(key, "font") == 0) {
        /* int handle returned by loadFont() */
        int64_t h; JS_ToInt64(ctx, &h, argv[2]);
        if (h) lv_obj_set_style_text_font(obj, (const lv_font_t *)(uintptr_t)h, selector);
    } else if (strcmp(key, "scrollable") == 0) {
        if (JS_ToBool(ctx, argv[2])) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            /* Scrolling is meaningless without clipping. make_clean_container
             * gives every View OVERFLOW_VISIBLE (so slider/arc knobs at the
             * track ends survive); on a scroll container that flag widens the
             * child clip AND the hit-test box by ext_draw_size, leaving rows
             * scrolled out of view painted over — and clickable through —
             * whatever sits beside the container. */
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        } else {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            if (lv_obj_get_class(obj) == &lv_obj_class)
                lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        }
    }
    /* ── Image / AnimImg src ── */
    else if (strcmp(key, "src") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_image_class) {
            int is_cstr = 0;
            const char *p = resolve_image_src(ctx, argv[2], &is_cstr);
            if (p) lv_image_set_src(obj, p);
            if (is_cstr) JS_FreeCString(ctx, p);
        } else if (cls == &lv_animimg_class) {
            /* Array of (string|handle). LVGL stores the array pointer as-is
             * (lv_animimg_set_src does NOT copy the array). It WILL copy each
             * individual path via lv_image_set_src per frame. We own the
             * array and the inner strdup'd strings; free both in DELETE cb. */
            if (JS_IsArray(ctx, argv[2])) {
                JSValue jlen = JS_GetPropertyStr(ctx, argv[2], "length");
                uint32_t len = 0;
                JS_ToUint32(ctx, &len, jlen);
                JS_FreeValue(ctx, jlen);

                /* Free previous owned src if any */
                sg_animimg_ctx_t *old = lv_obj_get_user_data(obj);
                if (old) {
                    for (uint32_t i = 0; i < (uint32_t)old->count; i++)
                        if (old->paths[i]) lv_free((void *)old->paths[i]);
                    lv_free(old->paths);
                    lv_free(old);
                }

                sg_animimg_ctx_t *nctx = lv_malloc(sizeof(sg_animimg_ctx_t));
                nctx->count = (int)len;
                nctx->paths = lv_malloc(len * sizeof(const char *));
                for (uint32_t i = 0; i < len; i++) {
                    JSValue item = JS_GetPropertyUint32(ctx, argv[2], i);
                    int is_cstr = 0;
                    const char *p = resolve_image_src(ctx, item, &is_cstr);
                    nctx->paths[i] = p ? lv_strdup(p) : lv_strdup("");
                    if (is_cstr && p) JS_FreeCString(ctx, p);
                    JS_FreeValue(ctx, item);
                }
                lv_obj_set_user_data(obj, nctx);
                lv_animimg_set_src(obj, (const void **)nctx->paths, nctx->count);
            }
        }
    }
    /* ── Progress / bar / slider / arc / dropdown / roller ── */
    else if (strcmp(key, "value") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if      (cls == &lv_bar_class)      lv_bar_set_value(obj, v, LV_ANIM_OFF);
        else if (cls == &lv_slider_class)   lv_slider_set_value(obj, v, LV_ANIM_OFF);
        else if (cls == &lv_arc_class)      lv_arc_set_value(obj, v);
        else if (cls == &lv_dropdown_class) lv_dropdown_set_selected(obj, (uint32_t)v);
        else if (cls == &lv_roller_class)   lv_roller_set_selected(obj, (uint32_t)v, LV_ANIM_OFF);
        else if (cls == &lv_spinbox_class)  lv_spinbox_set_value(obj, v);
    } else if (strcmp(key, "min") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_bar_class)
            lv_bar_set_range(obj, v, lv_bar_get_max_value(obj));
        else if (cls == &lv_slider_class)
            lv_slider_set_range(obj, v, lv_slider_get_max_value(obj));
        else if (cls == &lv_arc_class)
            lv_arc_set_range(obj, v, lv_arc_get_max_value(obj));
    } else if (strcmp(key, "max") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_bar_class)
            lv_bar_set_range(obj, lv_bar_get_min_value(obj), v);
        else if (cls == &lv_slider_class)
            lv_slider_set_range(obj, lv_slider_get_min_value(obj), v);
        else if (cls == &lv_arc_class)
            lv_arc_set_range(obj, lv_arc_get_min_value(obj), v);
    }
    /* ── Dropdown / Roller options ("a\nb\nc") ── */
    else if (strcmp(key, "options") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (v) {
            if (cls == &lv_dropdown_class)
                lv_dropdown_set_options(obj, v);
            else if (cls == &lv_roller_class)
                lv_roller_set_options(obj, v, LV_ROLLER_MODE_NORMAL);
        }
        JS_FreeCString(ctx, v);
    }
    /* ── Switch / Checkbox ── */
    else if (strcmp(key, "checked") == 0) {
        int v = JS_ToBool(ctx, argv[2]);
        if (v) lv_obj_add_state(obj, LV_STATE_CHECKED);
        else   lv_obj_remove_state(obj, LV_STATE_CHECKED);
    }
    /* ── Input / textarea ── */
    else if (strcmp(key, "placeholder") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_textarea_class)
            lv_textarea_set_placeholder_text(obj, v ? v : "");
        JS_FreeCString(ctx, v);
    }
    else if (strcmp(key, "oneLine") == 0) {
        if (lv_obj_get_class(obj) == &lv_textarea_class)
            lv_textarea_set_one_line(obj, JS_ToBool(ctx, argv[2]));
    }
    else if (strcmp(key, "maxLength") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_textarea_class)
            lv_textarea_set_max_length(obj, (uint32_t)v);
    }
    else if (strcmp(key, "acceptedChars") == 0) {
        const char *s = JS_ToCString(ctx, argv[2]);
        if (lv_obj_get_class(obj) == &lv_textarea_class)
            lv_textarea_set_accepted_chars(obj, s);
        JS_FreeCString(ctx, s);
    }
    else if (strcmp(key, "password") == 0) {
        if (lv_obj_get_class(obj) == &lv_textarea_class)
            lv_textarea_set_password_mode(obj, JS_ToBool(ctx, argv[2]));
    }
    else if (strcmp(key, "align") == 0) {
        const char *s = JS_ToCString(ctx, argv[2]);
        if (s && lv_obj_get_class(obj) == &lv_textarea_class) {
            lv_text_align_t a = LV_TEXT_ALIGN_LEFT;
            if      (strcmp(s, "center") == 0) a = LV_TEXT_ALIGN_CENTER;
            else if (strcmp(s, "right")  == 0) a = LV_TEXT_ALIGN_RIGHT;
            lv_textarea_set_align(obj, a);
        }
        JS_FreeCString(ctx, s);
    }
    else if (strcmp(key, "textSelection") == 0) {
        if (lv_obj_get_class(obj) == &lv_textarea_class)
            lv_textarea_set_text_selection(obj, JS_ToBool(ctx, argv[2]));
    }
    else if (strcmp(key, "cursorPos") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_textarea_class)
            lv_textarea_set_cursor_pos(obj, (int32_t)v);
    }
    /* ── Spinner: spinTime (ms per revolution). The arc length rides along in
     * user_data because lv_spinner_set_anim_params() takes both at once. */
    else if (strcmp(key, "spinTime") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_spinner_class) {
            int32_t v; JS_ToInt32(ctx, &v, argv[2]);
            if (v <= 0) v = SG_SPINNER_TIME_MS;
            uint32_t angle = (uint32_t)(uintptr_t)lv_obj_get_user_data(obj);
            lv_spinner_set_anim_params(obj, (uint32_t)v,
                                       angle ? angle : SG_SPINNER_ARC_ANGLE);
        }
    }
    /* ── Spinbox ── */
    else if (strcmp(key, "digits") == 0) {
        /* "N" or "N.M" → digit_count=N, sep_pos=M (0 if no dot). */
        const char *v = JS_ToCString(ctx, argv[2]);
        if (v && lv_obj_get_class(obj) == &lv_spinbox_class) {
            int dc = 0, sp = 0;
            sscanf(v, "%d.%d", &dc, &sp);
            if (dc <= 0) sscanf(v, "%d", &dc);
            if (dc > 0) lv_spinbox_set_digit_format(obj, (uint32_t)dc, (uint32_t)sp);
        }
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "step") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_spinbox_class && v > 0)
            lv_spinbox_set_step(obj, (uint32_t)v);
    }
    /* ── LED ── */
    else if (strcmp(key, "color") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        if (v && lv_obj_get_class(obj) == &lv_led_class)
            lv_led_set_color(obj, parse_color(v));
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "brightness") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_led_class) {
            if (v < 0) v = 0; if (v > 255) v = 255;
            lv_led_set_brightness(obj, (uint8_t)v);
        }
    }
    /* ── Chart ── */
    else if (strcmp(key, "chartType") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        if (v && lv_obj_get_class(obj) == &lv_chart_class) {
            lv_chart_type_t t = LV_CHART_TYPE_LINE;
            if      (strcmp(v, "bar")     == 0) t = LV_CHART_TYPE_BAR;
            else if (strcmp(v, "scatter") == 0) t = LV_CHART_TYPE_SCATTER;
            else if (strcmp(v, "none")    == 0) t = LV_CHART_TYPE_NONE;
            lv_chart_set_type(obj, t);
        }
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "pointCount") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_chart_class && v > 0)
            lv_chart_set_point_count(obj, (uint32_t)v);
    } else if (strcmp(key, "rangeMin") == 0 || strcmp(key, "rangeMax") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_chart_class) {
            int32_t lo = 0, hi = 100;
            if (strcmp(key, "rangeMin") == 0) { lo = v; hi = v + 100; }
            else                              { lo = 0; hi = v; }
            lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
        }
    } else if (strcmp(key, "divLines") == 0) {
        /* "HxV" — e.g. "5x10" → 5 horizontal, 10 vertical division lines. */
        const char *v = JS_ToCString(ctx, argv[2]);
        if (v && lv_obj_get_class(obj) == &lv_chart_class) {
            int h = 0, w = 0;
            sscanf(v, "%dx%d", &h, &w);
            lv_chart_set_div_line_count(obj, (uint8_t)h, (uint8_t)w);
        }
        JS_FreeCString(ctx, v);
    }
    /* ── ButtonMatrix: map = JS array of strings; "\n" entries start new row ── */
    else if (strcmp(key, "map") == 0) {
        if (lv_obj_get_class(obj) == &lv_buttonmatrix_class &&
            JS_IsArray(ctx, argv[2])) {
            uint32_t len = 0;
            JSValue jlen = JS_GetPropertyStr(ctx, argv[2], "length");
            JS_ToUint32(ctx, &len, jlen);
            JS_FreeValue(ctx, jlen);

            btnmatrix_map_t *m = lv_malloc(sizeof(*m));
            m->n    = (size_t)len;
            m->strs = lv_malloc(sizeof(char *) * (len + 1));
            m->map  = lv_malloc(sizeof(const char *) * (len + 1));
            for (uint32_t i = 0; i < len; i++) {
                JSValue it = JS_GetPropertyUint32(ctx, argv[2], i);
                const char *s = JS_ToCString(ctx, it);
                m->strs[i] = s ? lv_strdup(s) : lv_strdup("");
                m->map[i]  = m->strs[i];
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, it);
            }
            m->map[len] = "";                          /* LVGL terminator */

            btnmatrix_map_free((btnmatrix_map_t *)lv_obj_get_user_data(obj));
            lv_obj_set_user_data(obj, m);
            lv_buttonmatrix_set_map(obj, m->map);
        }
    } else if (strcmp(key, "oneChecked") == 0) {
        if (lv_obj_get_class(obj) == &lv_buttonmatrix_class)
            lv_buttonmatrix_set_one_checked(obj, JS_ToBool(ctx, argv[2]));
    }
    /* ── Calendar: today / shown as "YYYY-MM-DD" / "YYYY-MM" strings ── */
    else if (strcmp(key, "today") == 0 || strcmp(key, "shown") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        uint32_t y = 0, mo = 0, d = 1;
        if (v && lv_obj_get_class(obj) == &lv_calendar_class && parse_ymd(v, &y, &mo, &d)) {
            if (strcmp(key, "today") == 0) lv_calendar_set_today_date(obj, y, mo, d);
            else                           lv_calendar_set_showed_date(obj, y, mo);
        }
        JS_FreeCString(ctx, v);
    }
    /* ── Scale ── */
    else if (strcmp(key, "scaleMode") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        if (v && lv_obj_get_class(obj) == &lv_scale_class) {
            lv_scale_mode_t m = LV_SCALE_MODE_HORIZONTAL_BOTTOM;
            if      (strcmp(v, "h-top")       == 0) m = LV_SCALE_MODE_HORIZONTAL_TOP;
            else if (strcmp(v, "h-bottom")    == 0) m = LV_SCALE_MODE_HORIZONTAL_BOTTOM;
            else if (strcmp(v, "v-left")      == 0) m = LV_SCALE_MODE_VERTICAL_LEFT;
            else if (strcmp(v, "v-right")     == 0) m = LV_SCALE_MODE_VERTICAL_RIGHT;
            else if (strcmp(v, "round-inner") == 0) m = LV_SCALE_MODE_ROUND_INNER;
            else if (strcmp(v, "round-outer") == 0) m = LV_SCALE_MODE_ROUND_OUTER;
            lv_scale_set_mode(obj, m);
        }
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "totalTicks") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_scale_class && v > 0)
            lv_scale_set_total_tick_count(obj, (uint32_t)v);
    } else if (strcmp(key, "majorEvery") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_scale_class && v > 0)
            lv_scale_set_major_tick_every(obj, (uint32_t)v);
    } else if (strcmp(key, "showLabels") == 0) {
        if (lv_obj_get_class(obj) == &lv_scale_class)
            lv_scale_set_label_show(obj, JS_ToBool(ctx, argv[2]));
    }
    /* ── Spangroup: parts = [{text, color?, fontSize?}, ...] ── */
    else if (strcmp(key, "parts") == 0) {
        if (lv_obj_get_class(obj) == &lv_spangroup_class &&
            JS_IsArray(ctx, argv[2])) {
            while (lv_spangroup_get_span_count(obj) > 0) {
                lv_span_t *sp = lv_spangroup_get_child(obj, 0);
                if (!sp) break;
                lv_spangroup_delete_span(obj, sp);
            }
            uint32_t len = 0;
            JSValue jlen = JS_GetPropertyStr(ctx, argv[2], "length");
            JS_ToUint32(ctx, &len, jlen);
            JS_FreeValue(ctx, jlen);
            for (uint32_t i = 0; i < len; i++) {
                JSValue it    = JS_GetPropertyUint32(ctx, argv[2], i);
                JSValue jtext = JS_GetPropertyStr(ctx, it, "text");
                JSValue jcol  = JS_GetPropertyStr(ctx, it, "color");
                JSValue jfs   = JS_GetPropertyStr(ctx, it, "fontSize");
                JSValue jfont = JS_GetPropertyStr(ctx, it, "font");

                const char *t = JS_ToCString(ctx, jtext);
                lv_span_t *sp = lv_spangroup_new_span(obj);
                lv_span_set_text(sp, t ? t : "");
                JS_FreeCString(ctx, t);

                if (!JS_IsUndefined(jcol)) {
                    const char *cs = JS_ToCString(ctx, jcol);
                    if (cs) {
                        sg_color_t c = parse_color_ex(cs);
                        lv_style_set_text_color(lv_span_get_style(sp), c.color);
                        lv_style_set_text_opa(lv_span_get_style(sp), c.opa);
                    }
                    JS_FreeCString(ctx, cs);
                }
                if (JS_IsNumber(jfs)) {
                    int32_t fs; JS_ToInt32(ctx, &fs, jfs);
                    const lv_font_t *f = &lv_font_montserrat_14;
                    if      (fs >= 24) f = &lv_font_montserrat_24;
                    else if (fs >= 20) f = &lv_font_montserrat_20;
                    else if (fs >= 16) f = &lv_font_montserrat_16;
                    lv_style_set_text_font(lv_span_get_style(sp), f);
                }
                if (!JS_IsUndefined(jfont)) {
                    int64_t h; JS_ToInt64(ctx, &h, jfont);
                    if (h) lv_style_set_text_font(lv_span_get_style(sp),
                                                  (const lv_font_t *)(uintptr_t)h);
                }

                JS_FreeValue(ctx, jtext);
                JS_FreeValue(ctx, jcol);
                JS_FreeValue(ctx, jfs);
                JS_FreeValue(ctx, jfont);
                JS_FreeValue(ctx, it);
            }
            lv_spangroup_refr_mode(obj);
        }
    }
    /* ── Tabview ── */
    else if (strcmp(key, "tabBarSize") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_tabview_class)
            lv_tabview_set_tab_bar_size(obj, v);
    } else if (strcmp(key, "tabBarPosition") == 0) {
        const char *v = JS_ToCString(ctx, argv[2]);
        if (v && lv_obj_get_class(obj) == &lv_tabview_class) {
            lv_dir_t d = LV_DIR_TOP;
            if      (strcmp(v, "bottom") == 0) d = LV_DIR_BOTTOM;
            else if (strcmp(v, "left")   == 0) d = LV_DIR_LEFT;
            else if (strcmp(v, "right")  == 0) d = LV_DIR_RIGHT;
            lv_tabview_set_tab_bar_position(obj, d);
        }
        JS_FreeCString(ctx, v);
    } else if (strcmp(key, "activeTab") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_tabview_class && v >= 0)
            lv_tabview_set_active(obj, (uint32_t)v, LV_ANIM_OFF);
    }
    /* ── Line: points = [[x, y], [x, y], ...] ── */
    else if (strcmp(key, "points") == 0) {
        if (lv_obj_get_class(obj) == &lv_line_class && JS_IsArray(ctx, argv[2])) {
            uint32_t len = 0;
            JSValue jlen = JS_GetPropertyStr(ctx, argv[2], "length");
            JS_ToUint32(ctx, &len, jlen);
            JS_FreeValue(ctx, jlen);

            sg_line_pts_t *p = lv_malloc(sizeof(*p));
            p->count  = len;
            p->points = lv_malloc(sizeof(lv_point_precise_t) * (len > 0 ? len : 1));
            for (uint32_t i = 0; i < len; i++) {
                JSValue pt = JS_GetPropertyUint32(ctx, argv[2], i);
                int32_t x = 0, y = 0;
                if (JS_IsArray(ctx, pt)) {
                    JSValue jx = JS_GetPropertyUint32(ctx, pt, 0);
                    JSValue jy = JS_GetPropertyUint32(ctx, pt, 1);
                    JS_ToInt32(ctx, &x, jx);
                    JS_ToInt32(ctx, &y, jy);
                    JS_FreeValue(ctx, jx);
                    JS_FreeValue(ctx, jy);
                }
                p->points[i].x = x;
                p->points[i].y = y;
                JS_FreeValue(ctx, pt);
            }
            sg_line_pts_free((sg_line_pts_t *)lv_obj_get_user_data(obj));
            lv_obj_set_user_data(obj, p);
            lv_line_set_points(obj, p->points, p->count);
        }
    }
    /* ── Table: rows, cols, cells (2D string array) ── */
    else if (strcmp(key, "rows") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_table_class && v > 0)
            lv_table_set_row_count(obj, (uint32_t)v);
    } else if (strcmp(key, "cols") == 0) {
        int32_t v; JS_ToInt32(ctx, &v, argv[2]);
        if (lv_obj_get_class(obj) == &lv_table_class && v > 0)
            lv_table_set_column_count(obj, (uint32_t)v);
    } else if (strcmp(key, "cells") == 0) {
        if (lv_obj_get_class(obj) == &lv_table_class && JS_IsArray(ctx, argv[2])) {
            uint32_t rows = 0;
            JSValue jlen = JS_GetPropertyStr(ctx, argv[2], "length");
            JS_ToUint32(ctx, &rows, jlen);
            JS_FreeValue(ctx, jlen);

            uint32_t cols = 0;
            for (uint32_t r = 0; r < rows; r++) {
                JSValue row = JS_GetPropertyUint32(ctx, argv[2], r);
                if (JS_IsArray(ctx, row)) {
                    uint32_t rc = 0;
                    JSValue rlen = JS_GetPropertyStr(ctx, row, "length");
                    JS_ToUint32(ctx, &rc, rlen);
                    JS_FreeValue(ctx, rlen);
                    if (rc > cols) cols = rc;
                }
                JS_FreeValue(ctx, row);
            }
            if (rows > 0) lv_table_set_row_count(obj, rows);
            if (cols > 0) lv_table_set_column_count(obj, cols);

            for (uint32_t r = 0; r < rows; r++) {
                JSValue row = JS_GetPropertyUint32(ctx, argv[2], r);
                if (JS_IsArray(ctx, row)) {
                    uint32_t rc = 0;
                    JSValue rlen = JS_GetPropertyStr(ctx, row, "length");
                    JS_ToUint32(ctx, &rc, rlen);
                    JS_FreeValue(ctx, rlen);
                    for (uint32_t c = 0; c < rc; c++) {
                        JSValue cell = JS_GetPropertyUint32(ctx, row, c);
                        const char *s = JS_ToCString(ctx, cell);
                        if (s) lv_table_set_cell_value(obj, r, c, s);
                        JS_FreeCString(ctx, s);
                        JS_FreeValue(ctx, cell);
                    }
                }
                JS_FreeValue(ctx, row);
            }
        }
    }
    /* ── Keyboard (on-screen) ── */
    else if (strcmp(key, "target") == 0) {
        if (lv_obj_get_class(obj) == &lv_keyboard_class) {
            lv_obj_t *ta = js_to_obj(ctx, argv[2]);
            lv_keyboard_set_textarea(obj, ta);
        }
    } else if (strcmp(key, "mode") == 0) {
        if (lv_obj_get_class(obj) == &lv_keyboard_class) {
            const char *v = JS_ToCString(ctx, argv[2]);
            if (v) {
                lv_keyboard_mode_t m = LV_KEYBOARD_MODE_TEXT_LOWER;
                if      (strcmp(v, "text-upper") == 0) m = LV_KEYBOARD_MODE_TEXT_UPPER;
                else if (strcmp(v, "number")     == 0) m = LV_KEYBOARD_MODE_NUMBER;
                else if (strcmp(v, "special")    == 0) m = LV_KEYBOARD_MODE_SPECIAL;
                lv_keyboard_set_mode(obj, m);
                JS_FreeCString(ctx, v);
            }
        }
    }
    /* ── AnimImg props (other than `src`) ── */
    else if (strcmp(key, "duration") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_animimg_class) {
            int32_t v; JS_ToInt32(ctx, &v, argv[2]);
            lv_animimg_set_duration(obj, (uint32_t)v);
        }
    }
    else if (strcmp(key, "repeat") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_animimg_class) {
            if (JS_IsString(argv[2])) {
                const char *s = JS_ToCString(ctx, argv[2]);
                if (s && strcmp(s, "infinite") == 0)
                    lv_animimg_set_repeat_count(obj, LV_ANIM_REPEAT_INFINITE);
                JS_FreeCString(ctx, s);
            } else {
                int32_t v; JS_ToInt32(ctx, &v, argv[2]);
                lv_animimg_set_repeat_count(obj, (uint32_t)v);
            }
        }
    }
    else if (strcmp(key, "start") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_animimg_class && JS_ToBool(ctx, argv[2]))
            lv_animimg_start(obj);
    }

    /* ── ImageButton state srcs ── */
    else if (strcmp(key, "released") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class)
            imagebutton_set_state(ctx, obj, LV_IMAGEBUTTON_STATE_RELEASED, argv[2]);
    }
    else if (strcmp(key, "pressed") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class)
            imagebutton_set_state(ctx, obj, LV_IMAGEBUTTON_STATE_PRESSED, argv[2]);
    }
    else if (strcmp(key, "disabled") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class)
            imagebutton_set_state(ctx, obj, LV_IMAGEBUTTON_STATE_DISABLED, argv[2]);
    }
    else if (strcmp(key, "checkedReleased") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class)
            imagebutton_set_state(ctx, obj, LV_IMAGEBUTTON_STATE_CHECKED_RELEASED, argv[2]);
    }
    else if (strcmp(key, "checkedPressed") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class)
            imagebutton_set_state(ctx, obj, LV_IMAGEBUTTON_STATE_CHECKED_PRESSED, argv[2]);
    }
    else if (strcmp(key, "checkedDisabled") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class)
            imagebutton_set_state(ctx, obj, LV_IMAGEBUTTON_STATE_CHECKED_DISABLED, argv[2]);
    }
    else if (strcmp(key, "checkable") == 0) {
        if (lv_obj_get_class(obj) == &lv_imagebutton_class) {
            if (JS_ToBool(ctx, argv[2]))
                lv_obj_add_flag(obj, LV_OBJ_FLAG_CHECKABLE);
            else
                lv_obj_remove_flag(obj, LV_OBJ_FLAG_CHECKABLE);
        }
    }

    JS_FreeCString(ctx, key);
    return JS_UNDEFINED;
}

static JSValue style_color_to_js(JSContext *ctx, lv_color_t color) {
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", color.red, color.green, color.blue);
    return JS_NewString(ctx, hex);
}

/* getProperty(node, key, selector?) — read back common widget state.
 *   "value"   → number (slider/bar/arc) or selected index (dropdown/roller),
 *               or bool (switch/checkbox), or string (input)
 *   "checked" → bool   (switch/checkbox)
 *   "text"    → string (input/label)
 *   "cursorPos" → number (input)
 *   "target"  → node handle of the textarea a Keyboard is attached to
 * Resolved style keys use selector.part | selector.state. Padding represents
 * the resolved top side (lv_obj_get_style_pad_top).
 */
static JSValue js_getProperty(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;

    lv_obj_t   *obj = js_to_obj(ctx, argv[0]);
    const char *key = JS_ToCString(ctx, argv[1]);
    if (!key) return JS_EXCEPTION;

    lv_style_selector_t selector = LV_PART_MAIN | LV_STATE_DEFAULT;
    if (argc >= 3 && JS_IsObject(argv[2])) {
        int32_t part = LV_PART_MAIN;
        int32_t state = LV_STATE_DEFAULT;
        JSValue part_value = JS_GetPropertyStr(ctx, argv[2], "part");
        JSValue state_value = JS_GetPropertyStr(ctx, argv[2], "state");
        bool part_type_valid = JS_IsUndefined(part_value) || JS_IsString(part_value);
        bool state_type_valid = JS_IsUndefined(state_value) || JS_IsString(state_value);
        if (JS_IsString(part_value)) {
            const char *part_string = JS_ToCString(ctx, part_value);
            if (part_string) {
                part = parse_part(part_string);
                JS_FreeCString(ctx, part_string);
            }
        }
        if (JS_IsString(state_value)) {
            const char *state_string = JS_ToCString(ctx, state_value);
            if (state_string) {
                state = parse_resolved_state(state_string);
                JS_FreeCString(ctx, state_string);
            }
        }
        JS_FreeValue(ctx, part_value);
        JS_FreeValue(ctx, state_value);
        if (!part_type_valid || !state_type_valid) {
            JS_FreeCString(ctx, key);
            return JS_ThrowTypeError(ctx, !part_type_valid
                ? "getProperty: part must be a string"
                : "getProperty: state must be a string");
        }
        if (part < 0 || state < 0) {
            JS_FreeCString(ctx, key);
            return JS_ThrowTypeError(ctx, part < 0
                ? "getProperty: unknown part" : "getProperty: unknown state");
        }
        selector = (lv_style_selector_t)(part | state);

        /* LVGL renders a dropdown's SELECTED row and its popup SCROLLBAR on a
         * separate lv_dropdownlist object (lv_dropdown.c draw_box/draw_box_label
         * read them from `dropdown->list`), never on the dropdown itself. Those
         * two parts are therefore meaningless on the dropdown, so resolve them
         * where they are actually drawn. */
        if ((part == LV_PART_SELECTED || part == LV_PART_SCROLLBAR) &&
            lv_obj_check_type(obj, &lv_dropdown_class)) {
            lv_obj_t *list = lv_dropdown_get_list(obj);
            if (list) obj = list;
        }
    }

    JSValue out;
    if (strcmp(key, "checked") == 0) {
        out = JS_NewBool(ctx, lv_obj_has_state(obj, LV_STATE_CHECKED));
    } else if (strcmp(key, "text") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_textarea_class)
            out = JS_NewString(ctx, lv_textarea_get_text(obj));
        else if (cls == &lv_label_class)
            out = JS_NewString(ctx, lv_label_get_text(obj));
        else
            out = JS_UNDEFINED;
    } else if (strcmp(key, "cursorPos") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_textarea_class)
            out = JS_NewUint32(ctx, lv_textarea_get_cursor_pos(obj));
        else
            out = JS_UNDEFINED;
    } else if (strcmp(key, "target") == 0) {
        const lv_obj_class_t *cls = lv_obj_get_class(obj);
        if (cls == &lv_keyboard_class)
            out = obj_to_js(ctx, lv_keyboard_get_textarea(obj));
        else
            out = JS_UNDEFINED;
    } else if (strcmp(key, "backgroundColor") == 0) {
        out = style_color_to_js(ctx, lv_obj_get_style_bg_color(obj, selector));
    } else if (strcmp(key, "textColor") == 0) {
        out = style_color_to_js(ctx, lv_obj_get_style_text_color(obj, selector));
    } else if (strcmp(key, "borderColor") == 0) {
        out = style_color_to_js(ctx, lv_obj_get_style_border_color(obj, selector));
    } else if (strcmp(key, "outlineColor") == 0) {
        out = style_color_to_js(ctx, lv_obj_get_style_outline_color(obj, selector));
    } else if (strcmp(key, "lineColor") == 0) {
        out = style_color_to_js(ctx, lv_obj_get_style_line_color(obj, selector));
    } else if (strcmp(key, "arcColor") == 0) {
        out = style_color_to_js(ctx, lv_obj_get_style_arc_color(obj, selector));
    } else if (strcmp(key, "borderWidth") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_border_width(obj, selector));
    } else if (strcmp(key, "outlineWidth") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_outline_width(obj, selector));
    } else if (strcmp(key, "radius") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_radius(obj, selector));
    } else if (strcmp(key, "padding") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_pad_top(obj, selector));
    } else if (strcmp(key, "shadowWidth") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_shadow_width(obj, selector));
    } else if (strcmp(key, "shadowOpa") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_shadow_opa(obj, selector));
    } else if (strcmp(key, "bgOpa") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_bg_opa(obj, selector));
    } else if (strcmp(key, "imageOpa") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_image_opa(obj, selector));
    } else if (strcmp(key, "textOpa") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_text_opa(obj, selector));
    } else if (strcmp(key, "borderOpa") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_border_opa(obj, selector));
    } else if (strcmp(key, "lineWidth") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_line_width(obj, selector));
    } else if (strcmp(key, "arcWidth") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_style_arc_width(obj, selector));
    } else if (strcmp(key, "fontLineHeight") == 0) {
        const lv_font_t *font = lv_obj_get_style_text_font(obj, selector);
        out = JS_NewInt32(ctx, lv_font_get_line_height(font));
    } else if (strcmp(key, "scrollable") == 0) {
        out = JS_NewBool(ctx, lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE));
    } else if (strcmp(key, "visible") == 0) {
        out = JS_NewBool(ctx, lv_obj_is_visible(obj));
    } else if (strcmp(key, "width") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_width(obj));
    } else if (strcmp(key, "height") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_height(obj));
    } else if (strcmp(key, "x") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_x(obj));
    } else if (strcmp(key, "y") == 0) {
        out = JS_NewInt32(ctx, lv_obj_get_y(obj));
    } else if (strcmp(key, "value") == 0) {
        out = widget_value_to_js(ctx, obj);
    } else {
        JS_FreeCString(ctx, key);
        return JS_ThrowTypeError(ctx, "getProperty: unknown key");
    }

    JS_FreeCString(ctx, key);
    return out;
}

/* addEvent(node, eventName, callback) */
static JSValue js_addEvent(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) return JS_EXCEPTION;

    lv_obj_t   *obj  = js_to_obj(ctx, argv[0]);
    const char *name = JS_ToCString(ctx, argv[1]);
    if (!name) return JS_EXCEPTION;

    lv_event_code_t code = LV_EVENT_CLICKED;
    if      (strcmp(name, "click")     == 0) code = LV_EVENT_CLICKED;
    else if (strcmp(name, "longpress") == 0) code = LV_EVENT_LONG_PRESSED;
    else if (strcmp(name, "change")    == 0) code = LV_EVENT_VALUE_CHANGED;
    else if (strcmp(name, "focus")     == 0) code = LV_EVENT_FOCUSED;
    else if (strcmp(name, "blur")      == 0) code = LV_EVENT_DEFOCUSED;

    JS_FreeCString(ctx, name);

    EventCb *ecb = lv_malloc(sizeof(EventCb));
    ecb->ctx = ctx;
    ecb->fn  = JS_DupValue(ctx, argv[2]);

    lv_obj_add_event_cb(obj, lv_event_dispatch, code, ecb);
    /* Free the JS handler + struct when the widget is destroyed */
    lv_obj_add_event_cb(obj, lv_event_free, LV_EVENT_DELETE, ecb);
    return JS_UNDEFINED;
}

#define SG_FONT_FILE_CAP 32
#define SG_FONT_HANDLE_CAP 128

typedef struct {
    char *path;
    uint8_t *data;
    size_t len;
} sg_font_file_t;

typedef struct {
    const char *path;
    int32_t size;
    lv_font_t *font;
} sg_font_handle_t;

/* Font files and font objects are immortal because Tiny-TTF keeps the source
 * data by reference. Exact path strings are the cache key; resolving relative
 * paths or symlink aliases is intentionally outside this catalog's scope. */
static sg_font_file_t g_font_files[SG_FONT_FILE_CAP];
static size_t g_font_file_count;
static sg_font_handle_t g_font_handles[SG_FONT_HANDLE_CAP];
static size_t g_font_handle_count;

/* Typography roles in doc/theme.md §6.1: base, medium, large, display. */
static const int32_t g_default_font_sizes[] = { 14, 16, 20, 24 };
static const lv_font_t *g_default_fonts[4];

static sg_font_file_t *find_font_file(const char *path) {
    for (size_t i = 0; i < g_font_file_count; i++) {
        if (strcmp(g_font_files[i].path, path) == 0) return &g_font_files[i];
    }
    return NULL;
}

static lv_font_t *find_font_handle(const char *path, int32_t size) {
    for (size_t i = 0; i < g_font_handle_count; i++) {
        sg_font_handle_t *entry = &g_font_handles[i];
        if (entry->size == size && strcmp(entry->path, path) == 0) return entry->font;
    }
    return NULL;
}

static sg_font_file_t *load_font_file(const char *path) {
    if (g_font_file_count >= SG_FONT_FILE_CAP) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long file_len = ftell(f);
    if (file_len <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    size_t len = (size_t)file_len;
    uint8_t *data = lv_malloc(len);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, len, f) != len) {
        lv_free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    size_t path_len = strlen(path) + 1;
    char *stored_path = lv_malloc(path_len);
    if (!stored_path) {
        lv_free(data);
        return NULL;
    }
    memcpy(stored_path, path, path_len);

    sg_font_file_t *entry = &g_font_files[g_font_file_count++];
    entry->path = stored_path;
    entry->data = data;
    entry->len = len;
    return entry;
}

/* loadFont(path, size) → font handle (int), or 0 on failure.
 * The first size loaded for a path reads and retains the file bytes; other
 * sizes share those bytes but keep separate 256-glyph Tiny-TTF caches. */
static JSValue js_loadFont(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;

    const char *path = JS_ToCString(ctx, argv[0]);
    int32_t size; JS_ToInt32(ctx, &size, argv[1]);
    if (!path) return JS_EXCEPTION;

    JSValue result = JS_NewInt64(ctx, 0);
    lv_font_t *font = find_font_handle(path, size);
    if (font) {
        result = obj_to_js(ctx, (lv_obj_t *)font);
        JS_FreeCString(ctx, path);
        return result;
    }

    if (g_font_handle_count >= SG_FONT_HANDLE_CAP) {
        JS_FreeCString(ctx, path);
        return result;
    }

    sg_font_file_t *file = find_font_file(path);
    if (!file) file = load_font_file(path);
    if (file) {
        font = lv_tiny_ttf_create_data_ex(file->data, file->len, size,
                                          LV_FONT_KERNING_NORMAL,
                                          LV_TINY_TTF_CACHE_GLYPH_CNT);
        if (font) {
            /* CJK TTF files have no glyphs for LVGL's built-in icon
             * symbols (LV_SYMBOL_OK/DOWN/… live in the 0xF000+ private
             * use area, supplied only by Montserrat). Fall back to the
             * nearest enabled Montserrat so checkbox ticks, dropdown
             * arrows, etc. render instead of showing tofu boxes. */
            const lv_font_t *fb = &lv_font_montserrat_14;
            if      (size >= 24) fb = &lv_font_montserrat_24;
            else if (size >= 20) fb = &lv_font_montserrat_20;
            else if (size >= 16) fb = &lv_font_montserrat_16;
            font->fallback = fb;

            sg_font_handle_t *entry = &g_font_handles[g_font_handle_count++];
            entry->path = file->path;
            entry->size = size;
            entry->font = font;
            result = obj_to_js(ctx, (lv_obj_t *)font);
        }
    }
    JS_FreeCString(ctx, path);
    return result;
}

/* loadImage(path) → handle (opaque int), 0 on failure.
 * Validates the file exists, normalizes to "A:/abs/path" if needed,
 * strdup's the path into an sg_image_handle_t kept alive forever
 * (mirrors loadFont's "never free" catalog pattern). */
static JSValue js_loadImage(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    const char *raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;

    JSValue result = JS_NewInt64(ctx, 0);

    /* Strip any leading "A:" so we can stat the real file. */
    const char *fs_path = raw;
    if (raw[0] == 'A' && raw[1] == ':') fs_path = raw + 2;

    FILE *f = fopen(fs_path, "rb");
    if (f) {
        fclose(f);

        /* Normalize: ensure "A:" prefix. */
        size_t needed = strlen(fs_path) + 3; /* "A:" + path + NUL */
        char *normalized = lv_malloc(needed);
        if (normalized) {
            snprintf(normalized, needed, "A:%s", fs_path);

            sg_image_handle_t *hdl = lv_malloc(sizeof(sg_image_handle_t));
            if (hdl) {
                hdl->path = normalized;  /* both intentionally never freed */
                result = obj_to_js(ctx, (lv_obj_t *)hdl);
            } else {
                lv_free(normalized);
            }
        }
    }
    JS_FreeCString(ctx, raw);
    return result;
}

/* loadImages([path, path, ...]) → [handle, handle, ...]. */
static JSValue js_loadImages(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsArray(ctx, argv[0])) return JS_EXCEPTION;

    JSValue len_val = JS_GetPropertyStr(ctx, argv[0], "length");
    uint32_t len = 0;
    JS_ToUint32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < len; i++) {
        JSValue item = JS_GetPropertyUint32(ctx, argv[0], i);
        JSValueConst args[1] = { item };
        JSValue h = js_loadImage(ctx, JS_UNDEFINED, 1, args);
        JS_SetPropertyUint32(ctx, arr, i, h);
        JS_FreeValue(ctx, item);
    }
    return arr;
}

/* setDefaultFont(handleOrMap) — a number applies one face to every role for
 * backward compatibility; a map transfers all four role faces to sg_theme. */
static JSValue js_setDefaultFont(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;

    if (JS_IsObject(argv[0])) {
        bool updated = false;
        memset(g_default_fonts, 0, sizeof(g_default_fonts));
        for (size_t i = 0; i < 4; i++) {
            char key[4];
            snprintf(key, sizeof(key), "%d", (int)g_default_font_sizes[i]);
            JSValue value = JS_GetPropertyStr(ctx, argv[0], key);
            if (JS_IsNumber(value)) {
                int64_t h = 0;
                if (JS_ToInt64(ctx, &h, value) == 0 && h) {
                    g_default_fonts[i] = (const lv_font_t *)(uintptr_t)h;
                    updated = true;
                }
            }
            JS_FreeValue(ctx, value);
        }
        if (!updated) return JS_UNDEFINED;

        sg_theme_set_role_fonts(lv_display_get_default(),
                                g_default_fonts[0], g_default_fonts[1],
                                g_default_fonts[2], g_default_fonts[3]);
        return JS_UNDEFINED;
    }

    int64_t h; JS_ToInt64(ctx, &h, argv[0]);
    if (!h) return JS_UNDEFINED;

    sg_theme_set_font(lv_display_get_default(), (const lv_font_t *)(uintptr_t)h);
    return JS_UNDEFINED;
}

/* addTab(tabview, title) → page node handle. */
static JSValue js_addTab(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t   *tv    = js_to_obj(ctx, argv[0]);
    const char *title = JS_ToCString(ctx, argv[1]);
    lv_obj_t   *page  = lv_tabview_add_tab(tv, title ? title : "");
    JS_FreeCString(ctx, title);
    return obj_to_js(ctx, page);
}

/* listAddButton(list, text) → button node handle. Auto-added to focus group. */
static JSValue js_listAddButton(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t   *list = js_to_obj(ctx, argv[0]);
    const char *text = JS_ToCString(ctx, argv[1]);
    lv_obj_t   *btn  = lv_list_add_button(list, NULL, text ? text : "");
    JS_FreeCString(ctx, text);
    if (g_group && btn) lv_group_add_obj(g_group, btn);
    return obj_to_js(ctx, btn);
}

/* menuAddPage(menu, title) → page node handle.
 *
 * The first page added is auto-activated (lv_menu_create leaves the menu
 * blank otherwise). The "did we already auto-activate?" flag lives on the
 * menu's user_data — any non-NULL value means we have set a page. */
static JSValue js_menuAddPage(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t   *menu  = js_to_obj(ctx, argv[0]);
    const char *title = JS_ToCString(ctx, argv[1]);
    lv_obj_t   *page  = lv_menu_page_create(menu, title ? (char *)title : NULL);
    JS_FreeCString(ctx, title);
    if (page && menu && lv_obj_get_user_data(menu) == NULL) {
        lv_menu_set_page(menu, page);
        lv_obj_set_user_data(menu, (void *)1);
    }
    return obj_to_js(ctx, page);
}

/* menuSetPage(menu, page) — switch the displayed page imperatively. */
static JSValue js_menuSetPage(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t *menu = js_to_obj(ctx, argv[0]);
    lv_obj_t *page = js_to_obj(ctx, argv[1]);
    lv_menu_set_page(menu, page);
    return JS_UNDEFINED;
}

/* chartAddSeries(chart, colorString) → series handle (int).
 * Series are owned by the chart and freed when the chart is deleted. */
static JSValue js_chartAddSeries(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t   *chart = js_to_obj(ctx, argv[0]);
    const char *col   = JS_ToCString(ctx, argv[1]);
    lv_color_t  c     = col ? parse_color(col) : lv_palette_main(LV_PALETTE_BLUE);
    JS_FreeCString(ctx, col);
    lv_chart_series_t *ser = lv_chart_add_series(chart, c, LV_CHART_AXIS_PRIMARY_Y);
    return JS_NewInt64(ctx, (int64_t)(uintptr_t)ser);
}

/* chartSetSeriesColor(chart, series, colorString) — repaints an EXISTING
 * series. lv_chart_add_series copies the colour into the series struct, so a
 * theme/accent change cannot reach a series created earlier; without this the
 * chart is the one control that keeps a stale accent after setThemeToken. */
static JSValue js_chartSetSeriesColor(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) return JS_EXCEPTION;
    lv_obj_t          *chart = js_to_obj(ctx, argv[0]);
    int64_t            sh;    JS_ToInt64(ctx, &sh, argv[1]);
    lv_chart_series_t *ser   = (lv_chart_series_t *)(uintptr_t)sh;
    if (!chart || !ser) return JS_UNDEFINED;

    const char *col = JS_ToCString(ctx, argv[2]);
    if (col) {
        lv_chart_set_series_color(chart, ser, parse_color(col));
        JS_FreeCString(ctx, col);
    }
    return JS_UNDEFINED;
}

/* chartSetData(chart, series, [v0, v1, ...]) — replaces all points; resizes
 * the chart's point count to match the array length. */
static JSValue js_chartSetData(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) return JS_EXCEPTION;
    lv_obj_t          *chart = js_to_obj(ctx, argv[0]);
    int64_t            sh;    JS_ToInt64(ctx, &sh, argv[1]);
    lv_chart_series_t *ser   = (lv_chart_series_t *)(uintptr_t)sh;
    if (!ser || !JS_IsArray(ctx, argv[2])) return JS_UNDEFINED;

    uint32_t len = 0;
    JSValue jlen = JS_GetPropertyStr(ctx, argv[2], "length");
    JS_ToUint32(ctx, &len, jlen);
    JS_FreeValue(ctx, jlen);

    if (len > 0 && lv_chart_get_point_count(chart) != len)
        lv_chart_set_point_count(chart, len);

    for (uint32_t i = 0; i < len; i++) {
        JSValue it = JS_GetPropertyUint32(ctx, argv[2], i);
        int32_t v; JS_ToInt32(ctx, &v, it);
        lv_chart_set_value_by_id(chart, ser, i, v);
        JS_FreeValue(ctx, it);
    }
    return JS_UNDEFINED;
}

/* showMsgbox({title, text, buttons:[str,...]}, onClose?)
 * Opens a modal msgbox. onClose(idx) is called with the index of the clicked
 * footer button, or -1 if the close button (X) was pressed. The msgbox closes
 * itself on any button press. */
typedef struct {
    JSContext *ctx;
    JSValue    on_close;
    JSValue    button_indices;   /* array used to map button obj → index    */
} msgbox_ctx_t;

static void msgbox_button_cb(lv_event_t *e) {
    msgbox_ctx_t *m = lv_event_get_user_data(e);
    lv_obj_t     *btn = lv_event_get_current_target_obj(e);

    int32_t idx = -1;
    uint32_t n = 0;
    JSValue jlen = JS_GetPropertyStr(m->ctx, m->button_indices, "length");
    JS_ToUint32(m->ctx, &n, jlen);
    JS_FreeValue(m->ctx, jlen);
    for (uint32_t i = 0; i < n; i++) {
        JSValue it = JS_GetPropertyUint32(m->ctx, m->button_indices, i);
        int64_t ptr; JS_ToInt64(m->ctx, &ptr, it);
        JS_FreeValue(m->ctx, it);
        if ((lv_obj_t *)(uintptr_t)ptr == btn) { idx = (int32_t)i; break; }
    }

    JSValue arg = JS_NewInt32(m->ctx, idx);
    JSValue ret = JS_Call(m->ctx, m->on_close, JS_UNDEFINED, 1, &arg);
    if (JS_IsException(ret)) js_std_dump_error(m->ctx);
    JS_FreeValue(m->ctx, ret);
    JS_FreeValue(m->ctx, arg);

    lv_obj_t *mbox = lv_obj_get_parent(lv_obj_get_parent(btn));
    if (mbox && lv_obj_get_class(mbox) == &lv_msgbox_class)
        lv_msgbox_close_async(mbox);
}

static void msgbox_close_cb(lv_event_t *e) {
    msgbox_ctx_t *m = lv_event_get_user_data(e);
    if (!JS_IsUndefined(m->on_close)) {
        JSValue arg = JS_NewInt32(m->ctx, -1);
        JSValue ret = JS_Call(m->ctx, m->on_close, JS_UNDEFINED, 1, &arg);
        if (JS_IsException(ret)) js_std_dump_error(m->ctx);
        JS_FreeValue(m->ctx, ret);
        JS_FreeValue(m->ctx, arg);
    }
    lv_obj_t *mbox = lv_obj_get_parent(lv_obj_get_parent(lv_event_get_current_target_obj(e)));
    if (mbox && lv_obj_get_class(mbox) == &lv_msgbox_class)
        lv_msgbox_close_async(mbox);
}

static void msgbox_delete_cb(lv_event_t *e) {
    msgbox_ctx_t *m = lv_event_get_user_data(e);
    if (!m) return;
    JS_FreeValue(m->ctx, m->on_close);
    JS_FreeValue(m->ctx, m->button_indices);
    lv_free(m);
}

static JSValue js_showMsgbox(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_EXCEPTION;

    JSValue jtitle = JS_GetPropertyStr(ctx, argv[0], "title");
    JSValue jtext  = JS_GetPropertyStr(ctx, argv[0], "text");
    JSValue jbtns  = JS_GetPropertyStr(ctx, argv[0], "buttons");

    const char *title = JS_ToCString(ctx, jtitle);
    const char *text  = JS_ToCString(ctx, jtext);

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    if (title) lv_msgbox_add_title(mbox, title);
    if (text)  lv_msgbox_add_text(mbox, text);
    /* NOT lv_msgbox_add_close_button: that helper registers LVGL's own CLICKED
     * handler first (lv_msgbox.c:228-233), which calls lv_msgbox_close() and
     * DELETES the msgbox synchronously. The event dispatcher then aborts the
     * remaining callbacks on the dead object, so the onClose(-1) this module
     * documents could never fire. Building the same affordance by hand keeps
     * the close path entirely ours. */
    lv_obj_t *close_btn = lv_msgbox_add_header_button(mbox, LV_SYMBOL_CLOSE);

    msgbox_ctx_t *mctx = lv_malloc(sizeof(*mctx));
    mctx->ctx            = ctx;
    mctx->on_close       = (argc >= 2) ? JS_DupValue(ctx, argv[1])
                                       : JS_UNDEFINED;
    mctx->button_indices = JS_NewArray(ctx);

    if (JS_IsArray(ctx, jbtns)) {
        uint32_t n = 0;
        JSValue jlen = JS_GetPropertyStr(ctx, jbtns, "length");
        JS_ToUint32(ctx, &n, jlen);
        JS_FreeValue(ctx, jlen);
        for (uint32_t i = 0; i < n; i++) {
            JSValue it = JS_GetPropertyUint32(ctx, jbtns, i);
            const char *btxt = JS_ToCString(ctx, it);
            lv_obj_t *b = lv_msgbox_add_footer_button(mbox, btxt ? btxt : "");
            JS_FreeCString(ctx, btxt);
            JS_FreeValue(ctx, it);
            JS_SetPropertyUint32(ctx, mctx->button_indices, i,
                                 JS_NewInt64(ctx, (int64_t)(uintptr_t)b));
            if (!JS_IsUndefined(mctx->on_close))
                lv_obj_add_event_cb(b, msgbox_button_cb, LV_EVENT_CLICKED, mctx);
        }
    }
    if (close_btn)
        lv_obj_add_event_cb(close_btn, msgbox_close_cb, LV_EVENT_CLICKED, mctx);
    lv_obj_add_event_cb(mbox, msgbox_delete_cb, LV_EVENT_DELETE, mctx);

    JS_FreeCString(ctx, title);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, jtitle);
    JS_FreeValue(ctx, jtext);
    JS_FreeValue(ctx, jbtns);

    return obj_to_js(ctx, mbox);
}

static JSValue js_clipboardRead(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    char *text = SDL_GetClipboardText();
    JSValue v = (text && *text) ? JS_NewString(ctx, text) : JS_NULL;
    if (text) SDL_free(text);
    return v;
}

static JSValue js_clipboardWrite(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) { SDL_SetClipboardText(s); JS_FreeCString(ctx, s); }
    return JS_UNDEFINED;
}

static JSValue js_focus(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    lv_obj_t *obj = js_to_obj(ctx, argv[0]);
    if (!g_group || !obj) return JS_NewBool(ctx, false);
    lv_group_focus_obj(obj);
    return JS_NewBool(ctx, lv_group_get_focused(g_group) == obj);
}

/* sendKey(name, ctrl) — inject an SDL key press.
 *
 * SDL_PushEvent runs every SDL_AddEventWatch callback SYNCHRONOUSLY before
 * queueing, so `sdl_event_watch` in main.c (where the Ctrl+A/C/V/X and
 * Home/End handling lives) reacts before this function returns. That watch
 * reads SDL_GetModState() rather than the event's keysym, hence the
 * set-modifier / restore dance around the push. */
/* sendEvent(node, name) → bool. Dispatches a synthetic LVGL event so a scripted
 * run exercises the SAME handler a real pointer would, instead of reaching past
 * it. "released" is what toggles a dropdown (lv_dropdown.c btn_release_handler,
 * which is NULL-indev safe because lv_indev_get_scroll_obj(NULL) returns NULL);
 * "click" is what the msgbox footer and close buttons listen for. Returns false
 * for an unmapped name rather than silently doing nothing. */
static JSValue js_sendEvent(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_EXCEPTION;
    lv_obj_t   *obj  = js_to_obj(ctx, argv[0]);
    const char *name = JS_ToCString(ctx, argv[1]);
    if (!obj || !name) {
        JS_FreeCString(ctx, name);
        return JS_NewBool(ctx, false);
    }

    lv_event_code_t code = LV_EVENT_CLICKED;
    bool known = true;
    if      (strcmp(name, "click")    == 0) code = LV_EVENT_CLICKED;
    else if (strcmp(name, "released") == 0) code = LV_EVENT_RELEASED;
    else if (strcmp(name, "change")   == 0) code = LV_EVENT_VALUE_CHANGED;
    else known = false;
    JS_FreeCString(ctx, name);
    if (!known) return JS_NewBool(ctx, false);

    lv_obj_send_event(obj, code, NULL);
    return JS_NewBool(ctx, true);
}

static JSValue js_sendKey(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    bool ctrl = (argc > 1) && JS_ToBool(ctx, argv[1]);

    SDL_Keycode key = SDLK_UNKNOWN;
    if (name[0] >= 'a' && name[0] <= 'z' && name[1] == '\0')
        key = (SDL_Keycode)name[0];
    else if (strcmp(name, "home") == 0) key = SDLK_HOME;
    else if (strcmp(name, "end") == 0)  key = SDLK_END;
    JS_FreeCString(ctx, name);
    if (key == SDLK_UNKNOWN) return JS_NewBool(ctx, false);

    SDL_Keymod prev = SDL_GetModState();
    SDL_SetModState(ctrl ? KMOD_LCTRL : KMOD_NONE);

    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type             = SDL_KEYDOWN;
    e.key.state        = SDL_PRESSED;
    e.key.keysym.sym   = key;
    e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    e.key.keysym.mod   = ctrl ? KMOD_LCTRL : KMOD_NONE;
    int pushed = SDL_PushEvent(&e);

    SDL_SetModState(prev);
    return JS_NewBool(ctx, pushed == 1);
}

static JSValue js_findCjkFontPath(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    static const char *const candidates[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "C:/Windows/Fonts/msyh.ttc",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) { fclose(f); return JS_NewString(ctx, candidates[i]); }
    }
    return JS_NULL;
}

static JSValue js_setTheme(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_EXCEPTION;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        sg_theme_set_scheme(lv_display_get_default(), s);
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_getThemeToken(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "getThemeToken requires a name");
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    sg_theme_token_value_t value;
    sg_theme_token_result_t result = sg_theme_get_token(name, &value);
    JS_FreeCString(ctx, name);
    if (result == SG_THEME_TOKEN_UNKNOWN)
        return JS_ThrowTypeError(ctx, "unknown theme token name");
    if (value.kind != SG_TOKEN_COLOR)
        return JS_ThrowTypeError(ctx, "theme token is not a color");

    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x",
             value.color.red, value.color.green, value.color.blue);
    return JS_NewString(ctx, hex);
}

/* getThemeMetric(name) → number. The integer half of the registry
 * (radius_*, border_width, space_*, control_height, …). Deliberately a
 * SEPARATE entry point rather than widening getThemeToken: the colour/integer
 * split is a contract this project pins with a test, and setThemeToken already
 * keeps the two kinds apart through overloads. Lets a bundle build layout
 * primitives from the same tokens the theme paints with instead of re-typing
 * 4/8/12/16 as literals. */
static JSValue js_getThemeMetric(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_ThrowTypeError(ctx, "getThemeMetric requires a name");
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    sg_theme_token_value_t value;
    sg_theme_token_result_t result = sg_theme_get_token(name, &value);
    JS_FreeCString(ctx, name);
    if (result == SG_THEME_TOKEN_UNKNOWN)
        return JS_ThrowTypeError(ctx, "unknown theme token name");
    if (value.kind != SG_TOKEN_INT)
        return JS_ThrowTypeError(ctx, "theme token is not an integer metric");

    return JS_NewInt32(ctx, value.integer);
}

static JSValue js_setThemeToken(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2) return JS_ThrowTypeError(ctx, "setThemeToken requires name and value");
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    sg_theme_token_value_t value;
    if (JS_IsString(argv[1])) {
        const char *color = JS_ToCString(ctx, argv[1]);
        if (!color) {
            JS_FreeCString(ctx, name);
            return JS_EXCEPTION;
        }
        value.kind = SG_TOKEN_COLOR;
        value.color = parse_color_ex(color).color;
        JS_FreeCString(ctx, color);
    } else if (JS_IsNumber(argv[1])) {
        value.kind = SG_TOKEN_INT;
        if (JS_ToInt32(ctx, &value.integer, argv[1]) < 0) {
            JS_FreeCString(ctx, name);
            return JS_EXCEPTION;
        }
    } else {
        JS_FreeCString(ctx, name);
        return JS_ThrowTypeError(ctx, "theme token value must be a color string or number");
    }

    sg_theme_token_result_t result = sg_theme_set_token(
        lv_display_get_default(), name, value);
    JS_FreeCString(ctx, name);
    if (result == SG_THEME_TOKEN_UNKNOWN)
        return JS_ThrowTypeError(ctx, "unknown theme token name");
    if (result == SG_THEME_TOKEN_WRONG_KIND)
        return JS_ThrowTypeError(ctx, "theme token value has the wrong kind");
    return JS_UNDEFINED;
}

/* ── Animation API ──────────────────────────────────────────────────────── */

typedef struct {
    JSContext *ctx;
    JSValue    on_complete;
} sg_anim_cb_t;

static void sg_anim_completed(lv_anim_t *anim) {
    sg_anim_cb_t *cb = (sg_anim_cb_t *)lv_anim_get_user_data(anim);
    if (!cb || JS_IsUndefined(cb->on_complete)) return;
    JSValue ret = JS_Call(cb->ctx, cb->on_complete, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(ret)) js_std_dump_error(cb->ctx);
    JS_FreeValue(cb->ctx, ret);
}

static void sg_anim_deleted(lv_anim_t *anim) {
    sg_anim_cb_t *cb = (sg_anim_cb_t *)lv_anim_get_user_data(anim);
    if (!cb) return;
    JS_FreeValue(cb->ctx, cb->on_complete);
    lv_free(cb);
}

/* Property exec wrappers — `lv_obj_set_{x,y,width,height}` already match
 * lv_anim_exec_xcb_t; the rest need a wrapper that either passes a selector
 * (style props need one) or dispatches by widget class (the "value" anim). */

static void anim_exec_opacity(void *var, int32_t v) {
    if (v < 0) v = 0; else if (v > 255) v = 255;
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void anim_exec_rotation(void *var, int32_t v) {
    lv_obj_set_style_transform_rotation((lv_obj_t *)var, v, 0);
}

static void anim_exec_scale(void *var, int32_t v) {
    lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

static void anim_exec_value(void *var, int32_t v) {
    lv_obj_t *obj = (lv_obj_t *)var;
    const lv_obj_class_t *cls = lv_obj_get_class(obj);
    if      (cls == &lv_bar_class)      lv_bar_set_value(obj, v, LV_ANIM_OFF);
    else if (cls == &lv_slider_class)   lv_slider_set_value(obj, v, LV_ANIM_OFF);
    else if (cls == &lv_arc_class)      lv_arc_set_value(obj, v);
    else if (cls == &lv_dropdown_class) lv_dropdown_set_selected(obj, (uint32_t)v);
    else if (cls == &lv_roller_class)   lv_roller_set_selected(obj, (uint32_t)v, LV_ANIM_OFF);
    else if (cls == &lv_spinbox_class)  lv_spinbox_set_value(obj, v);
}

/* createAnimation(node, opts) — opts = {
 *   property:   "x" | "y" | "width" | "height" | "opacity" | "rotation"
 *               | "scale" | "value"                  (required)
 *   from, to:   int                                  (required)
 *   duration:   ms                                   (default 200)
 *   easing:     "linear" | "ease-in" | "ease-out" | "ease-in-out"
 *               | "overshoot" | "bounce" | "step"    (default "linear")
 *   delay:      ms                                   (default 0)
 *   repeat:     int | "infinite"                     (default 1)
 *   onComplete: function()                           (optional)
 * }
 */
static JSValue js_createAnimation(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2 || !JS_IsObject(argv[1])) return JS_EXCEPTION;

    lv_obj_t *obj = js_to_obj(ctx, argv[0]);

    JSValue jprop = JS_GetPropertyStr(ctx, argv[1], "property");
    JSValue jfrom = JS_GetPropertyStr(ctx, argv[1], "from");
    JSValue jto   = JS_GetPropertyStr(ctx, argv[1], "to");
    JSValue jdur  = JS_GetPropertyStr(ctx, argv[1], "duration");
    JSValue jease = JS_GetPropertyStr(ctx, argv[1], "easing");
    JSValue jrep  = JS_GetPropertyStr(ctx, argv[1], "repeat");
    JSValue jdel  = JS_GetPropertyStr(ctx, argv[1], "delay");
    JSValue jcb   = JS_GetPropertyStr(ctx, argv[1], "onComplete");

    const char *prop = JS_ToCString(ctx, jprop);
    const char *ease = JS_ToCString(ctx, jease);
    int32_t  from = 0, to = 0, duration = 200, delay = 0;
    uint32_t repeat = 1;

    if (JS_IsNumber(jfrom)) JS_ToInt32(ctx, &from, jfrom);
    if (JS_IsNumber(jto))   JS_ToInt32(ctx, &to,   jto);
    if (JS_IsNumber(jdur))  JS_ToInt32(ctx, &duration, jdur);
    if (JS_IsNumber(jdel))  JS_ToInt32(ctx, &delay, jdel);
    if (JS_IsNumber(jrep)) {
        int32_t r; JS_ToInt32(ctx, &r, jrep);
        if (r > 0) repeat = (uint32_t)r;
    } else if (JS_IsString(jrep)) {
        const char *s = JS_ToCString(ctx, jrep);
        if (s && strcmp(s, "infinite") == 0) repeat = LV_ANIM_REPEAT_INFINITE;
        JS_FreeCString(ctx, s);
    }

    lv_anim_exec_xcb_t exec_cb = NULL;
    if (prop) {
        if      (strcmp(prop, "x")        == 0) exec_cb = (lv_anim_exec_xcb_t)lv_obj_set_x;
        else if (strcmp(prop, "y")        == 0) exec_cb = (lv_anim_exec_xcb_t)lv_obj_set_y;
        else if (strcmp(prop, "width")    == 0) exec_cb = (lv_anim_exec_xcb_t)lv_obj_set_width;
        else if (strcmp(prop, "height")   == 0) exec_cb = (lv_anim_exec_xcb_t)lv_obj_set_height;
        else if (strcmp(prop, "opacity")  == 0) exec_cb = anim_exec_opacity;
        else if (strcmp(prop, "rotation") == 0) exec_cb = anim_exec_rotation;
        else if (strcmp(prop, "scale")    == 0) exec_cb = anim_exec_scale;
        else if (strcmp(prop, "value")    == 0) exec_cb = anim_exec_value;
    }

    lv_anim_path_cb_t path_cb = lv_anim_path_linear;
    if (ease) {
        if      (strcmp(ease, "ease-in")     == 0) path_cb = lv_anim_path_ease_in;
        else if (strcmp(ease, "ease-out")    == 0) path_cb = lv_anim_path_ease_out;
        else if (strcmp(ease, "ease-in-out") == 0) path_cb = lv_anim_path_ease_in_out;
        else if (strcmp(ease, "overshoot")   == 0) path_cb = lv_anim_path_overshoot;
        else if (strcmp(ease, "bounce")      == 0) path_cb = lv_anim_path_bounce;
        else if (strcmp(ease, "step")        == 0) path_cb = lv_anim_path_step;
    }

    JSValue result = JS_UNDEFINED;
    if (exec_cb == NULL) {
        result = JS_ThrowTypeError(ctx,
            "createAnimation: unknown property '%s' "
            "(use x/y/width/height/opacity/rotation/scale/value)",
            prop ? prop : "(null)");
    } else {
        sg_anim_cb_t *cb = lv_malloc(sizeof(*cb));
        cb->ctx         = ctx;
        cb->on_complete = JS_IsFunction(ctx, jcb) ? JS_DupValue(ctx, jcb) : JS_UNDEFINED;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, obj);
        lv_anim_set_values(&a, from, to);
        lv_anim_set_duration(&a, (uint32_t)duration);
        lv_anim_set_exec_cb(&a, exec_cb);
        lv_anim_set_path_cb(&a, path_cb);
        lv_anim_set_user_data(&a, cb);
        lv_anim_set_completed_cb(&a, sg_anim_completed);
        lv_anim_set_deleted_cb(&a, sg_anim_deleted);
        if (delay > 0)   lv_anim_set_delay(&a, (uint32_t)delay);
        if (repeat != 1) lv_anim_set_repeat_count(&a, repeat);
        lv_anim_start(&a);
    }

    JS_FreeCString(ctx, prop);
    JS_FreeCString(ctx, ease);
    JS_FreeValue(ctx, jprop);
    JS_FreeValue(ctx, jfrom);
    JS_FreeValue(ctx, jto);
    JS_FreeValue(ctx, jdur);
    JS_FreeValue(ctx, jease);
    JS_FreeValue(ctx, jrep);
    JS_FreeValue(ctx, jdel);
    JS_FreeValue(ctx, jcb);

    return result;
}

/* ── module export list ─────────────────────────────────────────────────── */

static const JSCFunctionListEntry lv_funcs[] = {
    JS_CFUNC_DEF("getScreen",      0, js_getScreen),
    JS_CFUNC_DEF("getParent",      1, js_getParent),
    JS_CFUNC_DEF("getChild",       2, js_getChild),
    JS_CFUNC_DEF("createThemeCoverageMenuInternals", 1, js_createThemeCoverageMenuInternals),
    JS_CFUNC_DEF("createNode",     1, js_createNode),
    JS_CFUNC_DEF("appendChild",    2, js_appendChild),
    JS_CFUNC_DEF("removeChild",    2, js_removeChild),
    JS_CFUNC_DEF("setProperty",    3, js_setProperty),
    JS_CFUNC_DEF("getProperty",    2, js_getProperty),
    JS_CFUNC_DEF("updateLayout",   1, js_updateLayout),
    JS_CFUNC_DEF("addEvent",       3, js_addEvent),
    JS_CFUNC_DEF("dispose",        1, js_dispose),
    JS_CFUNC_DEF("loadFont",       2, js_loadFont),
    JS_CFUNC_DEF("setDefaultFont", 1, js_setDefaultFont),
    JS_CFUNC_DEF("loadImage",      1, js_loadImage),
    JS_CFUNC_DEF("loadImages",     1, js_loadImages),
    JS_CFUNC_DEF("addTab",         2, js_addTab),
    JS_CFUNC_DEF("listAddButton",  2, js_listAddButton),
    JS_CFUNC_DEF("menuAddPage",    2, js_menuAddPage),
    JS_CFUNC_DEF("menuSetPage",    2, js_menuSetPage),
    JS_CFUNC_DEF("chartAddSeries", 2, js_chartAddSeries),
    JS_CFUNC_DEF("chartSetSeriesColor", 3, js_chartSetSeriesColor),
    JS_CFUNC_DEF("chartSetData",   3, js_chartSetData),
    JS_CFUNC_DEF("showMsgbox",     2, js_showMsgbox),
    JS_CFUNC_DEF("createAnimation",2, js_createAnimation),
    JS_CFUNC_DEF("findCjkFontPath",0, js_findCjkFontPath),
    JS_CFUNC_DEF("setTheme",       1, js_setTheme),
    JS_CFUNC_DEF("getThemeToken",  1, js_getThemeToken),
    JS_CFUNC_DEF("getThemeMetric", 1, js_getThemeMetric),
    JS_CFUNC_DEF("setThemeToken",  2, js_setThemeToken),
    JS_CFUNC_DEF("clipboardRead",  0, js_clipboardRead),
    JS_CFUNC_DEF("clipboardWrite", 1, js_clipboardWrite),
    JS_CFUNC_DEF("focus",          1, js_focus),
    JS_CFUNC_DEF("sendKey",        2, js_sendKey),
    JS_CFUNC_DEF("sendEvent",      2, js_sendEvent),
};

static int lv_module_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, lv_funcs,
                                  sizeof(lv_funcs) / sizeof(lv_funcs[0]));
}

JSModuleDef *js_init_module_lvgl(JSContext *ctx, const char *module_name) {
    JSModuleDef *m = JS_NewCModule(ctx, module_name, lv_module_init);
    if (!m) return NULL;
    JS_AddModuleExportList(ctx, m, lv_funcs,
                           sizeof(lv_funcs) / sizeof(lv_funcs[0]));
    return m;
}

void lv_bindings_register(JSContext *ctx) {
    js_init_module_lvgl(ctx, "lvgl");
}

void lv_bindings_flush_callbacks(JSContext *ctx) {
    /* QuickJS pending jobs (Promise continuations etc.) */
    JSContext *ctx1;
    int ret;
    do {
        ret = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
    } while (ret > 0);
}
