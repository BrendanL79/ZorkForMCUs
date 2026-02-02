#
# FizmoCommon.cmake
#
# Shared CMake definitions for building the fizmo interpreter and common UI
# code. Included by each UI toolkit's CMakeLists.txt.
#
# Expects the following variables to be set before including:
#   PROJECT_ROOT  - path to the repository root
#
# Provides the following variables:
#   FIZMO_COMMON_DIR            - path to ui/common/
#   FIZMO_COMMON_SOURCES        - common UI sources (FizmoTextBuffer, FizmoPoller)
#   FIZMO_STUB_SOURCES          - desktop stub source
#   FIZMO_INTERPRETER_SOURCES   - libfizmo interpreter .c files
#   FIZMO_TOOLS_SOURCES         - libfizmo tools .c files
#   FIZMO_LOCALE_STUBS          - locale stub source
#   FIZMO_RTOS_SOURCES          - RTOS-specific bridge and filesystem sources
#   FIZMO_DESKTOP_BRIDGE_SOURCES - desktop bridge source
#   FIZMO_COMPILE_DEFINITIONS   - compile definitions to disable unused features
#   FIZMO_EMBEDDED_COMPAT_HEADER - path to the embedded compatibility header
#
# Provides the following functions:
#   fizmo_common_apply(TARGET)  - apply include dirs, compile defs, and
#                                  compat header to a target
#

set(FIZMO_COMMON_DIR "${PROJECT_ROOT}/ui/common")

# ---- Common UI sources (toolkit-agnostic) ----

set(FIZMO_COMMON_SOURCES
    "${FIZMO_COMMON_DIR}/FizmoTextBuffer.c"
    "${FIZMO_COMMON_DIR}/FizmoPoller.c"
)

set(FIZMO_STUB_SOURCES
    "${FIZMO_COMMON_DIR}/fizmo_stub.c"
)

# ---- libfizmo interpreter sources ----
# Explicitly listed to exclude optional/problematic files
# (babel, blorb, cmd_hst, debugger, filelist, history, hyphenation)

set(FIZMO_INTERPRETER_SOURCES
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/blockbuf.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/config.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/fizmo.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/mathemat.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/misc.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/mt19937ar.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/object.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/output.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/property.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/routine.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/savegame.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/sound.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/stack.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/streams.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/table.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/text.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/undo.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/variable.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/wordwrap.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/zpu.c"
    "${PROJECT_ROOT}/external/libfizmo/src/interpreter/iff.c"
)

set(FIZMO_TOOLS_SOURCES
    "${PROJECT_ROOT}/external/libfizmo/src/tools/filesys.c"
    "${PROJECT_ROOT}/external/libfizmo/src/tools/i18n.c"
    "${PROJECT_ROOT}/external/libfizmo/src/tools/list.c"
    "${PROJECT_ROOT}/external/libfizmo/src/tools/stringmap.c"
    "${PROJECT_ROOT}/external/libfizmo/src/tools/tracelog.c"
    "${PROJECT_ROOT}/external/libfizmo/src/tools/types.c"
    "${PROJECT_ROOT}/external/libfizmo/src/tools/z_ucs.c"
)

# Desktop builds need filesys_c.c; embedded builds use fizmo_filesys_hybrid.c
set(FIZMO_TOOLS_DESKTOP_EXTRAS
    "${PROJECT_ROOT}/external/libfizmo/src/tools/filesys_c.c"
)

set(FIZMO_LOCALE_STUBS
    "${PROJECT_ROOT}/src/fizmo_locale_stubs.c"
)

# ---- RTOS-specific sources ----

set(FIZMO_RTOS_SOURCES
    "${PROJECT_ROOT}/src/fizmo_rtos_bridge.c"
    "${PROJECT_ROOT}/src/fizmo_filesys_hybrid.c"
    "${PROJECT_ROOT}/src/fizmo_locale_stubs.c"
    "${PROJECT_ROOT}/src/diskio_stub.c"
    "${PROJECT_ROOT}/src/posix_stubs.c"
    "${PROJECT_ROOT}/src/story_data.S"
)

# ---- Desktop bridge source ----

set(FIZMO_DESKTOP_BRIDGE_SOURCES
    "${PROJECT_ROOT}/src/fizmo_bridge.cpp"
)

# ---- Compile definitions (disable unused libfizmo features) ----

set(FIZMO_COMPILE_DEFINITIONS
    DISABLE_BABEL=1
    DISABLE_FILELIST=1
    DISABLE_CONFIGFILES=1
    DISABLE_COMMAND_HISTORY=1
    DISABLE_OUTPUT_HISTORY=1
    DISABLE_PREFIX_COMMANDS=1
    DISABLE_BLOCKBUFFER=1
)

# ---- Embedded compatibility header ----

set(FIZMO_EMBEDDED_COMPAT_HEADER
    "${PROJECT_ROOT}/src/fizmo_embedded_compat.h"
)

#
# fizmo_common_apply(TARGET)
#
# Applies standard include directories, compile definitions, and
# force-include of the embedded compatibility header to the given target.
#
function(fizmo_common_apply TARGET)
    # Include directories: src/ first (overrides libfizmo placeholders),
    # then ui/common, then libfizmo
    target_include_directories(${TARGET} BEFORE PRIVATE
        ${PROJECT_ROOT}/src
    )
    target_include_directories(${TARGET} PRIVATE
        ${FIZMO_COMMON_DIR}
        ${PROJECT_ROOT}/external/libfizmo/src
    )

    # Compile definitions
    target_compile_definitions(${TARGET} PRIVATE
        ${FIZMO_COMPILE_DEFINITIONS}
    )

    # Force-include compatibility header for C/C++ (not assembly)
    target_compile_options(${TARGET} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include "${FIZMO_EMBEDDED_COMPAT_HEADER}">
        $<$<COMPILE_LANGUAGE:CXX>:-include "${FIZMO_EMBEDDED_COMPAT_HEADER}">
    )
endfunction()

#
# fizmo_apply_display_profile(TARGET DISPLAY_PROFILE)
#
# Sets the display profile compile definition and prints a status message.
#
function(fizmo_apply_display_profile TARGET PROFILE)
    if(PROFILE STREQUAL "RT1170")
        target_compile_definitions(${TARGET} PRIVATE DISPLAY_RT1170=1)
        message(STATUS "Display profile: RT1170 (720x1280 portrait)")
    elseif(PROFILE STREQUAL "RT1170_SCALED")
        target_compile_definitions(${TARGET} PRIVATE DISPLAY_RT1170_SCALED=1)
        message(STATUS "Display profile: RT1170_SCALED (540x960 portrait, 75% scale)")
    else()
        target_compile_definitions(${TARGET} PRIVATE DISPLAY_RT1050=1)
        message(STATUS "Display profile: RT1050 (480x272 landscape)")
    endif()
endfunction()
