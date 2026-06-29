import {
  h,
  Fragment,
  render,
  createSignal,
  loadFont,
  setDefaultFont,
  chartAddSeries,
  chartSetData,
  showMsgbox,
  getProperty
} from "../../js/framework.js";
import * as lv from "lvgl";
setDefaultFont();
const _cjkPath = lv.findCjkFontPath();
const fontTitle = _cjkPath ? loadFont(_cjkPath, 28) : 0;
function Row({ height = 56, gap = 12, children }) {
  return /* @__PURE__ */ h("view", { style: {
    flexFlow: "row",
    width: "100%",
    height,
    gap,
    alignItems: "center"
  } }, children);
}
function PillButton({ color, textColor, onClick, width = 110, children }) {
  const style = { width, height: 40, backgroundColor: color, borderRadius: 8 };
  if (textColor) style.textColor = textColor;
  return /* @__PURE__ */ h("button", { style, onClick }, children);
}
function Stat({ color, label, value, width = 220 }) {
  return /* @__PURE__ */ h("text", { style: { textColor: color, width } }, () => `${label}: ${value()}`);
}
function TabPage({ children }) {
  return /* @__PURE__ */ h("view", { style: {
    width: "100%",
    height: "100%",
    padding: 16,
    flexFlow: "column",
    gap: 12,
    scrollable: true
  } }, children);
}
function BasicsTab() {
  const [count, setCount] = createSignal(0);
  const [progress, setProgress] = createSignal(40);
  const [on, setOn] = createSignal(false);
  return /* @__PURE__ */ h(TabPage, null, /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(Stat, { color: "#40a02b", label: "\u8BA1\u6570 / Count", value: count }), /* @__PURE__ */ h(
    PillButton,
    {
      color: "blue",
      onClick: () => {
        setCount((c) => c + 1);
        setProgress((p) => Math.min(100, p + 5));
      }
    },
    "\u52A0\u4E00"
  ), /* @__PURE__ */ h(
    PillButton,
    {
      color: "pink",
      onClick: () => {
        setCount(0);
        setProgress(0);
      }
    },
    "\u91CD\u7F6E"
  )), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(
    Stat,
    {
      color: "#8839ef",
      label: "\u8FDB\u5EA6 / Progress",
      value: () => `${progress()}%`,
      width: 200
    }
  ), /* @__PURE__ */ h(
    "progress",
    {
      style: { flexGrow: 1, height: 18, borderRadius: 9 },
      min: 0,
      max: 100,
      value: () => progress()
    }
  )), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(
    Stat,
    {
      color: "#df8e1d",
      label: "\u5F00\u5173 / Switch",
      value: () => on() ? "\u5F00" : "\u5173",
      width: 200
    }
  ), /* @__PURE__ */ h("switch", { checked: () => on(), onChange: (v) => setOn(v) }), /* @__PURE__ */ h(
    "led",
    {
      style: { width: 22, height: 22 },
      color: "lime",
      brightness: () => on() ? 255 : 30
    }
  )));
}
function InputsTab() {
  const [volume, setVolume] = createSignal(35);
  const [age, setAge] = createSignal(18);
  const [agree, setAgree] = createSignal(false);
  const [fruit, setFruit] = createSignal(0);
  const FRUITS = ["\u82F9\u679C", "\u9999\u8549", "\u6A59\u5B50", "\u8461\u8404"];
  let inputRef = null;
  return /* @__PURE__ */ h(TabPage, null, /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69" } }, "\u6587\u672C\u8F93\u5165 / Text input:"), /* @__PURE__ */ h(
    "input",
    {
      style: { width: "100%", height: 42, scrollable: false },
      oneLine: true,
      placeholder: "\u70B9\u51FB\u6B64\u5904\u8F93\u5165\u2026",
      ref: (n) => inputRef = n
    }
  ), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(
    Stat,
    {
      color: "#fe640b",
      label: "\u97F3\u91CF / Volume",
      value: () => volume(),
      width: 180
    }
  ), /* @__PURE__ */ h(
    "slider",
    {
      style: { flexGrow: 1, height: 8 },
      min: 0,
      max: 100,
      value: () => volume(),
      onChange: (v) => setVolume(v)
    }
  )), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(
    Stat,
    {
      color: "#179299",
      label: "\u5E74\u9F84 / Age",
      value: () => age(),
      width: 180
    }
  ), /* @__PURE__ */ h(
    "spinbox",
    {
      style: { width: 140, height: 42 },
      digits: "3.0",
      step: 1,
      value: () => age(),
      onChange: (v) => setAge(v)
    }
  )), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(
    "checkbox",
    {
      text: "\u540C\u610F\u6761\u6B3E / Agree",
      style: { textColor: "#5c5f77", width: 220 },
      checked: () => agree(),
      onChange: (v) => setAgree(v)
    }
  ), /* @__PURE__ */ h(
    "dropdown",
    {
      style: { width: 160 },
      options: FRUITS.join("\n"),
      value: () => fruit(),
      onChange: (i) => setFruit(i)
    }
  ), /* @__PURE__ */ h("text", { style: { textColor: "#5c5f77", width: 140 } }, () => `\u9009\u62E9: ${FRUITS[fruit()]}`)), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h(
    PillButton,
    {
      color: "#89b4fa",
      textColor: "#1e1e2e",
      width: 150,
      onClick: () => showMsgbox({
        title: "Confirm",
        text: `Age=${age()}, agree=${agree()}.
Proceed?`,
        buttons: ["Cancel", "OK"]
      }, (idx) => {
        if (idx === 1) setAgree(true);
      })
    },
    "Open Msgbox"
  ), /* @__PURE__ */ h(
    PillButton,
    {
      color: "#a6e3a1",
      textColor: "#1e1e2e",
      width: 150,
      onClick: () => {
        const t = getProperty(inputRef, "text") || "(empty)";
        showMsgbox({
          title: "Input text",
          text: t,
          buttons: ["OK"]
        });
      }
    },
    "Show input"
  )));
}
function ListsTab() {
  const [selected, setSelected] = createSignal("(none)");
  const [pressed, setPressed] = createSignal(-1);
  const [reel, setReel] = createSignal(0);
  const KEYS = ["1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", "C", "0", "="];
  const ITEMS = ["Inbox", "Sent", "Drafts", "Spam", "Trash", "Archive"];
  return /* @__PURE__ */ h(TabPage, null, /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69" } }, () => `Selected list item: ${selected()}`), /* @__PURE__ */ h("list", { style: { width: "100%", height: 180, borderRadius: 8 } }, ITEMS.map((it) => /* @__PURE__ */ h("listButton", { text: it, onClick: () => setSelected(it) }))), /* @__PURE__ */ h(Row, { height: 40 }, /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69", width: "100%" } }, () => `Button matrix pressed: ${pressed() < 0 ? "(none)" : KEYS.filter((k) => k !== "\n")[pressed()]}`)), /* @__PURE__ */ h(
    "buttonMatrix",
    {
      style: { width: "100%", height: 200 },
      map: KEYS,
      onChange: (i) => setPressed(i)
    }
  ), /* @__PURE__ */ h(Row, null, /* @__PURE__ */ h("text", { style: { textColor: "#179299", width: 180 } }, () => `Roller: ${reel()}`), /* @__PURE__ */ h(
    "roller",
    {
      style: { width: 120, height: 120 },
      options: [...Array(12)].map((_, i) => `Item ${i + 1}`).join("\n"),
      value: () => reel(),
      onChange: (i) => setReel(i)
    }
  )));
}
function VisualsTab() {
  const [pct, setPct] = createSignal(20);
  let chartRef = null;
  let series = null;
  const data = [10, 28, 16, 42, 35, 58, 47, 70, 60, 82, 75, 95];
  return /* @__PURE__ */ h(TabPage, null, /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69" } }, "Chart (line):"), /* @__PURE__ */ h(
    "chart",
    {
      style: { width: "100%", height: 160 },
      chartType: "line",
      rangeMax: 100,
      divLines: "4x6",
      ref: (n) => {
        chartRef = n;
        series = chartAddSeries(n, "#89b4fa");
        chartSetData(n, series, data);
      }
    }
  ), /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69" } }, "Scale (horizontal):"), /* @__PURE__ */ h(
    "scale",
    {
      style: { width: "100%", height: 40 },
      scaleMode: "h-bottom",
      min: 0,
      max: 100,
      totalTicks: 21,
      majorEvery: 5,
      showLabels: true
    }
  ), /* @__PURE__ */ h(Row, { height: 120 }, /* @__PURE__ */ h("view", { style: {
    flexFlow: "column",
    gap: 6,
    width: 180,
    alignItems: "center"
  } }, /* @__PURE__ */ h("text", { style: { textColor: "#5c5f77" } }, () => `Arc: ${pct()}%`), /* @__PURE__ */ h(
    "arc",
    {
      style: { width: 90, height: 90 },
      min: 0,
      max: 100,
      value: () => pct(),
      onChange: (v) => setPct(v)
    }
  )), /* @__PURE__ */ h("view", { style: {
    flexFlow: "column",
    gap: 6,
    width: 140,
    alignItems: "center"
  } }, /* @__PURE__ */ h("text", { style: { textColor: "#5c5f77" } }, "Spinner"), /* @__PURE__ */ h("spinner", { style: { width: 48, height: 48 }, spinTime: 2500 })), /* @__PURE__ */ h("view", { style: {
    flexFlow: "column",
    gap: 6,
    width: 140,
    alignItems: "center"
  } }, /* @__PURE__ */ h("text", { style: { textColor: "#5c5f77" } }, "LED"), /* @__PURE__ */ h(
    "led",
    {
      style: { width: 36, height: 36 },
      color: "red",
      brightness: () => 80 + pct() * 1.5
    }
  ))), /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69" } }, "Span (rich text):"), /* @__PURE__ */ h(
    "span",
    {
      style: {
        width: "100%",
        height: 60,
        backgroundColor: "#dce0e8",
        borderRadius: 8,
        padding: 8
      },
      parts: [
        { text: "stonegui ", color: "#40a02b", fontSize: 20 },
        { text: "= ", color: "#4c4f69", fontSize: 20 },
        { text: "LVGL ", color: "#1e66f5", fontSize: 20 },
        { text: "+ ", color: "#4c4f69", fontSize: 20 },
        { text: "QuickJS", color: "#df8e1d", fontSize: 20 }
      ]
    }
  ));
}
function CalendarTab() {
  const [picked, setPicked] = createSignal("(none)");
  return /* @__PURE__ */ h(TabPage, null, /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69" } }, () => `Picked date: ${picked()}`), /* @__PURE__ */ h(
    "calendar",
    {
      style: { width: 320, height: 320 },
      today: "2026-06-18",
      shown: "2026-06",
      onChange: (d) => {
        if (d && d.year)
          setPicked(`${d.year}-${String(d.month).padStart(2, "0")}-${String(d.day).padStart(2, "0")}`);
      }
    }
  ));
}
function App() {
  return /* @__PURE__ */ h("view", { style: {
    width: "100%",
    height: "100%",
    backgroundColor: "#eff1f5",
    flexFlow: "column"
  } }, /* @__PURE__ */ h("view", { style: { width: "100%", height: 48, padding: 12 } }, /* @__PURE__ */ h("text", { style: { textColor: "#4c4f69", font: fontTitle } }, "stonegui \u2014 Widget Showcase")), /* @__PURE__ */ h("tabview", { style: { flexGrow: 1, width: "100%" }, tabBarSize: 44 }, /* @__PURE__ */ h("tab", { title: "Basics" }, "    ", /* @__PURE__ */ h(BasicsTab, null), "    "), /* @__PURE__ */ h("tab", { title: "Inputs" }, "    ", /* @__PURE__ */ h(InputsTab, null), "    "), /* @__PURE__ */ h("tab", { title: "Lists" }, "     ", /* @__PURE__ */ h(ListsTab, null), "     "), /* @__PURE__ */ h("tab", { title: "Visuals" }, "   ", /* @__PURE__ */ h(VisualsTab, null), "   "), /* @__PURE__ */ h("tab", { title: "Calendar" }, "  ", /* @__PURE__ */ h(CalendarTab, null), "  ")));
}
render(() => /* @__PURE__ */ h(App, null));
