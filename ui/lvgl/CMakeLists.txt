cmake_minimum_required(VERSION 3.20)
project(ZorkLVGL C CXX)

# ASM is only needed for embedded builds (story_data.S).
if(NOT BUILD_DESKTOP)
    enable_language(ASM)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# --- Options ---
option(BUILD_DESKTOP "Build SDL2 desktop simulator" ON)
set(DISPLAY_PROFILE "DESKTOP" CACHE STRING "Display profile: RT1050|RT1170|DESKTOP")

# --- Project root (for shared sources) ---
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# --- SDL2 (submodule, built before LVGL so LVGL's SDL driver can find it) ---
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_ROOT}/external/sdl2 ${CMAKE_BINARY_DIR}/sdl2_build EXCLUDE_FROM_ALL)

# --- LVGL library ---
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
add_subdirectory(${PROJECT_ROOT}/external/lvgl ${CMAKE_BINARY_DIR}/lvgl_build)

# LVGL's GLOB_RECURSE pulls in ARM Helium .S files that fail to assemble
# on desktop (C typedefs from stdint.h leak through the preprocessor).
# Strip them; the actual Helium code is MCU-only anyway.
get_target_property(_lvgl_srcs lvgl SOURCES)
list(FILTER _lvgl_srcs EXCLUDE REGEX "\\.S$")
set_target_properties(lvgl PROPERTIES SOURCES "${_lvgl_srcs}")

# LVGL's SDL driver needs SDL2 headers. Link lvgl against SDL2-static so
# it inherits the correct include paths and link dependencies.
target_link_libraries(lvgl PUBLIC SDL2-static)

# --- Executable ---
add_executable(ZorkLVGL
    app/zork_ui.c
    app/zork_output.c
    app/zork_input.c
    app/zork_status.c
    app/zork_compass.c
    app/zork_theme.c
    adapter/fizmo_lvgl_adapter.c
)

# --- Display profile compile definitions ---
if(DISPLAY_PROFILE STREQUAL "RT1170")
    target_compile_definitions(ZorkLVGL PRIVATE DISPLAY_RT1170=1)
elseif(DISPLAY_PROFILE STREQUAL "RT1050")
    target_compile_definitions(ZorkLVGL PRIVATE DISPLAY_RT1050=1)
else()
    target_compile_definitions(ZorkLVGL PRIVATE DISPLAY_DESKTOP=1)
endif()

# --- Asset paths ---
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets" ASSETS_DIR)
target_compile_definitions(ZorkLVGL PRIVATE
    COMPASS_ROSE_PATH="A:${ASSETS_DIR}/compass-rose.png"
)

# --- Include directories ---
target_include_directories(ZorkLVGL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${PROJECT_ROOT}/src
    ${PROJECT_ROOT}/external/libfizmo/src
)

# --- Link LVGL ---
target_link_libraries(ZorkLVGL PRIVATE lvgl)

# --- Desktop build ---
if(BUILD_DESKTOP)
    # Desktop entry point
    target_sources(ZorkLVGL PRIVATE main_desktop.c)

    # fizmo desktop bridge (C++)
    target_sources(ZorkLVGL PRIVATE ${PROJECT_ROOT}/src/fizmo_bridge.cpp)

    # libfizmo interpreter sources
    set(LIBFIZMO_DIR ${PROJECT_ROOT}/external/libfizmo/src)
    target_sources(ZorkLVGL PRIVATE
        ${LIBFIZMO_DIR}/interpreter/blockbuf.c
        ${LIBFIZMO_DIR}/interpreter/config.c
        ${LIBFIZMO_DIR}/interpreter/fizmo.c
        ${LIBFIZMO_DIR}/interpreter/mathemat.c
        ${LIBFIZMO_DIR}/interpreter/misc.c
        ${LIBFIZMO_DIR}/interpreter/mt19937ar.c
        ${LIBFIZMO_DIR}/interpreter/object.c
        ${LIBFIZMO_DIR}/interpreter/output.c
        ${LIBFIZMO_DIR}/interpreter/property.c
        ${LIBFIZMO_DIR}/interpreter/routine.c
        ${LIBFIZMO_DIR}/interpreter/savegame.c
        ${LIBFIZMO_DIR}/interpreter/sound.c
        ${LIBFIZMO_DIR}/interpreter/stack.c
        ${LIBFIZMO_DIR}/interpreter/streams.c
        ${LIBFIZMO_DIR}/interpreter/table.c
        ${LIBFIZMO_DIR}/interpreter/text.c
        ${LIBFIZMO_DIR}/interpreter/undo.c
        ${LIBFIZMO_DIR}/interpreter/variable.c
        ${LIBFIZMO_DIR}/interpreter/wordwrap.c
        ${LIBFIZMO_DIR}/interpreter/zpu.c
        ${LIBFIZMO_DIR}/interpreter/iff.c
        ${LIBFIZMO_DIR}/tools/filesys.c
        ${LIBFIZMO_DIR}/tools/filesys_c.c
        ${LIBFIZMO_DIR}/tools/i18n.c
        ${LIBFIZMO_DIR}/tools/list.c
        ${LIBFIZMO_DIR}/tools/stringmap.c
        ${LIBFIZMO_DIR}/tools/tracelog.c
        ${LIBFIZMO_DIR}/tools/types.c
        ${LIBFIZMO_DIR}/tools/z_ucs.c
    )

    # Locale stubs
    target_sources(ZorkLVGL PRIVATE ${PROJECT_ROOT}/src/fizmo_locale_stubs.c)

    # libfizmo compile definitions
    target_compile_definitions(ZorkLVGL PRIVATE
        USE_FIZMO_BRIDGE=1
        DISABLE_BABEL=1
        DISABLE_FILELIST=1
        DISABLE_CONFIGFILES=1
        DISABLE_COMMAND_HISTORY=1
        DISABLE_OUTPUT_HISTORY=1
        DISABLE_PREFIX_COMMANDS=1
        DISABLE_BLOCKBUFFER=1
        ZORK_STORY_PATH="${PROJECT_ROOT}/zork1.z3"
    )

    # Force-include compatibility header
    target_compile_options(ZorkLVGL PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include ${PROJECT_ROOT}/src/fizmo_embedded_compat.h>
        $<$<COMPILE_LANGUAGE:CXX>:-include ${PROJECT_ROOT}/src/fizmo_embedded_compat.h>
    )

    # SDL2 (already added above for LVGL)
    target_link_libraries(ZorkLVGL PRIVATE SDL2-static)

    # Threading
    find_package(Threads REQUIRED)
    target_link_libraries(ZorkLVGL PRIVATE Threads::Threads)

    # Math library
    target_link_libraries(ZorkLVGL PRIVATE m)
endif()
