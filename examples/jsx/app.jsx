/**
 * app.jsx — stonegui JSX demo (tabbed showcase of all bound widgets)
 *
 * Build:   npm install && npm run build  (in this directory)
 * Run:     ./build/stonegui examples/jsx/app.js   (from repo root)
 *
 * Demonstrates every host widget exposed by the renderer, organized into tabs:
 *   1. Basics    — counter, progress, switch
 *   2. Inputs    — input, spinbox, slider, dropdown, checkbox, msgbox
 *   3. Lists     — list, button matrix, roller
 *   4. Visuals   — chart, scale, arc, spinner, LED, span
 *   5. Calendar  — date picker
 */

import {
    h, Fragment, render, createSignal, loadFont, setDefaultFont,
    chartAddSeries, chartSetData, showMsgbox, getProperty,
} from "../../js/framework.js";
import * as lv from "lvgl";

/* ── Fonts ───────────────────────────────────────────────────────────────── */

setDefaultFont();
const _cjkPath = lv.findCjkFontPath();
const fontTitle = _cjkPath ? loadFont(_cjkPath, 28) : 0;

/* ── Shared components ───────────────────────────────────────────────────── */

function Row({ height = 56, gap = 12, children }) {
    return (
        <view style={{ flexFlow: "row", width: "100%", height, gap,
                       alignItems: "center" }}>
            {children}
        </view>
    );
}

function PillButton({ color, textColor, onClick, width = 110, children }) {
    const style = { width, height: 40, backgroundColor: color, borderRadius: 8 };
    if (textColor) style.textColor = textColor;
    return (
        <button style={style} onClick={onClick}>
            {children}
        </button>
    );
}

function Stat({ color, label, value, width = 220 }) {
    return (
        <text style={{ textColor: color, width }}>
            {() => `${label}: ${value()}`}
        </text>
    );
}

function TabPage({ children }) {
    return (
        <view style={{ width: "100%", height: "100%", padding: 16,
                       flexFlow: "column", gap: 12, scrollable: true }}>
            {children}
        </view>
    );
}

/* ── Tab pages ───────────────────────────────────────────────────────────── */

function BasicsTab() {
    const [count, setCount]       = createSignal(0);
    const [progress, setProgress] = createSignal(40);
    const [on, setOn]             = createSignal(false);

    return (
        <TabPage>
            <Row>
                <Stat color="#40a02b" label="计数 / Count" value={count} />
                <PillButton color="blue"
                    onClick={() => { setCount((c) => c + 1);
                                     setProgress((p) => Math.min(100, p + 5)); }}>
                    加一
                </PillButton>
                <PillButton color="pink"
                    onClick={() => { setCount(0); setProgress(0); }}>
                    重置
                </PillButton>
            </Row>

            <Row>
                <Stat color="#8839ef" label="进度 / Progress"
                      value={() => `${progress()}%`} width={200} />
                <progress style={{ flexGrow: 1, height: 18, borderRadius: 9 }}
                          min={0} max={100} value={() => progress()} />
            </Row>

            <Row>
                <Stat color="#df8e1d" label="开关 / Switch"
                      value={() => (on() ? "开" : "关")} width={200} />
                <switch checked={() => on()} onChange={(v) => setOn(v)} />
                <led style={{ width: 22, height: 22 }}
                     color="lime" brightness={() => (on() ? 255 : 30)} />
            </Row>
        </TabPage>
    );
}

function InputsTab() {
    const [volume, setVolume]   = createSignal(35);
    const [age, setAge]         = createSignal(18);
    const [agree, setAgree]     = createSignal(false);
    const [fruit, setFruit]     = createSignal(0);
    const FRUITS = ["苹果", "香蕉", "橙子", "葡萄"];
    let inputRef = null;

    return (
        <TabPage>
            <text style={{ textColor: "#4c4f69" }}>文本输入 / Text input:</text>
            <input style={{ width: "100%", height: 42, scrollable: false }}
                   oneLine={true}
                   placeholder="点击此处输入…"
                   ref={(n) => (inputRef = n)} />

            <Row>
                <Stat color="#fe640b" label="音量 / Volume"
                      value={() => volume()} width={180} />
                <slider style={{ flexGrow: 1, height: 8 }}
                        min={0} max={100} value={() => volume()}
                        onChange={(v) => setVolume(v)} />
            </Row>

            <Row>
                <Stat color="#179299" label="年龄 / Age"
                      value={() => age()} width={180} />
                <spinbox style={{ width: 140, height: 42 }}
                         digits="3.0" step={1}
                         value={() => age()}
                         onChange={(v) => setAge(v)} />
            </Row>

            <Row>
                <checkbox text="同意条款 / Agree"
                          style={{ textColor: "#5c5f77", width: 220 }}
                          checked={() => agree()}
                          onChange={(v) => setAgree(v)} />
                <dropdown style={{ width: 160 }}
                          options={FRUITS.join("\n")}
                          value={() => fruit()}
                          onChange={(i) => setFruit(i)} />
                <text style={{ textColor: "#5c5f77", width: 140 }}>
                    {() => `选择: ${FRUITS[fruit()]}`}
                </text>
            </Row>

            <Row>
                <PillButton color="#89b4fa" textColor="#1e1e2e" width={150}
                    onClick={() => showMsgbox({
                        title:   "Confirm",
                        text:    `Age=${age()}, agree=${agree()}.\nProceed?`,
                        buttons: ["Cancel", "OK"],
                    }, (idx) => {
                        if (idx === 1) setAgree(true);
                    })}>
                    Open Msgbox
                </PillButton>
                <PillButton color="#a6e3a1" textColor="#1e1e2e" width={150}
                    onClick={() => {
                        const t = getProperty(inputRef, "text") || "(empty)";
                        showMsgbox({ title: "Input text",
                                     text:  t,
                                     buttons: ["OK"] });
                    }}>
                    Show input
                </PillButton>
            </Row>
        </TabPage>
    );
}

function ListsTab() {
    const [selected, setSelected] = createSignal("(none)");
    const [pressed, setPressed]   = createSignal(-1);
    const [reel, setReel]         = createSignal(0);
    const KEYS = ["1","2","3","\n","4","5","6","\n","7","8","9","\n","C","0","="];
    const ITEMS = ["Inbox", "Sent", "Drafts", "Spam", "Trash", "Archive"];

    return (
        <TabPage>
            <text style={{ textColor: "#4c4f69" }}>
                {() => `Selected list item: ${selected()}`}
            </text>
            <list style={{ width: "100%", height: 180, borderRadius: 8 }}>
                {ITEMS.map((it) => (
                    <listButton text={it} onClick={() => setSelected(it)} />
                ))}
            </list>

            <Row height={40}>
                <text style={{ textColor: "#4c4f69", width: "100%" }}>
                    {() => `Button matrix pressed: ${
                        pressed() < 0 ? "(none)" : KEYS.filter(k => k !== "\n")[pressed()]
                    }`}
                </text>
            </Row>
            <buttonMatrix style={{ width: "100%", height: 200 }}
                          map={KEYS}
                          onChange={(i) => setPressed(i)} />

            <Row>
                <text style={{ textColor: "#179299", width: 180 }}>
                    {() => `Roller: ${reel()}`}
                </text>
                <roller style={{ width: 120, height: 120 }}
                        options={[...Array(12)].map((_,i) => `Item ${i+1}`).join("\n")}
                        value={() => reel()}
                        onChange={(i) => setReel(i)} />
            </Row>
        </TabPage>
    );
}

function VisualsTab() {
    const [pct, setPct] = createSignal(20);
    let chartRef = null;
    let series   = null;

    const data = [10, 28, 16, 42, 35, 58, 47, 70, 60, 82, 75, 95];

    return (
        <TabPage>
            <text style={{ textColor: "#4c4f69" }}>Chart (line):</text>
            <chart style={{ width: "100%", height: 160 }}
                   chartType="line"
                   rangeMax={100}
                   divLines="4x6"
                   ref={(n) => {
                       chartRef = n;
                       series = chartAddSeries(n, "#89b4fa");
                       chartSetData(n, series, data);
                   }} />

            <text style={{ textColor: "#4c4f69" }}>Scale (horizontal):</text>
            <scale style={{ width: "100%", height: 40 }}
                   scaleMode="h-bottom"
                   min={0} max={100}
                   totalTicks={21} majorEvery={5} showLabels={true} />

            <Row height={120}>
                <view style={{ flexFlow: "column", gap: 6, width: 180,
                               alignItems: "center" }}>
                    <text style={{ textColor: "#5c5f77" }}>
                        {() => `Arc: ${pct()}%`}
                    </text>
                    <arc style={{ width: 90, height: 90 }}
                         min={0} max={100} value={() => pct()}
                         onChange={(v) => setPct(v)} />
                </view>
                <view style={{ flexFlow: "column", gap: 6, width: 140,
                               alignItems: "center" }}>
                    <text style={{ textColor: "#5c5f77" }}>Spinner</text>
                    <spinner style={{ width: 48, height: 48 }} spinTime={2500} />
                </view>
                <view style={{ flexFlow: "column", gap: 6, width: 140,
                               alignItems: "center" }}>
                    <text style={{ textColor: "#5c5f77" }}>LED</text>
                    <led style={{ width: 36, height: 36 }}
                         color="red" brightness={() => 80 + pct() * 1.5} />
                </view>
            </Row>

            <text style={{ textColor: "#4c4f69" }}>Span (rich text):</text>
            <span style={{ width: "100%", height: 60, backgroundColor: "#dce0e8",
                            borderRadius: 8, padding: 8 }}
                  parts={[
                      { text: "stonegui ", color: "#40a02b", fontSize: 20 },
                      { text: "= ",         color: "#4c4f69", fontSize: 20 },
                      { text: "LVGL ",      color: "#1e66f5", fontSize: 20 },
                      { text: "+ ",         color: "#4c4f69", fontSize: 20 },
                      { text: "QuickJS",    color: "#df8e1d", fontSize: 20 },
                  ]} />
        </TabPage>
    );
}

function CalendarTab() {
    const [picked, setPicked] = createSignal("(none)");
    return (
        <TabPage>
            <text style={{ textColor: "#4c4f69" }}>
                {() => `Picked date: ${picked()}`}
            </text>
            <calendar style={{ width: 320, height: 320 }}
                      today="2026-06-18"
                      shown="2026-06"
                      onChange={(d) => {
                          if (d && d.year)
                              setPicked(`${d.year}-${String(d.month).padStart(2,"0")}-${String(d.day).padStart(2,"0")}`);
                      }} />
        </TabPage>
    );
}

/* ── App: outer Tabview ──────────────────────────────────────────────────── */

function App() {
    return (
        <view style={{ width: "100%", height: "100%",
                       backgroundColor: "#eff1f5", flexFlow: "column" }}>
            <view style={{ width: "100%", height: 48, padding: 12 }}>
                <text style={{ textColor: "#4c4f69", font: fontTitle }}>
                    stonegui — Widget Showcase
                </text>
            </view>
            <tabview style={{ flexGrow: 1, width: "100%" }} tabBarSize={44}>
                <tab title="Basics">    <BasicsTab />    </tab>
                <tab title="Inputs">    <InputsTab />    </tab>
                <tab title="Lists">     <ListsTab />     </tab>
                <tab title="Visuals">   <VisualsTab />   </tab>
                <tab title="Calendar">  <CalendarTab />  </tab>
            </tabview>
        </view>
    );
}

render(() => <App />);
