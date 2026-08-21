#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP=""
cleanup() {
    if [[ -n "$TMP" ]]; then
        rm -rf "$TMP"
        printf 'CLEANUP removed %s\n' "$TMP"
    fi
}
TMP=$(mktemp -d /tmp/stonegui-theme-mutation.XXXXXX)
trap cleanup EXIT HUP INT TERM

COPY="$TMP/stonegui"
mkdir "$COPY"
STATUS_BEFORE=$(git -C "$ROOT" status --porcelain=v1 --untracked-files=all)
fingerprint() {
    tar -C "$ROOT" --sort=name --mtime='UTC 1970-01-01' \
        --exclude=.git --exclude=build --exclude=node_modules \
        --exclude=.debug-journal.md -cf - \
        CMakeLists.txt lv_conf.h src js examples doc tools 2>/dev/null | sha256sum
}
HASH_BEFORE=$(fingerprint)

tar -C "$ROOT" --exclude=.git --exclude=build --exclude=node_modules \
    --exclude=.omo --exclude=.debug-journal.md -cf - . | tar -C "$COPY" -xf -
cp "$COPY/src/sg_theme.c" "$TMP/sg_theme.c.clean"

cmake -S "$COPY" -B "$COPY/build" \
    -DSTONEGUI_LVGL_SOURCE_DIR="$ROOT/build/_deps/lvgl-src" \
    -DSTONEGUI_QUICKJS_SOURCE_DIR="$ROOT/build/_deps/quickjs-src" >/dev/null

mutate() {
    local name=$1
    local needle=$2
    local replacement=$3
    local expected_failure=$4
    cp "$TMP/sg_theme.c.clean" "$COPY/src/sg_theme.c"
    python3 - "$COPY/src/sg_theme.c" "$needle" "$replacement" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
needle = sys.argv[2]
replacement = sys.argv[3]
source = path.read_text()
if source.count(needle) != 1:
    raise SystemExit(f"mutation target count is {source.count(needle)}, expected one")
path.write_text(source.replace(needle, replacement))
PY
    cmake --build "$COPY/build" >/dev/null
    set +e
    local output
    output=$("$COPY/build/stonegui" --no-watch "$COPY/examples/test/app.js" 2>&1)
    local result=$?
    set -e
    if [[ $result -ne 1 ]]; then
        printf 'ERROR mutation %s exited %d, expected 1\n' "$name" "$result" >&2
        exit 2
    fi
    if ! grep -Fq "FAIL $expected_failure" <<<"$output"; then
        printf 'ERROR mutation %s lacked named failure: %s\n' "$name" "$expected_failure" >&2
        exit 3
    fi
    grep -F "FAIL $expected_failure" <<<"$output"
    printf 'MUTATION PASS mode=%s exit=1 named-failure="%s"\n' "$name" "$expected_failure"
}

mutate placeholder \
    'lv_style_set_text_color(&st_field_placeholder, t->text_placeholder);' \
    'lv_style_set_text_color(&st_field_placeholder, t->text_secondary);' \
    'coverage light: input placeholder default'
mutate animimg-branch \
    'lv_obj_add_style(obj, &st_animimg, 0);' \
    '(void)obj;' \
    'coverage dark: animimg MAIN exact-class branch witness'
mutate view-branch \
    'lv_obj_add_style(obj, &st_view, 0);' \
    '(void)obj;' \
    'coverage dark: view MAIN transparent branch witness'
mutate menu-section-branch \
    'lv_obj_add_style(obj, &st_menu_section, 0);' \
    '(void)obj;' \
    'coverage dark: menu section MAIN overlay'
mutate scrollbar-scrolled-state \
    'lv_style_set_bg_opa(&st_scrollbar_scrolled, LV_OPA_COVER);' \
    'lv_style_set_bg_opa(&st_scrollbar_scrolled, LV_OPA_TRANSP);' \
    'coverage light: view SCROLLBAR scrolled opacity'

STATUS_AFTER=$(git -C "$ROOT" status --porcelain=v1 --untracked-files=all)
HASH_AFTER=$(fingerprint)
if [[ "$STATUS_BEFORE" != "$STATUS_AFTER" || "$HASH_BEFORE" != "$HASH_AFTER" ]]; then
    printf 'ERROR source worktree changed during temp-copy mutations\n' >&2
    exit 4
fi

printf 'WORKTREE UNCHANGED fingerprint=%s status=yes\n' "${HASH_AFTER%% *}"
