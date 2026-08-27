#!/usr/bin/env node

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");
const EXPECTED_COUNTS = { color: 72, integer: 23 };

function parsePaths(argv) {
    const paths = {
        c: join(ROOT, "src/sg_theme.c"),
        types: join(ROOT, "js/framework.d.ts"),
        doc: join(ROOT, "doc/theme.md"),
    };
    for (let index = 0; index < argv.length; index += 2) {
        const option = argv[index];
        const value = argv[index + 1];
        if (!value || !["--c", "--types", "--doc"].includes(option)) {
            throw new TypeError(
                "usage: verify_theme_token_parity.mjs [--c path] [--types path] [--doc path]",
            );
        }
        paths[option.slice(2)] = value;
    }
    return paths;
}

function addToken(tokens, name, kind, source) {
    const previous = tokens.get(name);
    if (previous && previous !== kind) {
        throw new TypeError(`${source}: token ${name} has both ${previous} and ${kind} kinds`);
    }
    tokens.set(name, kind);
}

function parseC(path) {
    const tokens = new Map();
    const source = readFileSync(path, "utf8");
    const entries = source.matchAll(/SG_(COLOR|INT)_TOKEN\("([^"]+)"/g);
    for (const [, macroKind, name] of entries) {
        addToken(tokens, name, macroKind === "COLOR" ? "color" : "integer", path);
    }
    return tokens;
}

function parseTypeUnion(source, typeName, path) {
    const match = new RegExp(`export type ${typeName} =([\\s\\S]*?);`).exec(source);
    if (!match) throw new TypeError(`${path}: missing ${typeName}`);
    return [...match[1].matchAll(/"([^"]+)"/g)].map((entry) => entry[1]);
}

function parseTypes(path) {
    const tokens = new Map();
    const source = readFileSync(path, "utf8");
    for (const name of parseTypeUnion(source, "ColorThemeTokenName", path)) {
        addToken(tokens, name, "color", path);
    }
    for (const name of parseTypeUnion(source, "IntegerThemeTokenName", path)) {
        addToken(tokens, name, "integer", path);
    }
    return tokens;
}

function section(source, start, end) {
    const startIndex = source.indexOf(start);
    const endIndex = source.indexOf(end, startIndex + start.length);
    if (startIndex < 0 || endIndex < 0) {
        throw new TypeError(`doc/theme.md: missing section boundary ${start} .. ${end}`);
    }
    return source.slice(startIndex, endIndex);
}

function firstColumn(table) {
    return [...table.matchAll(/^\| `([^`]+)` \|/gm)].map((entry) => entry[1]);
}

function parseDoc(path) {
    const tokens = new Map();
    const source = readFileSync(path, "utf8");
    const ramps = firstColumn(section(source, "## 3. Derived ramps", "## 4. Derived ramps"));
    for (const semantic of ramps) {
        for (const suffix of ["base", "light_3", "light_5", "light_7", "light_9", "dark_2"]) {
            addToken(tokens, `${semantic}.${suffix}`, "color", path);
        }
    }
    for (const name of firstColumn(section(source, "## 5. Neutral roles", "### 5.1"))) {
        addToken(tokens, name, "color", path);
    }
    for (const name of firstColumn(section(source, "## 6. Locked geometry", "### 6.1"))) {
        addToken(tokens, name, "integer", path);
    }
    const aliases = section(source, "## 9. Legacy alias table", "### 9.1");
    for (const match of aliases.matchAll(/^\| `([^`]+)` \| `([^`]+)` \|/gm)) {
        const [, name, canonical] = match;
        addToken(tokens, name, canonical.startsWith("radius_") ? "integer" : "color", path);
    }
    return tokens;
}

function compare(registries) {
    const problems = [];
    const allNames = new Set(Object.values(registries).flatMap((tokens) => [...tokens.keys()]));
    for (const name of [...allNames].sort()) {
        for (const [source, tokens] of Object.entries(registries)) {
            if (!tokens.has(name)) problems.push(`MISSING ${name} in ${source}`);
        }
        const kinds = new Set(Object.values(registries).map((tokens) => tokens.get(name)).filter(Boolean));
        if (kinds.size > 1) {
            problems.push(
                `KIND ${name}: ${Object.entries(registries).map(([source, tokens]) => `${source}=${tokens.get(name) ?? "missing"}`).join(" ")}`,
            );
        }
    }
    for (const [source, tokens] of Object.entries(registries)) {
        for (const [kind, expected] of Object.entries(EXPECTED_COUNTS)) {
            const actual = [...tokens.values()].filter((value) => value === kind).length;
            if (actual !== expected) problems.push(`COUNT ${source} ${kind}: expected=${expected} actual=${actual}`);
        }
    }
    return problems;
}

function main(argv) {
    try {
        const paths = parsePaths(argv);
        const problems = compare({
            "src/sg_theme.c": parseC(paths.c),
            "js/framework.d.ts": parseTypes(paths.types),
            "doc/theme.md": parseDoc(paths.doc),
        });
        if (problems.length > 0) {
            process.stderr.write(`TOKEN REGISTRY PARITY FAILED\n${problems.join("\n")}\n`);
            return 1;
        }
        process.stderr.write("OK token registry parity: 72 color + 23 integer\n");
        return 0;
    } catch (error) {
        process.stderr.write(`${error.message}\n`);
        return 2;
    }
}

process.exit(main(process.argv.slice(2)));
