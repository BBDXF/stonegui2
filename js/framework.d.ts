/**
 * Type declarations for stonegui's `js/framework.js`.
 *
 * Picked up automatically by VS Code / TS-aware editors for IntelliSense
 * on plain `.js` and `.jsx` files. Not required at runtime.
 */

declare module "lvgl" {
    /* Mirrors the `lv_funcs` export list in `src/lv_bindings.c`. Deliberately
     * omitted: `getParent`, `getChild` and `createThemeCoverageMenuInternals`,
     * which exist only for the theme-coverage assertions in `examples/test`
     * and are not part of the app-facing surface. There is no native
     * `loadFontSizes` — that helper is pure JS in `framework.js`. */
    export function getScreen(): number;
    export function createNode(type: string): number;
    export function appendChild(parent: number, child: number): void;
    export function removeChild(parent: number, child: number): void;
    /** Throws `TypeError` for an unknown state or an unusable selector. */
    export function setProperty(
        node: number,
        key: string,
        value: unknown,
        selector?: PseudoState | StyleSelector,
    ): void;
    /** Raw, UNVALIDATED getter. Reads `selector.part` / `selector.state` if
     *  present but does not reject bad ones, and returns `undefined` for a key
     *  it does not know. Use the framework `getProperty` wrapper to get the
     *  `TypeError`s. */
    export function getProperty(node: number, key: string, selector?: {
        part?: string;
        state?: string;
    }): unknown;
    export function updateLayout(node: number): void;
    export function addEvent(node: number, event: string, cb: (value?: unknown) => void): void;
    export function dispose(node: number): void;
    export function loadFont(path: string, size: number): number;
    export function setDefaultFont(
        handle?: number | Partial<Record<14 | 16 | 20 | 24, number>>,
    ): void;
    export function findCjkFontPath(): string | null;
    /** Throws `TypeError` for an unknown name or for an integer-kind token. */
    export function getThemeToken(name: ColorThemeTokenName): Color;
    /** The integer half of the registry. Throws `TypeError` for an unknown
     *  name or for a colour-kind token. */
    export function getThemeMetric(name: IntegerThemeTokenName): number;
    export function setTheme(scheme: ThemeScheme): void;
    /** Throws `TypeError` for an unknown name or a value of the wrong kind. */
    export function setThemeToken(name: ColorThemeTokenName, value: Color): void;
    export function setThemeToken(name: IntegerThemeTokenName, value: number): void;
    export function loadImage(path: string): number;
    export function loadImages(paths: string[]): number[];
    export function focus(node: number): boolean;
    export function sendKey(key: string, ctrl?: boolean): boolean;
    export function sendEvent(node: number, event: "click" | "released" | "change"): boolean;
    export function clipboardRead(): string | null;
    export function clipboardWrite(text: string): void;
    export function addTab(tabview: number, title: string): number;
    export function listAddButton(list: number, text: string): number;
    export function menuAddPage(menu: number, title: string): number;
    export function menuSetPage(menu: number, page: number): void;
    export function chartAddSeries(chart: number, color: string): number;
    export function chartSetSeriesColor(chart: number, series: number, color: string): void;
    export function chartSetData(chart: number, series: number, data: number[]): void;
    export function showMsgbox(
        opts: { title?: string; text?: string; buttons?: string[] },
        onClose: (idx: number) => void,
    ): number;
    export function createAnimation(
        node: number,
        opts: {
            property: "x" | "y" | "width" | "height" | "opacity" | "rotation" | "scale" | "value";
            from: number;
            to: number;
            duration?: number;
            easing?: "linear" | "ease-in" | "ease-out" | "ease-in-out" | "overshoot" | "bounce" | "step";
            delay?: number;
            repeat?: number | "infinite";
            onComplete?: () => void;
        },
    ): void;
}

/* ── Reactivity ──────────────────────────────────────────────────────── */

/** Read accessor for a signal: calling it inside an effect tracks. */
export type Signal<T> = () => T;
/** Setter: pass a value or an updater `(prev) => next`. Returns the new value. */
export type Setter<T> = (next: T | ((prev: T) => T)) => T;

/** A value that may be reactive: either a plain `T` or a thunk `() => T`. */
export type Reactive<T> = T | (() => T);

export function createSignal<T>(init: T): [Signal<T>, Setter<T>];
export function createSignal<T>(): [Signal<T | undefined>, Setter<T | undefined>];

export function createEffect(fn: () => void): void;

export function createMemo<T>(fn: () => T): Signal<T>;

export function createRoot<T>(fn: (dispose: () => void) => T): T;

export function onCleanup(fn: () => void): void;
export function untrack<T>(fn: () => T): T;
export function batch<T>(fn: () => T): T;

/* ── Native helpers ──────────────────────────────────────────────────── */

/** Load a TTF/TTC font at a fixed pixel size. Returns `0` on failure.
 *  Handles are cached per `(path, size)` and the file bytes are shared by
 *  path, so loading four sizes reads the file once and never frees it. */
export function loadFont(path: string, size: number): number;
/** Load several sizes of one file. Pure JS over `loadFont`. A size that
 *  fails to load is OMITTED from the result, so every lookup is
 *  possibly-undefined — never `0`. */
export function loadFontSizes(path: string, sizes: number[]): Partial<Record<number, number>>;
export function findCjkFont(): string | null;
/** Install the default typography roles. With no argument it auto-discovers
 *  a CJK font and loads the 14/16/20/24 role sizes; pass a handle to use one
 *  face everywhere, or a partial size→handle map to set roles individually.
 *  A no-op when no font is found — call it after startup, before mounting. */
export function setDefaultFont(
    handle?: number | Partial<Record<14 | 16 | 20 | 24, number>>,
): void;

/** Opaque integer handle returned by `loadImage` / `loadImages`. */
export type ImageHandle = number;
/** Validate + normalize a filesystem path into a reusable image handle.
 *  Returns `0` if the file can't be opened. Pass directly to `<image src>`,
 *  inside `<animimg src={[...]}>`, or to `<imagebutton released={...}/>`. */
export function loadImage(path: string): ImageHandle;
/** Batch helper — returns one handle per input path (0 for failures). */
export function loadImages(paths: string[]): ImageHandle[];
/** Read current widget state — typically inside an event handler. The
 *  geometry keys ("width"/"height"/"x"/"y"/"visible") need a preceding
 *  {@link updateLayout} call, otherwise they read the pre-layout 0.
 *  Resolved style keys accept a part/state selector; `padding` reports the
 *  resolved top padding (`pad_top`). Widget-state keys return `undefined`
 *  on a widget class that does not carry them. */
export interface GetPropertyResults {
    value:            number | undefined;
    checked:          boolean;
    text:             string | undefined;
    cursorPos:        number | undefined;
    target:           number | undefined;
    scrollable:       boolean;
    visible:          boolean;
    width:            number;
    height:           number;
    x:                number;
    y:                number;
    backgroundColor:  Color;
    textColor:        Color;
    borderColor:      Color;
    borderWidth:      number;
    outlineColor:     Color;
    outlineWidth:     number;
    radius:           number;
    padding:          number;
    shadowWidth:      number;
    shadowOpa:        number;
    bgOpa:            number;
    imageOpa:         number;
    textOpa:          number;
    borderOpa:        number;
    lineColor:        Color;
    lineWidth:        number;
    arcColor:         Color;
    arcWidth:         number;
    fontLineHeight:   number;
}
export type GetPropertyKey = keyof GetPropertyResults;
/** Resolved-style selector. Every listed part and state is accepted by the
 *  native parser (`parse_part` / `parse_resolved_state`). Anything else is
 *  rejected by the `framework.js` wrappers with a `TypeError` — the raw
 *  `lvgl` module does not validate, it falls back to an unspecified
 *  selector. */
export interface StyleSelector {
    readonly part?: StylePart;
    readonly state?: ResolvedState;
}
export function getProperty<K extends GetPropertyKey>(
    node: number,
    key: K,
    selector?: StyleSelector,
): GetPropertyResults[K];

/** Flush pending layout for a subtree so the geometry getters return final
 *  coordinates. LVGL otherwise recomputes positions only inside its timer
 *  handler, so measuring straight after mount sees everything at 0,0. */
export function updateLayout(node: number): void;

export function chartAddSeries(chart: number, color: Color): number;
/** Repaints an existing series. `chartAddSeries` copies the colour into the
 *  series, so a theme repaint alone never reaches an already-created one. */
export function chartSetSeriesColor(chart: number, series: number, color: Color): void;
export function chartSetData(chart: number, series: number, data: number[]): void;

export interface MsgboxOpts {
    title?: string;
    text?: string;
    buttons?: string[];
}
export function showMsgbox(opts: MsgboxOpts, onClose?: (idx: number) => void): number;

export interface AnimationOpts {
    property: "x" | "y" | "width" | "height" | "opacity" | "rotation" | "scale" | "value";
    from: number;
    to: number;
    duration?: number;
    easing?: "linear" | "ease-in" | "ease-out" | "ease-in-out" | "overshoot" | "bounce" | "step";
    delay?: number;
    repeat?: number | "infinite";
    onComplete?: () => void;
}
export function createAnimation(node: number, opts: AnimationOpts): void;

/** Switch the displayed page of a `<menu>` imperatively. Pages are usually
 *  declared with `<menuPage title="..." ref={(p) => myRef = p}>` so you can
 *  pass that handle here from a button's onClick. */
export function setMenuPage(menu: number, page: number): void;

export declare const clipboard: {
    read(): string | null;
    write(text: string): void;
};

/** Absolute directory of the module whose `import.meta.url` is passed in —
 *  the CWD-independent way for a bundle to locate its own assets. */
export function moduleDir(importMetaUrl: string): string;

/** Move keyboard focus to a widget in the global focus group. */
export function focus(node: number): boolean;

/** Inject an SDL key press ("a".."z", "home", "end") for smoke tests and
 *  automation. Ctrl combos drive the `<input>` clipboard shortcuts. */
export function sendKey(key: string, ctrl?: boolean): boolean;

/** Swap the whole live token set. Every mounted widget repaints in place;
 *  nothing is remounted and JS local styles survive. */
export type ThemeScheme = "light" | "dark";
export function setTheme(scheme: ThemeScheme): void;

/** Integer-valued tokens (geometry, spacing, stroke, elevation, opacity).
 *  `radius_btn` / `radius_field` default to `radius_base` but are patchable
 *  independently. */
export type IntegerThemeTokenName =
    | "radius_base" | "radius_small" | "radius_round" | "border_width"
    | "space_xs" | "space_sm" | "space_md" | "space_lg" | "space_xl" | "control_height"
    | "slider_track_size" | "slider_knob_size" | "arc_width" | "scrollbar_size"
    | "shadow_small_width" | "shadow_overlay_width" | "shadow_opa" | "disabled_opa"
    | "overlay_mask_opa" | "btn_pad_hor" | "btn_pad_ver"
    | "radius_btn" | "radius_field";
export type ThemeTokenName = ColorThemeTokenName | IntegerThemeTokenName;
/** Patch one live token. Colour tokens take a {@link Color}, integer tokens
 *  take a number; the pairing is checked here and again at runtime, where a
 *  mismatched kind or unknown name throws a `TypeError`. */
export function setThemeToken(name: ColorThemeTokenName, value: Color): void;
export function setThemeToken(name: IntegerThemeTokenName, value: number): void;

/* ── View layer ──────────────────────────────────────────────────────── */

/** Opaque VNode produced by `h()`. Mount with `render()`. */
export interface VNode { readonly __vnode: true; }

export const Fragment: unique symbol;
export type Component<P = {}> = (props: P & { children?: unknown[] }) => VNode;

/** JSX factory. Use either lowercase host tags (see `IntrinsicElements`)
 *  or pass a Component function for capitalised tags. */
export function h(type: string | Component<any> | typeof Fragment, props?: object | null, ...children: unknown[]): VNode;

/** Mount the tree once. Returns a function that disposes the entire subtree
 *  (frees every effect and native widget created during mount). */
export function render(fn: () => VNode | VNode[] | void, containerNative?: number): () => void;

/* ── Show / For ──────────────────────────────────────────────────────── */

export interface ShowProps {
    when: Reactive<unknown>;
    fallback?: VNode | (() => VNode);
    children?: VNode | (() => VNode) | Array<VNode | (() => VNode)>;
}
/** Conditionally mount children. Use a function child `() => <X/>` to get
 *  per-mount component lifecycle (onCleanup re-fires on toggle). */
export const Show: Component<ShowProps>;

export interface ForProps<T> {
    each: Reactive<readonly T[]>;
    key?: (item: T, index: number) => string | number;
    children: (item: T, index: number) => VNode;
}
/** Keyed list. Existing items keep their state across re-renders; only
 *  added/removed items are mounted/disposed. */
export const For: <T>(props: ForProps<T>) => VNode;

/* ── Styles ──────────────────────────────────────────────────────────── */

/** "#rrggbb" / "#rrggbbaa" / named ("red","blue","gray", etc.). See parse_color_ex. */
export type Color = string;
export type ColorThemeTokenName =
    | "primary.base" | "primary.light_3" | "primary.light_5" | "primary.light_7" | "primary.light_9" | "primary.dark_2"
    | "success.base" | "success.light_3" | "success.light_5" | "success.light_7" | "success.light_9" | "success.dark_2"
    | "warning.base" | "warning.light_3" | "warning.light_5" | "warning.light_7" | "warning.light_9" | "warning.dark_2"
    | "danger.base" | "danger.light_3" | "danger.light_5" | "danger.light_7" | "danger.light_9" | "danger.dark_2"
    | "error.base" | "error.light_3" | "error.light_5" | "error.light_7" | "error.light_9" | "error.dark_2"
    | "info.base" | "info.light_3" | "info.light_5" | "info.light_7" | "info.light_9" | "info.dark_2"
    | "text_primary" | "text_regular" | "text_secondary" | "text_placeholder" | "text_disabled"
    | "border_base" | "border_light" | "border_lighter" | "border_extra_light" | "border_dark" | "border_darker"
    | "fill_base" | "fill_light" | "fill_lighter" | "fill_extra_light" | "fill_dark" | "fill_darker" | "fill_blank"
    | "bg_page" | "bg_base" | "bg_overlay" | "overlay_mask" | "white" | "black"
    | "primary" | "primary_dark" | "on_primary" | "secondary" | "bg" | "surface"
    | "on_surface" | "on_variant" | "outline" | "track" | "danger" | "warning";
/** `"$<colour token>"` resolves live: the widget repaints on every
 *  `setTheme` / `setThemeToken`. Only `backgroundColor`, `borderColor` and
 *  `textColor` resolve references; elsewhere a `$…` string stays literal.
 *  A name outside {@link ColorThemeTokenName} throws a `TypeError` at mount,
 *  and a template-literal string built at runtime is not checked here. */
export type ThemeColorReference = `$${ColorThemeTokenName}`;
export type StyleColor = Color | ThemeColorReference;
/** Number = px; "NN%" = percent of parent; "fill" = 100%. */
export type Size = number | `${number}%` | "fill" | "auto";

/** States a `style` object may nest. `default` re-states the base state
 *  explicitly; `checked` targets toggled switches/checkboxes/buttons. */
export type PseudoState = "default" | "hover" | "focus" | "pressed" | "checked" | "disabled";
/** Extra read-only states the resolved getters accept on top of the
 *  writable {@link PseudoState} set. */
export type ResolvedState = PseudoState | "focusKey" | "edited" | "scrolled";
export type StylePart = "main" | "scrollbar" | "indicator" | "knob" | "selected"
    | "items" | "cursor" | "placeholder";

export interface StyleValues {
    width?:           Reactive<Size>;
    height?:          Reactive<Size>;
    x?:               Reactive<number>;
    y?:               Reactive<number>;
    flexFlow?:        Reactive<"row" | "column">;
    flexGrow?:        Reactive<number>;
    gap?:             Reactive<number>;
    alignItems?:      Reactive<"start" | "center" | "end" | "between" | "around" | "evenly">;
    justifyContent?:  Reactive<"start" | "center" | "end" | "between" | "around" | "evenly">;
    padding?:         Reactive<number>;
    backgroundColor?: Reactive<StyleColor>;
    borderRadius?:    Reactive<number>;
    borderWidth?:     Reactive<number>;
    borderColor?:     Reactive<StyleColor>;
    textColor?:       Reactive<StyleColor>;
    fontSize?:        Reactive<14 | 16 | 20 | 24>;
    font?:            Reactive<number>;
    scrollable?:      Reactive<boolean>;
}

/** Base-state values plus at most one level of pseudo-state nesting.
 *  Nesting a state inside a state is rejected at runtime with a `TypeError`. */
export type PartStyleProps = StyleValues & {
    readonly [S in PseudoState]?: StyleValues;
};

export interface StyleProps extends PartStyleProps {
    /** Per-part overrides. Distinct from `SpanProps.parts`, which is span
     *  text runs. An unlisted part name throws a `TypeError` at mount. */
    partStyles?: Partial<Record<StylePart, PartStyleProps>>;
}

/* ── Widget props (per host tag) ─────────────────────────────────────── */

export interface CommonProps {
    style?:        StyleProps;
    ref?:          (node: number) => void;
    onClick?:      () => void;
    onLongPress?:  () => void;
    onFocus?:      () => void;
    onBlur?:       () => void;
    children?:     unknown;
}

export interface ViewProps      extends CommonProps {}
export interface TextProps      extends CommonProps { text?: Reactive<string>; }
export interface ButtonProps    extends CommonProps { text?: Reactive<string>; }
export interface ImageProps     extends CommonProps { src?:  Reactive<string | ImageHandle>; }
export interface InputProps     extends CommonProps {
    placeholder?: Reactive<string>;
    oneLine?:     Reactive<boolean>;
    maxLength?:     Reactive<number>;
    acceptedChars?: Reactive<string>;
    password?:      Reactive<boolean>;
    align?:         Reactive<"left" | "center" | "right">;
    textSelection?: Reactive<boolean>;
    cursorPos?:     Reactive<number>;
    onChange?:    (text: string) => void;
}
export interface SwitchProps    extends CommonProps { checked?: Reactive<boolean>; onChange?: (checked: boolean) => void; }
export interface ProgressProps  extends CommonProps { value?: Reactive<number>; min?: Reactive<number>; max?: Reactive<number>; }
export interface SliderProps    extends ProgressProps { onChange?: (value: number) => void; }
export interface ArcProps       extends SliderProps {}
export interface SpinnerProps   extends CommonProps { spinTime?: Reactive<number>; arcAngle?: number; }
export interface CheckboxProps  extends CommonProps { text?: Reactive<string>; checked?: Reactive<boolean>; onChange?: (checked: boolean) => void; }
export interface DropdownProps  extends CommonProps { options?: Reactive<string>; value?: Reactive<number>; onChange?: (selectedIndex: number) => void; }
export interface RollerProps    extends DropdownProps {}
export interface TabviewProps   extends CommonProps {
    tabBarSize?:     Reactive<number>;
    tabBarPosition?: Reactive<"top" | "bottom" | "left" | "right">;
    activeTab?:      Reactive<number>;
    onChange?:       (tabIndex: number) => void;
}
export interface TabProps       extends CommonProps { title?: string; }
export interface ListProps      extends CommonProps {}
export interface ListButtonProps extends CommonProps { text?: string; }
export interface SpinboxProps   extends CommonProps {
    value?:  Reactive<number>;
    digits?: Reactive<string>;
    step?:   Reactive<number>;
    onChange?: (value: number) => void;
}
export interface LEDProps       extends CommonProps { color?: Reactive<Color>; brightness?: Reactive<number>; }
export interface ChartProps     extends CommonProps {
    chartType?:  Reactive<"line" | "bar" | "scatter" | "none">;
    pointCount?: Reactive<number>;
    rangeMin?:   Reactive<number>;
    rangeMax?:   Reactive<number>;
    divLines?:   Reactive<string>;
}
export interface ButtonMatrixProps extends CommonProps {
    map?:        readonly string[];
    oneChecked?: Reactive<boolean>;
    onChange?:   (buttonIndex: number) => void;
}
export interface CalendarProps  extends CommonProps {
    today?:    Reactive<string>;
    shown?:    Reactive<string>;
    arrowHeader?: boolean;
    onChange?: (date: { year: number; month: number; day: number }) => void;
}
export interface ScaleProps     extends CommonProps {
    scaleMode?:  Reactive<"h-top" | "h-bottom" | "v-left" | "v-right" | "round-inner" | "round-outer">;
    totalTicks?: Reactive<number>;
    majorEvery?: Reactive<number>;
    showLabels?: Reactive<boolean>;
    min?:        Reactive<number>;
    max?:        Reactive<number>;
}
export interface SpanProps      extends CommonProps {
    parts?: readonly { text: string; color?: Color; fontSize?: number; font?: number }[];
}
export interface LineProps      extends CommonProps {
    /** Array of `[x, y]` pixel pairs joined into a polyline. */
    points?: readonly [number, number][];
}
export interface TableProps     extends CommonProps {
    rows?:  Reactive<number>;
    cols?:  Reactive<number>;
    /** Row-major 2-D string array; resizes `rows` / `cols` to fit. */
    cells?: readonly (readonly string[])[];
}
export interface MenuProps      extends CommonProps {}
export interface MenuPageProps  extends CommonProps {
    title?: string;
}
export interface KeyboardProps  extends CommonProps {
    target?: number;
    mode?:   Reactive<"text-lower" | "text-upper" | "number" | "special">;
}
export interface AnimImgProps   extends CommonProps {
    /** Frame sources — handles (preferred) or raw `"A:/..."` paths. */
    src?:      Reactive<(string | ImageHandle)[]>;
    /** Cycle duration in ms. */
    duration?: Reactive<number>;
    /** Number of cycles, or `"infinite"`. */
    repeat?:   Reactive<number | "infinite">;
    /** Set true to (re)start the animation. */
    start?:    Reactive<boolean>;
}
export interface ImageButtonProps extends CommonProps {
    released?:        Reactive<string | ImageHandle>;
    pressed?:         Reactive<string | ImageHandle>;
    disabled?:        Reactive<string | ImageHandle>;
    checkedReleased?: Reactive<string | ImageHandle>;
    checkedPressed?:  Reactive<string | ImageHandle>;
    checkedDisabled?: Reactive<string | ImageHandle>;
    /** Add `LV_OBJ_FLAG_CHECKABLE` so clicks toggle the checked state. */
    checkable?:       Reactive<boolean>;
}

/* ── JSX intrinsics ──────────────────────────────────────────────────── */

declare global {
    namespace JSX {
        type Element = VNode;
        interface IntrinsicElements {
            view:         ViewProps;
            text:         TextProps;
            button:       ButtonProps;
            image:        ImageProps;
            input:        InputProps;
            switch:       SwitchProps;
            progress:     ProgressProps;
            slider:       SliderProps;
            arc:          ArcProps;
            spinner:      SpinnerProps;
            checkbox:     CheckboxProps;
            dropdown:     DropdownProps;
            roller:       RollerProps;
            tabview:      TabviewProps;
            tab:          TabProps;
            list:         ListProps;
            listButton:   ListButtonProps;
            spinbox:      SpinboxProps;
            led:          LEDProps;
            chart:        ChartProps;
            buttonMatrix: ButtonMatrixProps;
            calendar:     CalendarProps;
            scale:        ScaleProps;
            span:         SpanProps;
            line:         LineProps;
            table:        TableProps;
            menu:         MenuProps;
            menuPage:     MenuPageProps;
            keyboard:     KeyboardProps;
            animimg:      AnimImgProps;
            imagebutton:  ImageButtonProps;
        }
    }
}
