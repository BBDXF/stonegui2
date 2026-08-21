#!/usr/bin/env node
/**
 * verify_theme_tokens.mjs — deterministic verifier for doc/theme.md.
 *
 * Zero dependencies (Node built-ins only). ESM, matching the repo's only
 * Node precedent (examples/jsx/package.json declares "type": "module").
 *
 * Every derived colour is RE-COMPUTED here from the six Element Plus base
 * colours using the documented Sass-compatible mixing formula; nothing in the
 * ramp is hardcoded. The results are then compared against the frozen table in
 * tools/theme_tokens.expected.json, which is what doc/theme.md's tables were
 * copy-pasted from. If the document and this script ever diverge, --check
 * fails.
 *
 *   node tools/verify_theme_tokens.mjs --check
 *       Recompute everything, compare to the expected table, print one
 *       `key=value` line per token to stdout (greppable), exit 0 on success.
 *
 *   node tools/verify_theme_tokens.mjs --expect primary.light_9=#ecf5ff
 *       Compare ONE computed key against a CLI-supplied value. Exit 0 on
 *       match; exit 1 printing `expected=... actual=...` on mismatch.
 *       May be repeated. Hex comparison is case-insensitive.
 *
 *   node tools/verify_theme_tokens.mjs --emit-expected
 *       Regenerate tools/theme_tokens.expected.json from the computation.
 *       Only ever run this deliberately, after re-deriving from upstream.
 *
 * Upstream sources (fetched from element-plus/element-plus @ dev):
 *   packages/theme-chalk/src/common/var.scss    — bases, mix loop, light roles
 *   packages/theme-chalk/src/dark/var.scss      — dark bg map, dark mix loop
 *   packages/theme-chalk/src/mixins/function.scss — roundColor()
 *   packages/theme-chalk/src/color/index.scss   — mix-overlay-color()
 */

import { readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import process from "node:process";

const HERE = dirname(fileURLToPath(import.meta.url));
const EXPECTED_PATH = join(HERE, "theme_tokens.expected.json");

/* ── Colour maths ───────────────────────────────────────────────────────── */

/** Sass `math.round`: round half AWAY FROM ZERO (not banker's rounding). */
const roundSass = (x) => (x < 0 ? -Math.floor(-x + 0.5) : Math.floor(x + 0.5));

const parseHex = (hex) => {
    const m = /^#([0-9a-fA-F]{6})$/.exec(hex.trim());
    if (!m) throw new TypeError(`not a #rrggbb colour: ${hex}`);
    const n = parseInt(m[1], 16);
    return [(n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff];
};

const toHex = (ch) =>
    "#" + ch.map((c) => Math.max(0, Math.min(255, c)).toString(16).padStart(2, "0")).join("");

/**
 * Sass `color.mix($c1, $c2, $weight)` for opaque colours, followed by Element
 * Plus's `roundColor()`:
 *     channel = round(weight * c1 + (1 - weight) * c2)
 * `weightPercent` is 0..100, matching `math.percentage(math.div($i, 10))`.
 */
const mixSass = (hex1, hex2, weightPercent) => {
    const w = weightPercent / 100;
    const a = parseHex(hex1);
    const b = parseHex(hex2);
    return toHex([0, 1, 2].map((i) => roundSass(w * a[i] + (1 - w) * b[i])));
};

/**
 * Element Plus `mix-overlay-color($upper, $lower)`: flatten an rgba colour
 * over an opaque backdrop.
 *     channel = round(upper * alpha + lower * (1 - alpha))
 * Rounding happens in Sass's `ie-hex-str`, which also rounds half away from
 * zero — identical to roundSass.
 */
const mixOverlay = (hexUpper, alpha, hexLower) => {
    const u = parseHex(hexUpper);
    const l = parseHex(hexLower);
    return toHex([0, 1, 2].map((i) => roundSass(u[i] * alpha + l[i] * (1 - alpha))));
};

/* ── Frozen upstream inputs ─────────────────────────────────────────────── */

const WHITE = "#ffffff";
const BLACK = "#000000";

/** common/var.scss $colors — the only hardcoded colours in this file. */
const BASES = {
    primary: "#409eff",
    success: "#67c23a",
    warning: "#e6a23c",
    danger: "#f56c6c",
    error: "#f56c6c", // documented alias of danger
    info: "#909399",
};

/** Only the ramp levels stonegui consumes. */
const LIGHT_LEVELS = [3, 5, 7, 9];

/** common/var.scss light neutral maps, verbatim. */
const LIGHT_NEUTRALS = {
    text_primary: "#303133",
    text_regular: "#606266",
    text_secondary: "#909399",
    text_placeholder: "#a8abb2",
    text_disabled: "#c0c4cc",
    border_base: "#dcdfe6",
    border_light: "#e4e7ed",
    border_lighter: "#ebeef5",
    border_extra_light: "#f2f6fc",
    border_dark: "#d4d7de",
    border_darker: "#cdd0d6",
    fill_base: "#f0f2f5",
    fill_light: "#f5f7fa",
    fill_lighter: "#fafafa",
    fill_extra_light: "#fafcff",
    fill_dark: "#ebedf0",
    fill_darker: "#e6e8eb",
    fill_blank: "#ffffff",
    bg_base: "#ffffff",
    bg_page: "#f2f3f5",
    bg_overlay: "#ffffff",
    white: WHITE,
    black: BLACK,
    overlay_mask: BLACK,
};

/** dark/var.scss $bg-color, verbatim. */
const DARK_BG = { bg_page: "#0a0a0a", bg_base: "#141414", bg_overlay: "#1d1e1f" };

/** dark/var.scss rgba bases + alpha maps; all flattened over $bg-color ''. */
const DARK_TEXT_BASE = "#f0f5ff";
const DARK_TEXT_ALPHA = {
    text_primary: 0.95,
    text_regular: 0.85,
    text_secondary: 0.65,
    text_placeholder: 0.55,
    text_disabled: 0.4,
};
const DARK_BORDER_BASE = "#f5f8ff";
const DARK_BORDER_ALPHA = {
    border_darker: 0.35,
    border_dark: 0.3,
    border_base: 0.25,
    border_light: 0.2,
    border_lighter: 0.15,
    border_extra_light: 0.1,
};
const DARK_FILL_BASE = "#fafcff";
const DARK_FILL_ALPHA = {
    fill_darker: 0.2,
    fill_dark: 0.16,
    fill_base: 0.12,
    fill_light: 0.08,
    fill_lighter: 0.04,
    fill_extra_light: 0.02,
};

/** Locked integer metrics — copied verbatim from the plan's Locked mapping. */
const METRICS = {
    radius_base: 4,
    radius_small: 2,
    radius_round: 20,
    border_width: 1,
    space_xs: 4,
    space_sm: 8,
    space_md: 12,
    space_lg: 16,
    space_xl: 20,
    control_height: 32,
    slider_track_size: 6,
    slider_knob_size: 20,
    arc_width: 10,
    scrollbar_size: 6,
    shadow_small_width: 6,
    shadow_overlay_width: 12,
    shadow_opa: 31, // rgba(0,0,0,0.12) -> round(0.12 * 255)
    disabled_opa: 128, // LV_OPA_50
    overlay_mask_opa: 128, // $popup modal-opacity 0.5 -> round(0.5 * 255)
    btn_pad_hor: 16,
    btn_pad_ver: 9,
};

/** Montserrat faces compiled into lv_conf.h, and the roles bound to them. */
const FONTS = { base: 14, medium: 16, large: 20, display: 24 };

/** Legacy sg_theme_tokens_t names -> canonical key (no `light.`/`dark.` prefix). */
const ALIASES = {
    primary: "primary.base",
    primary_dark: "primary.dark_2",
    on_primary: "white",
    secondary: "success.base",
    bg: "bg_page",
    surface: "bg_overlay",
    on_surface: "text_primary",
    on_variant: "text_secondary",
    outline: "border_base",
    track: "border_light",
    danger: "danger.base",
    warning: "warning.base",
    radius_btn: "metric.radius_base",
    radius_field: "metric.radius_base",
};

/* ── Table construction (everything below is derived, never typed) ──────── */

function buildRamp(out, prefix, mixColor, darkMixColor) {
    for (const [type, base] of Object.entries(BASES)) {
        out[`${prefix}${type}.base`] = base;
        for (const i of LIGHT_LEVELS) {
            out[`${prefix}${type}.light_${i}`] = mixSass(mixColor, base, i * 10);
        }
        out[`${prefix}${type}.dark_2`] = mixSass(darkMixColor, base, 20);
    }
}

function buildTokens() {
    const out = {};

    // Light ramps: light-i mixes toward white, dark-2 mixes toward black.
    buildRamp(out, "", WHITE, BLACK);
    // Light neutrals are literal upstream map values.
    for (const [k, v] of Object.entries(LIGHT_NEUTRALS)) out[k] = v;

    // Dark ramps: light-i mixes toward the dark PAGE-LEVEL bg (#141414), and
    // dark-2 mixes toward WHITE. This inversion is dark/var.scss's whole point.
    buildRamp(out, "dark.", DARK_BG.bg_base, WHITE);

    // Dark neutrals: rgba-over-#141414 flattened to hex via mix-overlay-color.
    for (const [k, v] of Object.entries(DARK_BG)) out[`dark.${k}`] = v;
    for (const [k, a] of Object.entries(DARK_TEXT_ALPHA)) {
        out[`dark.${k}`] = mixOverlay(DARK_TEXT_BASE, a, DARK_BG.bg_base);
    }
    for (const [k, a] of Object.entries(DARK_BORDER_ALPHA)) {
        out[`dark.${k}`] = mixOverlay(DARK_BORDER_BASE, a, DARK_BG.bg_base);
    }
    for (const [k, a] of Object.entries(DARK_FILL_ALPHA)) {
        out[`dark.${k}`] = mixOverlay(DARK_FILL_BASE, a, DARK_BG.bg_base);
    }
    out["dark.fill_blank"] = DARK_BG.bg_base; // upstream keeps 'blank' unmixed
    out["dark.white"] = WHITE;
    out["dark.black"] = BLACK;
    out["dark.overlay_mask"] = BLACK;

    for (const [k, v] of Object.entries(METRICS)) out[`metric.${k}`] = String(v);
    for (const [k, v] of Object.entries(FONTS)) out[`font.${k}`] = String(v);
    for (const [k, v] of Object.entries(ALIASES)) out[`alias.${k}`] = v;

    return out;
}

/* ── Self-check anchors (public Element Plus values, hard failures) ─────── */

const ANCHORS = {
    // common/var.scss documents the full primary ramp in a comment block.
    "primary.light_3": "#79bbff",
    "primary.light_5": "#a0cfff",
    "primary.light_7": "#c6e2ff",
    "primary.light_9": "#ecf5ff",
    // Published Element Plus dark CSS variables.
    "dark.text_primary": "#e5eaf3",
    "dark.text_regular": "#cfd3dc",
    "dark.border_base": "#4c4d4f",
    "dark.fill_base": "#303030",
    // Pre-existing sg_theme.c value that must survive the migration.
    "primary.dark_2": "#337ecc",
};

function checkAnchors(tokens) {
    const bad = [];
    for (const [k, want] of Object.entries(ANCHORS)) {
        if ((tokens[k] ?? "").toLowerCase() !== want) {
            bad.push(`  ANCHOR FAIL ${k} expected=${want} actual=${tokens[k]}`);
        }
    }
    return bad;
}

/* ── CLI ────────────────────────────────────────────────────────────────── */

function main(argv) {
    const tokens = buildTokens();

    if (argv.includes("--emit-expected")) {
        writeFileSync(EXPECTED_PATH, JSON.stringify(tokens, null, 2) + "\n");
        process.stderr.write(
            `wrote ${Object.keys(tokens).length} keys to ${EXPECTED_PATH}\n`,
        );
        return 0;
    }

    const expects = [];
    for (let i = 0; i < argv.length; i++) {
        if (argv[i] !== "--expect") continue;
        const pair = argv[i + 1];
        if (!pair || !pair.includes("=")) {
            process.stderr.write("--expect requires key=value\n");
            return 2;
        }
        const idx = pair.indexOf("=");
        expects.push([pair.slice(0, idx), pair.slice(idx + 1)]);
        i++;
    }

    if (expects.length > 0) {
        let failed = 0;
        for (const [key, want] of expects) {
            const actual = tokens[key];
            if (actual === undefined) {
                process.stderr.write(`FAIL ${key}: unknown token key\n`);
                failed++;
            } else if (actual.toLowerCase() !== want.toLowerCase()) {
                process.stderr.write(
                    `FAIL ${key}: expected=${want} actual=${actual}\n`,
                );
                failed++;
            } else {
                process.stdout.write(`OK ${key}=${actual}\n`);
            }
        }
        return failed === 0 ? 0 : 1;
    }

    if (!argv.includes("--check")) {
        process.stderr.write(
            "usage: verify_theme_tokens.mjs (--check | --expect key=value ... | --emit-expected)\n",
        );
        return 2;
    }

    const problems = checkAnchors(tokens);

    let expected;
    try {
        expected = JSON.parse(readFileSync(EXPECTED_PATH, "utf8"));
    } catch (e) {
        process.stderr.write(`cannot read ${EXPECTED_PATH}: ${e.message}\n`);
        return 2;
    }

    for (const key of Object.keys(tokens)) {
        if (!(key in expected)) problems.push(`  MISSING-IN-EXPECTED ${key}`);
    }
    for (const key of Object.keys(expected)) {
        if (!(key in tokens)) problems.push(`  STALE-IN-EXPECTED ${key}`);
        else if (tokens[key] !== expected[key]) {
            problems.push(
                `  MISMATCH ${key} expected=${expected[key]} actual=${tokens[key]}`,
            );
        }
    }

    // One line per key, sorted, so the output is stable and greppable.
    for (const key of Object.keys(tokens).sort()) {
        process.stdout.write(`${key}=${tokens[key]}\n`);
    }

    if (problems.length > 0) {
        process.stderr.write("FAILED:\n" + problems.join("\n") + "\n");
        return 1;
    }
    process.stderr.write(
        `OK ${Object.keys(tokens).length} keys verified (${Object.keys(ANCHORS).length} public anchors matched)\n`,
    );
    return 0;
}

process.exit(main(process.argv.slice(2)));
