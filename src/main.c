/**
 * main.c — stonegui host
 *
 * Initialises:
 *   1. LVGL
 *   2. LVGL SDL display + mouse/keyboard inputs
 *   3. QuickJS runtime
 *   4. Native "lvgl" module bindings
 *   5. Loads and executes the JS bundle
 *   6. Runs the event loop
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include <SDL.h>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"

#include "quickjs.h"
#include "quickjs-libc.h"

#include "lv_bindings.h"
#include "sg_theme.h"

#define DISPLAY_WIDTH  800
#define DISPLAY_HEIGHT 480
#define BUNDLE_PATH    "examples/hello/app.js"

/* ── Shutdown signalling ────────────────────────────────────────────────────
 *
 * The event loop stops when either the user closes the SDL window (SDL_QUIT)
 * or sends a Unix signal (SIGINT / SIGTERM). g_exit_signal records which
 * signal fired so we can return the conventional 128+signo exit code; a clean
 * SDL_QUIT (no signal) yields exit 0.
 *
 * sig_atomic_t guarantees torn-write-free reads across the signal-handler /
 * main-thread boundary.
 */
static volatile sig_atomic_t g_running     = 1;
static volatile sig_atomic_t g_exit_signal = 0;

static void on_signal(int sig) {
    g_exit_signal = sig;
    g_running     = 0;
}

/* SDL event watcher — fires synchronously on every SDL_PumpEvents call (which
 * lv_timer_handler triggers via the LVGL SDL driver). We only observe; the
 * return value is ignored by SDL_AddEventWatch and the event still flows on
 * to LVGL's own SDL input handlers. */
static int sdl_event_watch(void *userdata, SDL_Event *e) {
    (void)userdata;
    if (e->type == SDL_QUIT) g_running = 0;
    return 1;
}

/* ── Hot reload (inotify) ───────────────────────────────────────────────────
 *
 * Watches the bundle's PARENT DIRECTORY (not the file directly) so we catch
 * the typical "write to tmp + rename" save pattern most editors use — IN_FILE
 * watches die when the inode is replaced. Filter the dir events by filename
 * to ignore unrelated writes.
 *
 * Disable with the `--no-watch` CLI flag. After a reload, all JS state is
 * fresh (signals, fonts, loaded resources must be re-initialised by the
 * bundle); LVGL state (window, theme, input devices, focus group) persists.
 */
static int  g_inotify_fd        = -1;
static int  g_should_reload     = 0;
static int  g_enable_hot_reload = 1;
static char g_bundle_dir[1024]  = "";
static char g_bundle_name[256]  = "";

static void setup_hot_reload(const char *bundle) {
    if (!g_enable_hot_reload) return;

    const char *slash = strrchr(bundle, '/');
    if (slash) {
        size_t dirlen = (size_t)(slash - bundle);
        if (dirlen >= sizeof(g_bundle_dir)) {
            fprintf(stderr, "stonegui: bundle dir path too long; hot reload disabled\n");
            return;
        }
        memcpy(g_bundle_dir, bundle, dirlen);
        g_bundle_dir[dirlen] = '\0';
        snprintf(g_bundle_name, sizeof(g_bundle_name), "%s", slash + 1);
    } else {
        strcpy(g_bundle_dir, ".");
        snprintf(g_bundle_name, sizeof(g_bundle_name), "%s", bundle);
    }

    g_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (g_inotify_fd < 0) {
        fprintf(stderr, "stonegui: inotify_init1 failed (%s); hot reload disabled\n", strerror(errno));
        return;
    }
    int wd = inotify_add_watch(g_inotify_fd, g_bundle_dir,
                               IN_CLOSE_WRITE | IN_MOVED_TO | IN_MODIFY);
    if (wd < 0) {
        fprintf(stderr, "stonegui: inotify_add_watch(%s) failed (%s); hot reload disabled\n",
                g_bundle_dir, strerror(errno));
        close(g_inotify_fd);
        g_inotify_fd = -1;
        return;
    }
    fprintf(stderr, "stonegui: hot reload watching %s/%s (pass --no-watch to disable)\n",
            g_bundle_dir, g_bundle_name);
}

static void check_reload(void) {
    if (g_inotify_fd < 0) return;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t n;
    while ((n = read(g_inotify_fd, buf, sizeof(buf))) > 0) {
        for (char *p = buf; p < buf + n; ) {
            struct inotify_event *ev = (struct inotify_event *)p;
            if (ev->len > 0 && strcmp(ev->name, g_bundle_name) == 0) {
                g_should_reload = 1;
            }
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
}

/* ── LVGL tick source ───────────────────────────────────────────────────── */

static uint32_t millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ── UTF-8 aware keyboard read (CJK / multi-byte input) ─────────────────────
 *
 * LVGL's SDL keyboard driver receives full UTF-8 strings from SDL_TEXTINPUT
 * (IME-composed text), but its default read callback emits the buffer one BYTE
 * at a time, which splits multi-byte characters (e.g. Chinese) into invalid
 * single bytes. We replace the read callback with one that emits a whole UTF-8
 * character per keypress, packed little-endian into data->key — the exact
 * layout lv_textarea_add_char() expects.
 *
 * The driver's private struct begins with `char buf[KEYBOARD_BUFFER_SIZE]`
 * (the first member), followed by a `bool` flag; we mirror that layout.
 */
typedef struct {
    char buf[KEYBOARD_BUFFER_SIZE];
    bool dummy_read;
} sg_sdl_kb_t;

static int utf8_seq_len(unsigned char c) {
    if (c < 0x80) return 1;            /* ASCII / LV_KEY_* control codes */
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;                          /* lone continuation byte: skip one */
}

static void sg_keyboard_read(lv_indev_t *indev, lv_indev_data_t *data) {
    sg_sdl_kb_t *dev = lv_indev_get_driver_data(indev);
    if (!dev) return;

    size_t len = strlen(dev->buf);

    if (dev->dummy_read) {
        dev->dummy_read = false;
        data->state = LV_INDEV_STATE_RELEASED;
    } else if (len > 0) {
        int n = utf8_seq_len((unsigned char)dev->buf[0]);
        if ((size_t)n > len) n = (int)len;

        uint32_t key = 0;
        for (int i = 0; i < n; i++)
            key |= (uint32_t)(unsigned char)dev->buf[i] << (8 * i);

        dev->dummy_read = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key   = key;

        memmove(dev->buf, dev->buf + n, len - n + 1); /* keep NUL terminator */
    }
}

/* ── load a JS module file ──────────────────────────────────────────────── */

static int load_and_run(JSContext *ctx, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "stonegui: cannot open '%s'\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    JSValue val = JS_Eval(ctx, buf, len, path,
                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(buf);
    if (JS_IsException(val)) {
        js_std_dump_error(ctx);
        return -1;
    }

    /* link and evaluate the module */
    JSValue promise = JS_EvalFunction(ctx, val);
    if (JS_IsException(promise)) {
        js_std_dump_error(ctx);
        return -1;
    }
    js_std_loop(ctx);   /* drain initial micro-task queue */
    JS_FreeValue(ctx, promise);
    return 0;
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *bundle = BUNDLE_PATH;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-watch") == 0) g_enable_hot_reload = 0;
        else                                    bundle = argv[i];
    }

    /* Install signal handlers EARLY so a SIGINT during init still stops us
     * cleanly. Also tell SDL NOT to install its own SIGINT/SIGTERM handlers
     * — by default SDL silently swallows them (pushes SDL_QUIT) and the
     * combination with our own handler reliably hangs the process on
     * shutdown. Hint must be set before SDL_Init, which lv_sdl_window_create
     * calls below. */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    /* 1. Initialise LVGL */
    lv_init();

    /* 2. Create SDL display */
    lv_display_t *disp = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!disp) {
        fprintf(stderr, "stonegui: failed to create SDL window\n");
        return 1;
    }
    lv_display_set_default(disp);
    lv_sdl_window_set_resizeable(disp, true);

    SDL_AddEventWatch(sdl_event_watch, NULL);

    /* Install stonegui's default look & feel (Flutter/Material-like) */
    sg_theme_init(disp, NULL);

    /* 3. Input devices */
    lv_indev_t *mouse = lv_sdl_mouse_create();
    (void)mouse;
    lv_indev_t *kb = lv_sdl_keyboard_create();
    /* Replace the byte-at-a-time read with a UTF-8 aware one (CJK input) */
    lv_indev_set_read_cb(kb, sg_keyboard_read);

    /* Keyboard focus group: keyboard events route to the focused widget */
    lv_group_t *group = lv_group_create();
    lv_group_set_default(group);
    lv_indev_set_group(kb, group);
    lv_bindings_set_group(group);

    /* 4. QuickJS runtime */
    JSRuntime *rt = JS_NewRuntime();
    js_std_set_worker_new_context_func(NULL);
    js_std_init_handlers(rt);

    JSContext *ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, 0, NULL);
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    /* ES module loader so relative imports ("../../js/framework.js") resolve */
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader,
                            js_module_check_attributes, NULL);

    /* 5. Native LVGL module */
    lv_bindings_register(ctx);

    /* 6. Load and execute the application bundle */
    if (load_and_run(ctx, bundle) != 0) {
        fprintf(stderr, "stonegui: failed to load bundle: %s\n", bundle);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return 1;
    }

    printf("stonegui: running %s\n", bundle);

    setup_hot_reload(bundle);

    /* 7. Event loop */
    uint32_t last_tick = millis();
    while (g_running) {
        /* LVGL tick */
        uint32_t now  = millis();
        uint32_t diff = now - last_tick;
        last_tick     = now;
        lv_tick_inc(diff);

        /* Handle LVGL tasks (SDL events are polled inside) */
        uint32_t sleep_ms = lv_timer_handler();

        /* Flush any pending JS microtasks / Promise callbacks */
        lv_bindings_flush_callbacks(ctx);

        /* Hot reload: if the bundle file changed since we last looked, tear
         * down all widgets + the JS runtime (LV_EVENT_DELETE handlers must
         * fire BEFORE ctx is freed, hence lv_obj_clean first), then rebuild
         * a fresh runtime and re-run the bundle. */
        check_reload();
        if (g_should_reload) {
            g_should_reload = 0;
            printf("stonegui: hot reload\n");

            lv_obj_clean(lv_screen_active());
            js_std_free_handlers(rt);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);

            rt = JS_NewRuntime();
            js_std_set_worker_new_context_func(NULL);
            js_std_init_handlers(rt);
            ctx = JS_NewContext(rt);
            js_std_add_helpers(ctx, 0, NULL);
            js_init_module_std(ctx, "std");
            js_init_module_os(ctx, "os");
            JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader,
                                    js_module_check_attributes, NULL);
            lv_bindings_register(ctx);

            if (load_and_run(ctx, bundle) != 0)
                fprintf(stderr, "stonegui: hot reload bundle load failed; screen is empty until the file is saved again\n");
        }

        /* Yield to OS */
        if (sleep_ms > 0 && sleep_ms < 32)
            usleep(sleep_ms * 1000);
        else
            usleep(5000);
    }

    printf("stonegui: shutting down\n");

    /* Tear-down order matters:
     *   1. lv_deinit() destroys every widget, firing LV_EVENT_DELETE handlers
     *      registered by lv_bindings.c which free the JS callback values.
     *      Must run while ctx is still alive.
     *   2. Free QuickJS helpers + runtime.
     *   3. Close SDL (LVGL's display handle is now stale but unused).
     */
    lv_deinit();
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    if (g_inotify_fd >= 0) close(g_inotify_fd);
    SDL_Quit();

    /* Unix convention: 128 + signo for signal-triggered exit, 0 for clean */
    return g_exit_signal ? 128 + g_exit_signal : 0;
}
