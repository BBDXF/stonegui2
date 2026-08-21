#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BINARY="$BUILD_DIR/stonegui"
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/stonegui-regression.XXXXXX")"
INTERACTIVE_TIMEOUT="${STONEGUI_REGRESSION_TIMEOUT:-3}"
SKIP_JSX_BUILD=0

readonly -a BUNDLE_NAMES=(
    assertion showcase-smoke jsx showcase
)
readonly -a BUNDLE_MODES=(
    FINITE FINITE INTERACTIVE INTERACTIVE
)
readonly -a BUNDLE_EXPECTED=(
    "0 + ALL TESTS PASSED" "0 + SHOWCASE SMOKE PASSED" 143 143
)
declare -a BUNDLE_ACTUAL BUNDLE_RESULT

for index in "${!BUNDLE_NAMES[@]}"; do
    BUNDLE_ACTUAL[$index]="NOT RUN"
    BUNDLE_RESULT[$index]="FAIL"
done

usage() {
    cat <<'EOF'
Usage: ./scripts/run_regression.sh [--skip-jsx-build]

Runs the complete stonegui regression suite from the repository root.
EOF
}

while (($#)); do
    case "$1" in
        --skip-jsx-build) SKIP_JSX_BUILD=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR arguments: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ "$PWD" != "$ROOT_DIR" ]]; then
    echo "ERROR working-directory: run ./scripts/run_regression.sh from $ROOT_DIR" >&2
    exit 2
fi

print_summary() {
    local exit_status=$?
    printf '\n%-14s %-12s %-26s %-12s %s\n' "BUNDLE" "MODE" "EXPECTED" "ACTUAL" "RESULT"
    printf '%-14s %-12s %-26s %-12s %s\n' "--------------" "------------" "--------------------------" "------------" "------"
    local index
    for index in "${!BUNDLE_NAMES[@]}"; do
        printf '%-14s %-12s %-26s %-12s %s\n' \
            "${BUNDLE_NAMES[$index]}" "${BUNDLE_MODES[$index]}" \
            "${BUNDLE_EXPECTED[$index]}" "${BUNDLE_ACTUAL[$index]}" \
            "${BUNDLE_RESULT[$index]}"
    done
    printf '\nLogs: %s\n' "$RUN_DIR"
    return "$exit_status"
}
trap print_summary EXIT

hard_failure() {
    local name=$1 message=$2 log=${3:-}
    echo "HARD FAILURE [$name]: $message" >&2
    if [[ -n "$log" && -f "$log" ]]; then
        echo "----- $name output ($log) -----" >&2
        cat "$log" >&2
        echo "----- end $name output -----" >&2
    fi
    exit 1
}

declare -a DISPLAY_PREFIX=()
if [[ -z "${DISPLAY:-}" ]]; then
    if ! command -v xvfb-run >/dev/null 2>&1; then
        hard_failure "environment" 'DISPLAY is unset and xvfb-run is unavailable'
    fi
    if ! xvfb-run -a sh -c 'test -n "$DISPLAY"' >/dev/null 2>&1; then
        hard_failure "environment" 'DISPLAY is unset and xvfb-run cannot start a display'
    fi
    DISPLAY_PREFIX=(xvfb-run -a)
fi

echo ">>> Logs: $RUN_DIR"
echo ">>> Configure"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >"$RUN_DIR/configure.log" 2>&1 \
    || hard_failure "configure" "cmake configure failed" "$RUN_DIR/configure.log"

JOBS="$(nproc 2>/dev/null || printf '4')"
echo ">>> Build (-j$JOBS)"
cmake --build "$BUILD_DIR" -- -j"$JOBS" >"$RUN_DIR/build.log" 2>&1 \
    || hard_failure "build" "cmake build failed" "$RUN_DIR/build.log"

if [[ ! -x "$BINARY" ]]; then
    hard_failure "build" "missing executable: $BINARY" "$RUN_DIR/build.log"
fi

if ((SKIP_JSX_BUILD == 0)); then
    echo ">>> Rebuild JSX"
    (
        cd "$ROOT_DIR/examples/jsx"
        npm install --prefer-offline --no-audit --no-fund
        npm run build
    ) >"$RUN_DIR/jsx-build.log" 2>&1 \
        || hard_failure "jsx-build" "JSX rebuild failed" "$RUN_DIR/jsx-build.log"
else
    echo ">>> Rebuild JSX: skipped"
fi

run_finite() {
    local index=$1 name=$2 bundle=$3 marker=$4
    local log="$RUN_DIR/$name.log" status

    echo ">>> Run finite bundle: $name"
    set +e
    "${DISPLAY_PREFIX[@]}" "$BINARY" --no-watch "$bundle" >"$log" 2>&1
    status=$?
    set -e
    BUNDLE_ACTUAL[$index]="$status"

    if ((status != 0)); then
        hard_failure "$name" "expected exit 0, got $status" "$log"
    fi
    if ! grep -Fq "$marker" "$log"; then
        hard_failure "$name" "exit 0 but missing marker: $marker" "$log"
    fi
    BUNDLE_RESULT[$index]="PASS"
}

run_interactive() {
    local index=$1 name=$2 bundle=$3
    local log="$RUN_DIR/$name.log" status

    echo ">>> Run interactive bundle: $name"
    set +e
    "${DISPLAY_PREFIX[@]}" timeout --preserve-status --signal=TERM \
        "$INTERACTIVE_TIMEOUT" "$BINARY" --no-watch "$bundle" >"$log" 2>&1
    status=$?
    set -e
    BUNDLE_ACTUAL[$index]="$status"

    if ((status != 143)); then
        hard_failure "$name" "expected timeout exit 143, got $status" "$log"
    fi
    BUNDLE_RESULT[$index]="PASS"
}

ASSERTION_BUNDLE="${STONEGUI_REGRESSION_ASSERTION_BUNDLE:-$ROOT_DIR/examples/test/app.js}"
run_finite 0 assertion "$ASSERTION_BUNDLE" "ALL TESTS PASSED"

echo ">>> Type-check declarations"
npx -y -p typescript@5 tsc --strict --target ES2020 --lib ES2020,DOM \
    --noEmit "$ROOT_DIR/js/framework.d.ts" >"$RUN_DIR/declarations.log" 2>&1 \
    || hard_failure "declarations" "strict declaration check failed" "$RUN_DIR/declarations.log"

STONEGUI_SHOWCASE_SMOKE=1 run_finite \
    1 showcase-smoke "$ROOT_DIR/examples/showcase/app.js" "SHOWCASE SMOKE PASSED"

readonly -a INTERACTIVE_BUNDLES=(jsx showcase)
for offset in "${!INTERACTIVE_BUNDLES[@]}"; do
    name="${INTERACTIVE_BUNDLES[$offset]}"
    run_interactive "$((offset + 2))" "$name" "$ROOT_DIR/examples/$name/app.js"
done

echo ">>> Regression suite passed"
