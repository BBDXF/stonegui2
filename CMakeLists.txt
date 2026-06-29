cmake_minimum_required(VERSION 3.16)
project(stonegui C)

set(CMAKE_C_STANDARD 11)

# ── Dependencies via FetchContent ─────────────────────────────────────────
# Fresh clones get LVGL + QuickJS automatically (downloaded into build/_deps/).
# Override either with -DSTONEGUI_LVGL_SOURCE_DIR=... or
# -DSTONEGUI_QUICKJS_SOURCE_DIR=... to point at a local checkout (useful when
# hacking on upstream or working offline).
include(FetchContent)

# ── LVGL 9.2.2 ────────────────────────────────────────────────────────────
# LV_CONF_PATH and LV_CONF_BUILD_DISABLE_* MUST be set before
# FetchContent_MakeAvailable, which calls add_subdirectory(lvgl) implicitly
# and LVGL's own CMakeLists reads them at that point.
set(LV_CONF_PATH                   ${CMAKE_SOURCE_DIR}/lv_conf.h)
set(LV_CONF_BUILD_DISABLE_EXAMPLES 1)
set(LV_CONF_BUILD_DISABLE_DEMOS    1)

if(STONEGUI_LVGL_SOURCE_DIR)
    FetchContent_Declare(lvgl SOURCE_DIR ${STONEGUI_LVGL_SOURCE_DIR})
else()
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG        v9.2.2          # commit c98ab243621a2a948674da5339c15da88832f928
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
endif()
FetchContent_MakeAvailable(lvgl)
target_include_directories(lvgl PUBLIC ${CMAKE_SOURCE_DIR})

# ── QuickJS (Bellard, 2025-09-13) ─────────────────────────────────────────
# Upstream has no CMakeLists.txt; we build it manually below.
# GIT_TAG must be a commit hash because Bellard's repo carries no release tags.
if(STONEGUI_QUICKJS_SOURCE_DIR)
    FetchContent_Declare(quickjs SOURCE_DIR ${STONEGUI_QUICKJS_SOURCE_DIR})
else()
    FetchContent_Declare(quickjs
        GIT_REPOSITORY https://github.com/bellard/quickjs.git
        GIT_TAG        4c722cea4e709682003eb0da67b393ea38c56e5e   # VERSION = 2025-09-13
        GIT_SHALLOW    FALSE   # commit hash can't be shallow-cloned
        GIT_PROGRESS   TRUE
    )
endif()
FetchContent_MakeAvailable(quickjs)

# Read the version string from QuickJS's own VERSION file so JS_GetVersion()
# reports the right value (the previous hardcoded "2024-01-13" had drifted).
file(READ ${quickjs_SOURCE_DIR}/VERSION QUICKJS_VERSION_RAW)
string(STRIP "${QUICKJS_VERSION_RAW}" QUICKJS_VERSION)

add_library(quickjs STATIC
    ${quickjs_SOURCE_DIR}/quickjs.c
    ${quickjs_SOURCE_DIR}/libregexp.c
    ${quickjs_SOURCE_DIR}/libunicode.c
    ${quickjs_SOURCE_DIR}/cutils.c
    ${quickjs_SOURCE_DIR}/dtoa.c
    ${quickjs_SOURCE_DIR}/quickjs-libc.c
)
target_include_directories(quickjs PUBLIC ${quickjs_SOURCE_DIR})
target_compile_definitions(quickjs PRIVATE
    CONFIG_VERSION="${QUICKJS_VERSION}"
    _GNU_SOURCE
)
target_compile_options(quickjs PRIVATE -Wno-unused-result -Wno-sign-compare)

# ── SDL2 ──────────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# ── stonegui host ─────────────────────────────────────────────────────────
add_executable(stonegui
    src/main.c
    src/lv_bindings.c
    src/sg_theme.c
)

target_include_directories(stonegui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${lvgl_SOURCE_DIR}
    ${quickjs_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(stonegui PRIVATE
    lvgl
    quickjs
    ${SDL2_LIBRARIES}
    m dl pthread
)

target_compile_definitions(stonegui PRIVATE _GNU_SOURCE)

# Copy JS / examples next to the binary so runtime relative imports resolve.
add_custom_command(TARGET stonegui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/js ${CMAKE_BINARY_DIR}/js
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/examples ${CMAKE_BINARY_DIR}/examples
    COMMENT "Copying JS files to build directory"
)
