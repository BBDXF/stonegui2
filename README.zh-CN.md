[English](README.md) | **简体中文**

# stonegui

一个现代的声明式嵌入式 GUI 框架：**QuickJS + LVGL**，直接渲染为原生 LVGL 控件
—— 没有 XML，没有 HTML，也没有序列化的 UI 树。

你用 JavaScript 编写 React-Native 风格的组件，渲染器把它们直接映射成 LVGL 的
API 调用。

```js
h("Text", { text: () => `Count: ${count()}` })
//                         ↓ on signal change
// lv_label_set_text(...)      // a single property update, not a remount
```

> **项目状态 —— 已于 2026-09 停止扩展功能面。** 现有内容可运行且有回归覆盖，
> 但不再朝"通用桌面 GUI 框架"方向继续开发。瓶颈、根因与后续方案对比见
> [`doc/retrospective.md`](doc/retrospective.md)。

最初的（且已部分过时的）设计构想见 [`doc/prompts.md`](doc/prompts.md)；本文档的
其余部分反映的是当前实际交付的内容。

## 截图

`examples/showcase` —— 同一个界面在两套配色下的样子。`setTheme("dark")` 会就地
重建共享样式并重绘每一个控件；没有任何控件被重新挂载，内联样式也不会丢失。

| 浅色 | 深色 |
| --- | --- |
| ![showcase, light scheme](doc/screenshots/showcase-light.png) | ![showcase, dark scheme](doc/screenshots/showcase-dark.png) |

| 图表与数据类控件 | `examples/jsx` —— 通过真实 JSX 使用同一套 API |
| --- | --- |
| ![showcase data cards](doc/screenshots/showcase-widgets.png) | ![jsx demo](doc/screenshots/jsx-demo.png) |

## 架构

```text
Application (examples/*)
    ↓
framework.js          fine-grained reactivity (signals + effects + owners)
    ↓  HostRenderer API (createNode / appendChild / setProperty / addEvent / dispose)
lv_bindings.c         native "lvgl" QuickJS module
    ↓
LVGL                  widgets, layout, styles, events, rendering, animation
    ↓
SDL2 (Linux/X11)
```

* **`js/framework.js`** —— 与框架无关的响应式内核。组件树只构建**一次**；每个响应式
  属性值（thunk）各自持有一个 effect，因此一次信号变更只更新单个 LVGL 属性，不会
  重新挂载。owner 树驱动 `<Show>` / `<For>` 及热重载拆除时的 `dispose()`。
* **`src/lv_bindings.c`** —— 以原生 `lvgl` 模块的形式暴露给 JS 的最小
  `HostRenderer` 接口面。
* **`src/main.c`** —— 宿主：启动 LVGL + SDL2 与 QuickJS 运行时（含 ES 模块加载
  器），接好 SIGINT/SIGTERM/SDL_QUIT 的干净退出流程，加载 bundle 并运行事件循环。

## 构建与运行

需要 CMake（≥ 3.16）、C/C++ 工具链，以及 SDL2（`pkg-config sdl2`）。LVGL
**9.2.2** 和 QuickJS **2025-09-13** 会在首次 configure 时由 CMake `FetchContent`
从 GitHub 拉取（约 60–120 秒）；不需要 `git submodule`。

```sh
cmake -S . -B build
cmake --build build
./build/stonegui                       # runs examples/showcase/app.js
./build/stonegui examples/jsx/app.js   # or pass a bundle explicitly
```

构建会把 `js/` 和 `examples/` 拷贝到可执行文件旁边，这样相对导入在运行时才能解析。
如需离线工作或修改上游代码，可用 `-DSTONEGUI_LVGL_SOURCE_DIR=/path` 或
`-DSTONEGUI_QUICKJS_SOURCE_DIR=/path` 覆盖依赖来源。

按 `Ctrl+C` 或关闭窗口即可干净退出（`lv_deinit` → `JS_FreeRuntime` →
`SDL_Quit`，退出码为 `128+signo`）。

本项目没有 CI，也没有回归运行器。要检查一处改动，请自行在仓库根目录运行这些 bundle：

```sh
./build/stonegui --no-watch examples/test/app.js   # 537 assertions, exit 0
STONEGUI_SHOWCASE_SMOKE=1 ./build/stonegui --no-watch examples/showcase/app.js
npx -y -p typescript@5 tsc --strict --target ES2020 --lib ES2020,DOM \
    --noEmit js/framework.d.ts                     # declarations type-check
```

断言 bundle 会打印 `ALL TESTS PASSED`；showcase 冒烟测试会打印
`SHOWCASE SMOKE PASSED`。交互式的 `jsx` / `showcase` bundle 没有退出条件 —— 用
`timeout` 运行它们，并把状态码 143 视为成功。

## 组件（宿主标签）

31 个小写 JSX 宿主标签映射到 LVGL 控件。首字母大写的别名同样可用。权威列表见
[`js/framework.d.ts`](js/framework.d.ts) 与 [`HOST_TAGS`](js/framework.js)。

| 标签 (JSX)     | LVGL 控件           | 说明                                   |
| -------------- | ------------------- | -------------------------------------- |
| `view`         | `lv_obj`            | 透明布局容器                           |
| `text`         | `lv_label`          | 纯文本                                 |
| `button`       | `lv_button`         | 主题化按钮，内联子标签                 |
| `image`        | `lv_image`          | PNG / JPG / BMP，走内置解码器          |
| `input`        | `lv_textarea`       | 单行或多行，支持中日韩输入法           |
| `switch`       | `lv_switch`         | 开关胶囊                               |
| `progress`     | `lv_bar`            | 确定进度条                             |
| `slider`       | `lv_slider`         | 带滑块的可拖动条                       |
| `arc`          | `lv_arc`            | 环形滑块 / 进度                        |
| `spinner`      | `lv_spinner`        | 循环弧形指示器                         |
| `checkbox`     | `lv_checkbox`       | 方形勾选框 + 内联文字                  |
| `dropdown`     | `lv_dropdown`       | 弹出列表，`options="a\nb\nc"`          |
| `roller`       | `lv_roller`         | 滚轮式选择器                           |
| `tabview`      | `lv_tabview`        | 配合 `<tab title="...">` 子元素        |
| `tab`          | `lv_tab`（内部）    | 标签页页面                             |
| `list`         | `lv_list`           | 配合 `<listButton text="...">` 项      |
| `listButton`   | （内部）            | 列表行                                 |
| `spinbox`      | `lv_spinbox`        | 数字步进器；可直接输入数字或用 `↑`/`↓` |
| `led`          | `lv_led`            | 彩色指示灯                             |
| `chart`        | `lv_chart`          | 折线 / 柱状 / 散点；序列经由 `ref` 创建 |
| `buttonMatrix` | `lv_buttonmatrix`   | 按钮网格，`"\n"` 分隔行                |
| `calendar`     | `lv_calendar`       | 带月份导航的日期选择器                 |
| `scale`        | `lv_scale`          | 刻度 / 标签尺                          |
| `span`         | `lv_spangroup`      | 富文本（逐 span 设置颜色 / 字号）      |
| `line`         | `lv_line`           | 由 `points={[[x,y],...]}` 构成的折线   |
| `table`        | `lv_table`          | 网格：`rows`/`cols` + `cells={[[…]]}`  |
| `menu`         | `lv_menu`           | 多页菜单容器                           |
| `menuPage`     | （内部）            | 菜单页；第一页自动激活                 |
| `keyboard`     | `lv_keyboard`       | 屏幕键盘，`target={inputRef}`          |
| `animimg`      | `lv_animimg`        | 多帧图像循环（PNG/JPG/BMP）            |
| `imagebutton`  | `lv_imagebutton`    | 分状态图像按钮（released/pressed/checked） |

`View` 是一个透明、无边框、零内边距的布局盒（React-Native 语义）—— 需要装饰时请
显式设置 `backgroundColor`、`borderRadius`、`padding` 等。

## 响应式

```js
import {
    createSignal, createEffect, createMemo, createRoot,
    onCleanup, untrack, batch,
} from "./js/framework.js";

const [count, setCount] = createSignal(0);
const doubled = createMemo(() => count() * 2);

createEffect(() => {
    console.log("count =", count(), "doubled =", doubled());
    onCleanup(() => console.log("effect torn down"));
});

batch(() => { setCount(1); setCount(2); });   // one effect run, not two
untrack(() => count());                       // read without subscribing
```

owner 构成一棵树：每个 effect 都运行在某个 owner 内部；销毁一个 owner 会执行它的
`onCleanup` 回调并递归销毁其子节点。`render()` 返回的函数就是根 dispose。

## 动态 UI：`<Show>` / `<For>`

```jsx
import { Show, For } from "./js/framework.js";

<Show when={visible}>{() => <Header />}</Show>
<Show when={() => loading()} fallback={() => <Empty />}>
    {() => <List items={items} />}
</Show>

<For each={items} key={(item) => item.id}>
    {(item, index) => <Row data={item} index={index} />}
</For>
```

* `<For>` 是**带 key 的** —— 已存在的项在重新渲染时保留自身状态，只有新增/移除的
  项才会挂载/卸载。子元素必须是一个渲染函数 `(item, index) => VNode`。
* 当你希望每次切换都得到全新的组件实例（以及每次挂载都触发的 `onCleanup`）时，
  `<Show>` 的子元素应当是一个函数 `() => VNode`。静态 VNode 子元素也能工作，但不会
  被重新求值。

## 图像（`<image>`、`<animimg>`、`<imagebutton>`）

LVGL 内置的 `lodepng`、`tjpgd` 和 BMP 解码器均已启用 —— PNG / JPG / BMP 零系统
依赖。POSIX 文件系统驱动注册在盘符 `A:` 上，因此绝对路径写作：

```jsx
<image src="A:/abs/path/to/picture.png" style={{ width: 128, height: 128 }} />
```

### `loadImage` —— 可复用句柄

对于会被多次引用的图像、`<animimg>` 数组中的图像，或 `<imagebutton>` 的各个状态，
建议使用句柄形式：

```js
import { loadImage, loadImages } from "./js/framework.js";

const logo   = loadImage("/abs/path/logo.png");       // 0 if file missing
const frames = loadImages([                            // batch
    "/abs/path/frame1.png",
    "/abs/path/frame2.png",
    "/abs/path/frame3.png",
]);
```

bundle 应当用 `moduleDir(import.meta.url)` 而不是绝对路径来定位自己的资源 ——
可执行文件是相对 CWD 解析相对导入的，而 `moduleDir` 在任何情况下都返回 bundle
的真实目录：

```js
import { moduleDir, loadImage } from "../../js/framework.js";
const ASSETS = moduleDir(import.meta.url);
const logo   = loadImage(`${ASSETS}/logo.png`);
```

`loadImage` 会用 `fopen` 校验文件是否存在，在你传入 `/` 开头的绝对路径时自动补上
`A:` 前缀，并返回一个不透明的整数句柄（与 `loadFont` 一样）。句柄的生命周期与运行时
相同 —— 它们被刻意设计为永不释放（LVGL 会以路径为键缓存已解码的图像）。

`<image src>` 两种形式都接受：

```jsx
<image src={logo} style={{ width: 64, height: 64 }} />     {/* handle */}
<image src="A:/abs/path/icon.png" style={{ width: 32 }} /> {/* string */}
```

### `<animimg>` —— 多帧循环

```jsx
<animimg
    src={frames}
    duration={900}             /* one full cycle, ms */
    repeat="infinite"          /* or a finite count */
    start={true}
    style={{ width: 128, height: 128 }}
/>
```

### `<imagebutton>` —— 分状态 PNG 按钮

```jsx
const released = loadImage("/abs/path/btn-normal.png");
const pressed  = loadImage("/abs/path/btn-pressed.png");

<imagebutton
    released={released}
    pressed={pressed}
    checkable={false}          /* set true so clicks toggle to checked state */
    style={{ width: 128, height: 128 }}
    onClick={() => console.log("click")}
/>
```

六个状态属性接受句柄或字符串：`released`、`pressed`、`disabled`、
`checkedReleased`、`checkedPressed`、`checkedDisabled`。stonegui 绑定的是纯图像
变体 —— LVGL 的 9 宫格三段式 API 未被暴露（没有 `*_mid` / `*_right` 属性）。

整个图像家族（静态 `<image>`、`<animimg>` 循环、分状态 `<imagebutton>`）在
showcase 的「图像家族」卡片中有演示：

```sh
./build/stonegui examples/showcase/app.js       # see the "图像家族" media card
```

测试资源可以用 `examples/showcase/assets/` 目录下相邻的 `make_*.py` 脚本刷新
（只依赖 Python 标准库 —— 它们从 [Twemoji](https://github.com/twitter/twemoji)
CDN 拉取 72×72 的 PNG）。Twemoji 图形采用 CC-BY 4.0 许可，版权归 Twitter Inc
及其他贡献者所有。

## 伪状态样式

```jsx
<button style={{
    width: 120, height: 40, borderRadius: 8,
    backgroundColor: "#3498db",
    hover:    { backgroundColor: "#5dade2" },
    pressed:  { backgroundColor: "#2980b9" },
    focus:    { borderWidth: 2, borderColor: "#ffffff" },
    disabled: { backgroundColor: "#7f8c8d" },
}}>Click me</button>
```

伪状态对象嵌套在 `style` 内部，其条目会带上对应的 LVGL 状态选择器生效。支持的状态
为 `default`、`hover`、`focus`、`pressed`、`checked` 和 `disabled`。嵌套严格只有
一层：在状态对象内部再放一个样式对象会抛出 `TypeError`。

## 部件样式

控件的子部件通过 `partStyles` 设置样式，以部件名为键：

```jsx
<slider style={{
    partStyles: {
        indicator: { backgroundColor: "$primary.base" },
        knob:      { backgroundColor: "$white", pressed: { backgroundColor: "$fill_dark" } },
    },
}} />
```

支持的部件：`main`、`scrollbar`、`indicator`、`knob`、`selected`、`items`、
`cursor`、`placeholder`。每个部件接受与 `style` 相同的基础值，外加一层伪状态嵌套。
未知的部件名会在挂载时抛出 `TypeError`。

`partStyles` 与 `<span parts={…}>` 无关，后者描述的是单个 span 控件内部带样式的
文本片段。

## 命令式逃生口（图表、消息框）

有两样东西不适合「只挂载一次」的声明式树，改为通过函数调用驱动。图表序列由 `ref`
回调创建：

```jsx
import { chartAddSeries, chartSetSeriesColor, chartSetData } from "./js/framework.js";

<chart
    style={{ width: 300, height: 180 }}
    ref={(node) => {
        const series = chartAddSeries(node, "#409EFF");
        chartSetData(node, series, [10, 40, 25, 70, 55]);
        chartSetSeriesColor(node, series, "#67C23A");   // recolour later
    }}
/>
```

`chartSetData` 会替换整个数据点数组。这三个函数接受的是字面颜色（`#rrggbb` 或具名
颜色）—— 它们**不是**样式属性，因此 `"$token"` 引用在这里不会被解析。若想让序列跟随
主题，请先从原生模块读出该 token：

```js
import * as lv from "lvgl";
chartSetSeriesColor(node, series, lv.getThemeToken("primary.base"));
```

`getThemeToken` / `getThemeMetric` 位于 `"lvgl"` 模块上，而不是 `framework.js`。
`chartSetSeriesColor` 之所以存在，是因为 `lv_chart_add_series` 会把颜色复制进序列
结构体，所以之后的主题变更无法触及一个已经创建好的序列。

模态框是按需打开的 —— 它没有声明式形态，因为在一棵只挂载一次的树里没有它的位置：

```js
import { showMsgbox } from "./js/framework.js";

showMsgbox(
    { title: "确认", text: "要删除这一项吗？", buttons: ["取消", "删除"] },
    (index) => console.log("clicked", index),   // onClose is optional
);
```

`onClose` 收到的是被按下按钮的 0 起始索引；若消息框是通过关闭按钮被关掉的，则收到
`-1`。

## 动画

```js
import { createAnimation } from "./js/framework.js";

createAnimation(node, {
    property:   "width",      // x|y|width|height|opacity|rotation|scale|value
    from:       100,
    to:         250,
    duration:   200,          // ms
    easing:     "ease-out",   // linear|ease-in|ease-out|ease-in-out|overshoot|bounce|step
    delay:      0,            // ms
    repeat:     1,            // int or "infinite"
    onComplete: () => {},
});
```

属性由 LVGL 的 `lv_anim` 引擎驱动；`onComplete` 在动画结束时触发（若设置了重复，
则每一轮结束后都会触发）。

## 主题（Element Plus）

stonegui 自带一套完整独立的 Element Plus 主题。它以 `parent = NULL` 安装，因此
stonegui 拥有 LVGL 全部十个主题槽位，不从 LVGL 默认主题继承任何东西。每一条可见
规则（颜色、圆角、间距、描边、投影、排版）都来自 stonegui 自己的 token 集合；你的
内联 `style={…}` 永远优先于它。

在运行时切换配色方案或修补单个 token：

```js
import { setTheme, setThemeToken } from "./js/framework.js";

setTheme("dark");                          // full dark mode
setTheme("light");                         // back to light
setThemeToken("primary.base", "#e74c3c");  // colour token takes a colour
setThemeToken("radius_base", 8);           // integer token takes a number
```

token 是带类型的。颜色 token 是六组语义色阶（`primary` / `success` / `warning` /
`danger`（别名 `error`）/ `info`，每组含 `.base`、`.light_3`、`.light_5`、
`.light_7`、`.light_9`、`.dark_2`），外加中性角色色（`text_primary`、
`text_regular`、`text_secondary`、`text_placeholder`、`text_disabled`，以及
`border_*` / `fill_*` / `bg_*` 系列、`overlay_mask`、`white`、`black`）。整数
token 是各项度量（`radius_base`、`radius_small`、`radius_round`、`border_width`、
`space_xs`…`space_xl`、`control_height`、`slider_track_size`、
`slider_knob_size`、`arc_width`、`scrollbar_size`、`shadow_small_width`、
`shadow_overlay_width`、`shadow_opa`、`disabled_opa`、`overlay_mask_opa`、
`btn_pad_hor`、`btn_pad_ver`、`radius_btn`、`radius_field`）。旧的扁平命名
（`primary`、`primary_dark`、`on_primary`、`secondary`、`bg`、`surface`、
`on_surface`、`on_variant`、`outline`、`track`、`danger`、`warning`）仍作为规范
角色的别名可用。`doc/theme.md` 是带精确十六进制值的权威列表。

`control_height` 是唯一一个已注册、可设置，但当前没有任何主题样式消费的度量 ——
修改它能通过类型检查也能成功执行，却不会重绘任何东西。控件高度目前来自 LVGL 自身
的内容尺寸计算。

未知的 token 名，或类型不匹配的值（给颜色 token 传数字、给整数 token 传颜色），
都会抛出 `TypeError`。完整集合同样声明在 `js/framework.d.ts` 中，因此在支持 TS 的
编辑器里这种不匹配会直接是一个编译错误。

### `style` 中的实时 token 引用

`backgroundColor`、`borderColor` 和 `textColor` 接受 `"$token"` 字符串，它会针对
当前主题解析，并在每次 `setTheme` / `setThemeToken` 后重新解析：

```jsx
<view style={{ backgroundColor: "$bg_base", textColor: "$text_primary" }} />
```

只有这三个属性会解析引用；在其他任何地方 `$…` 字符串都保持为字面量。只有颜色
token 可以被引用，若名称不是颜色 token，则在样式生效时抛出 `TypeError`。

`setTheme` / `setThemeToken` 都会就地重建共享样式并调用
`lv_obj_report_style_change(NULL)` —— 每个控件都会重绘，没有任何东西被重新挂载，
你的局部样式也会保留。在一个 200 控件的界面上这大约耗时 1-2 毫秒；它是为主题切换
设计的，不是为逐帧动画设计的。

### 读取已解析的样式

`getProperty(node, key, selector?)` 会报告主题加上你的内联样式实际解析出的结果，
适用于任意 part/state 组合：

```js
import { getProperty } from "./js/framework.js";

getProperty(slider, "backgroundColor", { part: "knob" });
getProperty(button, "backgroundColor", { state: "disabled" });
```

选择器的 part 就是 `partStyles` 的那些部件；选择器的 state 是各伪状态，外加只读的
`focusKey`、`edited` 和 `scrolled`。未知的 key、part 或 state 会抛出 `TypeError`，
而不是悄悄返回黑色。

## 热重载

默认情况下可执行文件会监视 bundle 的父目录，并在每次保存时重新运行 bundle
（`IN_CLOSE_WRITE | IN_MOVED_TO | IN_MODIFY`）。重载时：所有 LVGL 控件被拆除，
QuickJS 运行时被释放并重建，bundle 从头重新执行。LVGL 状态（窗口、主题、输入设备、
焦点组）会保留；JS 状态（信号、`loadFont` 句柄、动画）不会 —— 请在 bundle 的顶层
重新初始化它们。

用 `--no-watch` 关闭（冒烟测试循环用的也是它）。

## 中日韩字体自动探测

不带参数调用 `setDefaultFont()` 会从内置候选列表中自动探测第一个已安装的中日韩
字体，并按四个排版角色字号 14 / 16 / 20 / 24 px 加载它：

```js
import { setDefaultFont, findCjkFont, loadFontSizes } from "./js/framework.js";

setDefaultFont();               // zero-config — auto-discovers CJK at 14/16/20/24

const path = findCjkFont();     // returns path string or null
const fonts = loadFontSizes(path, [14, 16, 20, 24]);  // {14: h, 16: h, 20: h, 24: h}
                                // a size that fails to load is omitted, not 0
setDefaultFont(fonts);          // or pass a partial map: { 14: h, 20: h }
```

如果系统没有安装中日韩字体，`setDefaultFont()` 就是一个空操作，文本会回退到内置的
仅含拉丁字形的 Montserrat 字面。

字体句柄按 `(path, size)` 缓存，解码后的文件字节按路径共享，因此加载同一个文件的
四种字号只会读取该文件一次。Tiny-TTF 按引用持有那块缓冲区，所以无论是字节还是字体
对象都不会被释放。

按顺序探测的候选路径：
1. `/usr/share/fonts/truetype/wqy/wqy-microhei.ttc`
2. `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`
3. `/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc`
4. `/System/Library/Fonts/PingFang.ttc`
5. `C:/Windows/Fonts/msyh.ttc`

## 字体与国际化（中日韩 / 中文）

内置的 Montserrat 字体只含拉丁字形。要渲染中文（或任何其他文字系统），需在运行时
加载 TTF/TTC。推荐的模式是设置一个**全局默认字体**，仅在需要不同字号/字面时才逐
元素覆盖：

```js
import { loadFont, setDefaultFont } from "./js/framework.js";

const body  = loadFont("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 20);
const title = loadFont("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 30);

setDefaultFont(body);                                   // global default
h("Text", { text: "你好，世界" });                       // inherits `body`
h("Text", { style: { font: title }, text: "标题" });    // explicit override
```

* `loadFont(path, size)` 把文件读入内存并构建一个 Tiny-TTF 字体；文件打不开时返回
  `0`。文本按 UTF-8 处理。
* 中日韩字体会针对 `LV_SYMBOL_*` 字形（复选框对勾、下拉箭头）自动回退到对应的
  Montserrat；没有这层回退，你看到的这些符号会是豆腐块。
* Tiny-TTF 字体每个句柄对应一个固定像素字号 —— 需要几个字号就加载几个句柄。
  （`fontSize: 14|16|20|24` 选择的是内置的拉丁 Montserrat 字号。）

### 不支持 Emoji

**Emoji 不受支持，会渲染成豆腐块。** 这是文本栈本身的性质，而不是缺少字体：
Tiny-TTF 的 `stb_truetype` 后端只读取 `glyf`/`CFF` 轮廓，完全不支持所有 emoji
字体都在使用的 `CBDT`/`COLR`/`sbix` 彩色字形表，所以即便你加载
`NotoColorEmoji.ttf` 也毫无改变。LVGL 自己给出的方案是 `LV_USE_IMGFONT`（每个码点
一张图片），而本构建把它关闭了。中日韩文字、中日韩标点和 `LV_SYMBOL_*` 字形不受
影响。

## 输入框属性

所有属性都可以是响应式的（`() => signal()`）。

| 属性 | 类型 | 说明 |
|---|---|---|
| `text` | string | 输入框内容 |
| `placeholder` | string | 为空时的占位文字 |
| `oneLine` | boolean | 单行模式 |
| `maxLength` | number | 最大字符数（0 = 不限） |
| `acceptedChars` | string | 允许的字符，例如 `"0123456789"` |
| `password` | boolean | 用圆点遮蔽文本 |
| `align` | `"left"` \| `"center"` \| `"right"` | 文本对齐 |
| `textSelection` | boolean | 启用鼠标/触摸选区 |
| `cursorPos` | number | 设置光标位置（通过 `getProperty(ref, "cursorPos")` 读取） |

剪贴板通过下面的方式暴露：
```js
import { clipboard } from "./js/framework.js";
clipboard.write("text");
const s = clipboard.read();   // returns string or null
```

在任何获得焦点的 `<input>` 中，Ctrl+A/C/V/X 和 Home/End 都自动生效。

焦点与按键注入是可脚本化的，快捷键的往返测试正是借此做回归验证：

```js
import { focus, sendKey } from "./js/framework.js";

focus(inputRef);            // lv_group_focus_obj
sendKey("c", true);         // Ctrl+C into the focused widget
sendKey("end");             // Home / End also supported
```

## 键盘输入

一个全局焦点组被接到键盘设备上。交互式控件（`Input`、`Button`、`Switch`、
`Slider`、`Arc`、`Checkbox`、`Dropdown`、`Roller`、`Spinbox`、`ButtonMatrix`、
`Calendar`）会被自动加入，因此你可以点击 `Input` 使其获得焦点然后输入，或用 `Tab`
在控件之间切换。

ASCII 与多字节 **UTF-8（中日韩）输入**都能工作：宿主安装了一个 UTF-8 感知的键盘
读取回调，每次按键交付一个完整字符（否则 LVGL 默认的 SDL 驱动会把多字节字符拆开
并破坏它们）。

要输入中文，你仍然需要系统**输入法**（例如 fcitx5 或 ibus）在运行。每个 `Input`
在获得焦点时会把自己的屏幕矩形上报给 SDL，这样输入法的候选窗口就会跟随输入框。
宿主会在窗口创建之后重新激活 SDL 的文本输入，因为 `lv_sdl_window_create()` 调用
`SDL_StartTextInput()` 时窗口还不存在，X11 后端无从把输入法的输入上下文绑定上去
—— 没有这一步，普通打字仍然可用，但输入法合成出的文本永远送不进来。

`<spinbox>` 接受直接输入数字：一个数字会覆盖光标所在位上的数字，光标随之右移一位，
因此配置好的数字格式得以保持。点击某一位（或用 `←`/`→`）来选择在哪里输入；`↑`/`↓`
则对选中位做步进。

## 鼠标滚轮

滚轮滚动的是**指针下方**最内层的可滚动容器，一旦该容器滚到尽头，手势就交给下一个
可滚动的祖先 —— 因此滚动页面里的列表，行为和浏览器里一致。LVGL 自带的 SDL 滚轮
驱动是一个驱动焦点组的 encoder，那会去滚动当前获得焦点的对象，而不是光标所在的
对象；宿主改为按几何位置解析目标。

## 窗口标题

```js
import { setWindowTitle } from "./js/framework.js";
setWindowTitle("My App");        // default without this call: "LVGL Simulator"
```

若尚无显示设备则返回 `false`。X11 上有一个注意点：**非 ASCII** 标题只能可靠地写入
`WM_NAME`。SDL 是通过 `Xutf8TextListToTextProperty` 设置 `_NET_WM_NAME` 的 ——
而后者才是现代窗口管理器实际显示的属性 —— 该函数需要一个 Xlib 支持的 locale，而
常见的 `LANG=C.UTF-8` **并不在其列**，于是标题栏保持旧文本，`WM_NAME` 却已更新。
请使用 ASCII 标题，或在真正的 UTF-8 locale（例如 `en_US.UTF-8`）下运行。

## TypeScript 支持

[`js/framework.d.ts`](js/framework.d.ts) 提供了完整的类型声明：公开 API + JSX
内置元素 + 各控件属性 + 伪状态与部件样式 + 带类型的主题 token。VS Code 及其他
LSP 编辑器会对 `.js` 和 `.jsx` 文件自动采用它 —— 不需要 `tsconfig.json`。

颜色 token 与整数 token 是分开的联合类型，因此当值的种类与名称不匹配时，
`setThemeToken` 会直接是一个编译错误。已解析样式的 getter 也是按 key 定型的：
`getProperty(node, "backgroundColor")` 是颜色字符串，`getProperty(node, "radius")`
是数字。项目没有 CI —— 每次修改 `framework.d.ts` 后请手动做类型检查：

```sh
npx -y -p typescript@5 tsc --strict --target ES2020 --lib ES2020,DOM \
    --noEmit js/framework.d.ts
```

## 示例

| 路径                  | 展示内容                                          |
| --------------------- | ------------------------------------------------ |
| `examples/showcase`   | **核心演示** —— 全部 31 个宿主标签、明暗主题 + 运行时 token、中日韩角色字体、图像家族、屏幕键盘、图表/菜单/消息框、动画引擎。默认为交互式；`STONEGUI_SHOWCASE_SMOKE=1` 会运行脚本化的回归测试 |
| `examples/jsx`        | JSX + esbuild 工具链 —— 通过真实 JSX（转译后）展示控件 |
| `examples/test`       | 框架 / 主题 / 布局 / 输入 / 键盘 —— 537 条断言 |

### `examples/jsx` —— JSX（React/Vue 风格）

真正的 JSX，用 esbuild 转译成普通 JS（QuickJS 自己无法解析 JSX）。

```sh
cd examples/jsx
npm install        # once — installs esbuild (dev dependency)
npm run build      # app.jsx → app.js  (or: npm run watch)
cd ../..
./build/stonegui examples/jsx/app.js
```

`node_modules/` 和 `package-lock.json` 已被 gitignore —— `app.js` 是提交进仓库的，
因此不装任何东西也能跑这个演示；只有在修改 `app.jsx` 之后才需要 `npm install` 来
重新构建。

## 现状

仅支持 Linux/X11，基于 LVGL 9.2.2 + QuickJS + SDL2。本节以上的全部内容均已实现，
并由 `examples/showcase` 实际演练。

**不支持：** 彩色 emoji —— Tiny-TTF 文本栈无法解码彩色字形表，因此无论安装了什么
字体，emoji 都渲染为豆腐块（见[不支持 Emoji](#不支持-emoji)）。

**没有任何自动化闸门。** 没有 CI、没有格式化工具、没有 linter、没有回归运行器 ——
`.github/`、`scripts/` 和 `tools/` 都是被刻意移除的。正确性依赖于手动运行
`examples/test`（537 条断言）和 showcase 冒烟 bundle；已经**没有任何东西**会检查
主题 token 注册表的漂移，或 `doc/theme.md` 中的十六进制色值。

**计划中：** Wayland 后端、框架适配器（Solid/Preact）、原子化 intern 的
`js_setProperty`、DevTools / 信号检查器，以及尚未绑定的 LVGL 控件（`canvas`、
`tileview`、`win`）。
