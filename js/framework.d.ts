/**
 * Type declarations for stonegui's `js/framework.js`.
 *
 * Picked up automatically by VS Code / TS-aware editors for IntelliSense
 * on plain `.js` and `.jsx` files. Not required at runtime.
 */

declare module "lvgl" {
    export function getScreen(): number;
    export function createNode(type: string): number;
    export function appendChild(parent: number, child: number): void;
    export function removeChild(parent: number, child: number): void;
    export function setProperty(
        node: number,
        key: string,
        value: unknown,
        state?: "hover" | "focus" | "pressed" | "disabled",
    ): void;
    export function getProperty(node: number, key: string): unknown;
    export function addEvent(node: number, event: string, cb: (value?: unknown) => void): void;
    export function dispose(node: number): void;
    export function loadFont(path: string, size: number): number;
    export function loadFontSizes(path: string, sizes: number[]): Record<number, number>;
    export function setDefaultFont(handle?: number): void;
    export function findCjkFontPath(): string | null;
    export function loadImage(path: string): number;
    export function loadImages(paths: string[]): number[];
    export function addTab(tabview: number, title: string): number;
    export function listAddButton(list: number, text: string): number;
    export function menuAddPage(menu: number, title: string): number;
    export function menuSetPage(menu: number, page: number): void;
    export function chartAddSeries(chart: number, color: string): number;
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

/** Load a TTF/TTC font at a fixed pixel size. Returns `0` on failure. */
export function loadFont(path: string, size: number): number;
export function loadFontSizes(path: string, sizes: number[]): Record<number, number>;
export function findCjkFont(): string | null;
/** Make a loaded font the global default (call after startup). */
export function setDefaultFont(handle?: number): void;

/** Opaque integer handle returned by `loadImage` / `loadImages`. */
export type ImageHandle = number;
/** Validate + normalize a filesystem path into a reusable image handle.
 *  Returns `0` if the file can't be opened. Pass directly to `<image src>`,
 *  inside `<animimg src={[...]}>`, or to `<imagebutton released={...}/>`. */
export function loadImage(path: string): ImageHandle;
/** Batch helper — returns one handle per input path (0 for failures). */
export function loadImages(paths: string[]): ImageHandle[];
/** Read current widget state — typically inside an event handler. */
export function getProperty(node: number, key: "value" | "checked" | "text"): unknown;

export function chartAddSeries(chart: number, color: Color): number;
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

export function setTheme(scheme: "light" | "dark"): void;
export function setThemeToken(name: "primary"|"primary_dark"|"on_primary"|"secondary"|"bg"|"surface"|"on_surface"|"on_variant"|"outline"|"track"|"danger"|"warning", color: Color): void;

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
/** Number = px; "NN%" = percent of parent; "fill" = 100%. */
export type Size = number | `${number}%` | "fill";

export type PseudoState = "hover" | "focus" | "pressed" | "disabled";

export interface StyleProps {
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
    backgroundColor?: Reactive<Color>;
    borderRadius?:    Reactive<number>;
    borderWidth?:     Reactive<number>;
    borderColor?:     Reactive<Color>;
    textColor?:       Reactive<Color>;
    fontSize?:        Reactive<14 | 16 | 20 | 24>;
    font?:            Reactive<number>;
    scrollable?:      Reactive<boolean>;

    hover?:    StyleProps;
    focus?:    StyleProps;
    pressed?:  StyleProps;
    disabled?: StyleProps;
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
