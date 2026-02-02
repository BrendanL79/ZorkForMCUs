cmake_minimum_required (VERSION 3.21.1)

project(ZorkUI VERSION 0.0.1 LANGUAGES C CXX ASM)

# Display profile selection
# Must be set before find_package(Qul) to affect board defaults
set(DISPLAY_PROFILE "RT1050" CACHE STRING "Display profile (RT1050, RT1170, or RT1170_SCALED)")
set_property(CACHE DISPLAY_PROFILE PROPERTY STRINGS "RT1050" "RT1170" "RT1170_SCALED")

# Use custom board defaults for virtual keyboard width
if(DISPLAY_PROFILE STREQUAL "RT1170")
    set(QUL_PLATFORM_BOARD_DEFAULTS_QMLPROJECTCONFIG
        "${CMAKE_CURRENT_SOURCE_DIR}/BoardDefaults_720vkb.qmlprojectconfig")
elseif(DISPLAY_PROFILE STREQUAL "RT1170_SCALED")
    set(QUL_PLATFORM_BOARD_DEFAULTS_QMLPROJECTCONFIG
        "${CMAKE_CURRENT_SOURCE_DIR}/BoardDefaults_540vkb.qmlprojectconfig")
elseif(DISPLAY_PROFILE STREQUAL "RT1050")
    set(QUL_PLATFORM_BOARD_DEFAULTS_QMLPROJECTCONFIG
        "${CMAKE_CURRENT_SOURCE_DIR}/BoardDefaults_480vkb.qmlprojectconfig")
endif()

find_package(Qul)

# Path to project root (for libfizmo, src/, and shared cmake modules)
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Include shared fizmo/common CMake definitions
include("${PROJECT_ROOT}/cmake/FizmoCommon.cmake")

# =============================================================================
# Build Mode Auto-Detection
# =============================================================================
# Automatically detect build mode from platform for OOTB experience.
# Override with -DBUILD_WITH_FREERTOS=OFF or -DBUILD_WITH_FIZMO=OFF if needed.

# Auto-detect FreeRTOS from platform name (e.g., mimxrt1050-evk-freertos)
if(QUL_PLATFORM MATCHES "freertos")
    option(BUILD_WITH_FREERTOS "Build with FreeRTOS support (embedded target)" ON)
else()
    option(BUILD_WITH_FREERTOS "Build with FreeRTOS support (embedded target)" OFF)
endif()

# For desktop builds, default to real fizmo interpreter (not stub)
if(NOT BUILD_WITH_FREERTOS)
    option(BUILD_WITH_FIZMO "Build with real fizmo interpreter (desktop)" ON)
else()
    option(BUILD_WITH_FIZMO "Build with real fizmo interpreter (desktop)" OFF)
endif()

# Diagnostic output - helps users verify correct configuration
if(BUILD_WITH_FREERTOS)
    message(STATUS "Build mode: FreeRTOS with embedded fizmo interpreter")
elseif(BUILD_WITH_FIZMO)
    message(STATUS "Build mode: Desktop with fizmo interpreter (std::thread)")
else()
    message(STATUS "Build mode: Desktop STUB (UI testing only, no game logic)")
endif()

# Verify libfizmo submodule exists (required for non-stub builds)
if(BUILD_WITH_FREERTOS OR BUILD_WITH_FIZMO)
    if(NOT EXISTS "${PROJECT_ROOT}/external/libfizmo/src/interpreter/fizmo.c")
        message(FATAL_ERROR
            "libfizmo not found at ${PROJECT_ROOT}/external/libfizmo\n"
            "Run: git submodule update --init --recursive")
    endif()
endif()

# Determine which qmlproject to use based on display profile
# This applies to all build types (FreeRTOS, desktop with fizmo, desktop stub)
if(DISPLAY_PROFILE STREQUAL "RT1050")
    set(QML_PROJECT_FILE "ZorkUI_RT1050.qmlproject")
    message(STATUS "Using RT1050-optimized qmlproject (ZorkKeyboard, Static fonts)")
else()
    set(QML_PROJECT_FILE "ZorkUI.qmlproject")
    message(STATUS "Using standard qmlproject (VirtualKeyboard, Spark fonts)")
endif()

if(BUILD_WITH_FREERTOS)

    # FreeRTOS build - provide custom main.cpp

    add_compile_definitions(configTOTAL_HEAP_SIZE=\(1024*1024\))

    qul_add_target(ZorkUI
        FizmoBackend.cpp
        main_freertos.cpp
        ${FIZMO_COMMON_SOURCES}
        ${FIZMO_RTOS_SOURCES}
        QML_PROJECT "${QML_PROJECT_FILE}"
        SELECTORS "zork"
    )

    # Apply shared include dirs, compile defs, and compat header
    fizmo_common_apply(ZorkUI)

    # FatFS include directory
    target_include_directories(ZorkUI PRIVATE
        ${QUL_BOARD_SDK_DIR}/middleware/fatfs/source
    )

    # Add libfizmo sources
    target_sources(ZorkUI PRIVATE
        ${FIZMO_INTERPRETER_SOURCES}
        ${FIZMO_TOOLS_SOURCES}
    )

    # Add FatFS sources from SDK
    set(FATFS_SOURCES
        "${QUL_BOARD_SDK_DIR}/middleware/fatfs/source/ff.c"
        "${QUL_BOARD_SDK_DIR}/middleware/fatfs/source/ffsystem.c"
        "${QUL_BOARD_SDK_DIR}/middleware/fatfs/source/ffunicode.c"
    )
    target_sources(ZorkUI PRIVATE ${FATFS_SOURCES})

    target_compile_definitions(ZorkUI PRIVATE
        STORY_FILE_PATH="${PROJECT_ROOT}/zork1.z3"
    )

    # Enable Link-Time Optimization for smaller binary size
    set_property(TARGET ZorkUI PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

    target_link_options(ZorkUI PRIVATE
        -Wl,--wrap=malloc
        -Wl,--wrap=free
        -Wl,--wrap=calloc
        -Wl,--wrap=realloc
        -Wl,--wrap=strdup
        -Wl,--wrap=strndup
        -Wl,--wrap=_malloc_r
        -Wl,--wrap=_free_r
        -Wl,--wrap=_calloc_r
        -Wl,--wrap=_realloc_r
    )

elseif(BUILD_WITH_FIZMO)
    # Desktop build with real fizmo interpreter via std::thread
    qul_add_target(ZorkUI
        FizmoBackend.cpp
        ${FIZMO_COMMON_SOURCES}
        ${FIZMO_DESKTOP_BRIDGE_SOURCES}
        QML_PROJECT "${QML_PROJECT_FILE}"
        SELECTORS "zork"
        GENERATE_ENTRYPOINT
    )

    # Apply shared include dirs, compile defs, and compat header
    fizmo_common_apply(ZorkUI)

    # Add libfizmo sources (desktop needs filesys_c.c)
    target_sources(ZorkUI PRIVATE
        ${FIZMO_INTERPRETER_SOURCES}
        ${FIZMO_TOOLS_SOURCES}
        ${FIZMO_TOOLS_DESKTOP_EXTRAS}
        ${FIZMO_LOCALE_STUBS}
    )

    target_compile_definitions(ZorkUI PRIVATE
        USE_FIZMO_BRIDGE=1
        ZORK_STORY_PATH="${PROJECT_ROOT}/zork1.z3"
    )

    # Threading support
    find_package(Threads REQUIRED)
    target_link_libraries(ZorkUI PRIVATE Threads::Threads)

else()
    # Desktop build - use generated entrypoint for testing UI
    qul_add_target(ZorkUI
        FizmoBackend.cpp
        ${FIZMO_COMMON_SOURCES}
        ${FIZMO_STUB_SOURCES}
        QML_PROJECT "${QML_PROJECT_FILE}"
        SELECTORS "zork"
        GENERATE_ENTRYPOINT
    )

    # Apply shared include dirs (needed for common headers)
    fizmo_common_apply(ZorkUI)

    # Desktop stub - fizmo functions return dummy values
    target_compile_definitions(ZorkUI PRIVATE
        DESKTOP_STUB=1
    )
endif()

# Display profile compile definition (applies to all build types)
fizmo_apply_display_profile(ZorkUI "${DISPLAY_PROFILE}")

app_target_setup_os(ZorkUI)
