cmake_minimum_required(VERSION 3.21)
project(ZorkSlint C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

set(LIBFIZMO_DIR "${PROJECT_ROOT}/external/libfizmo/src")
set(LIBFIZMO_SOURCES
    "${LIBFIZMO_DIR}/interpreter/blockbuf.c"
    "${LIBFIZMO_DIR}/interpreter/config.c"
    "${LIBFIZMO_DIR}/interpreter/fizmo.c"
    "${LIBFIZMO_DIR}/interpreter/mathemat.c"
    "${LIBFIZMO_DIR}/interpreter/misc.c"
    "${LIBFIZMO_DIR}/interpreter/mt19937ar.c"
    "${LIBFIZMO_DIR}/interpreter/object.c"
    "${LIBFIZMO_DIR}/interpreter/output.c"
    "${LIBFIZMO_DIR}/interpreter/property.c"
    "${LIBFIZMO_DIR}/interpreter/routine.c"
    "${LIBFIZMO_DIR}/interpreter/sound.c"
    "${LIBFIZMO_DIR}/interpreter/stack.c"
    "${LIBFIZMO_DIR}/interpreter/streams.c"
    "${LIBFIZMO_DIR}/interpreter/table.c"
    "${LIBFIZMO_DIR}/interpreter/undo.c"
    "${LIBFIZMO_DIR}/interpreter/variable.c"
    "${LIBFIZMO_DIR}/interpreter/wordwrap.c"
    "${LIBFIZMO_DIR}/interpreter/zpu.c"
    "${LIBFIZMO_DIR}/interpreter/iff.c"
    "${LIBFIZMO_DIR}/tools/filesys.c"
    "${LIBFIZMO_DIR}/tools/filesys_c.c"
    "${LIBFIZMO_DIR}/tools/i18n.c"
    "${LIBFIZMO_DIR}/tools/list.c"
    "${LIBFIZMO_DIR}/tools/stringmap.c"
    "${LIBFIZMO_DIR}/tools/tracelog.c"
    "${LIBFIZMO_DIR}/tools/types.c"
    "${LIBFIZMO_DIR}/tools/z_ucs.c"
    "${PROJECT_ROOT}/src/fizmo_locale_stubs.c"
    "${PROJECT_ROOT}/src/fizmo_bridge.cpp"
    "${PROJECT_ROOT}/src/msvc_shims/dirent_msvc.c"
)

# --- Patch libfizmo's two VLAs into fixed-size buffers WITHOUT touching the
# --- pinned submodule: read each pristine file, string-replace only the VLA
# --- line, write a patched copy into the build tree, compile the copy.
set(_patched_dir "${CMAKE_BINARY_DIR}/patched")
file(MAKE_DIRECTORY "${_patched_dir}")

function(patch_vla src_abs out_var from to)
    file(READ "${src_abs}" _content)
    set(_original "${_content}")
    string(REPLACE "${from}" "${to}" _content "${_content}")
    if(_content STREQUAL _original)
        message(FATAL_ERROR "patch_vla: VLA pattern not found in ${src_abs}\n  expected substring: ${from}\n  (did the libfizmo pin change? the VLA workaround must be updated)")
    endif()
    get_filename_component(_name "${src_abs}" NAME)
    set(_dst "${_patched_dir}/${_name}")
    file(WRITE "${_dst}" "${_content}")
    set(${out_var} "${_dst}" PARENT_SCOPE)
endfunction()

patch_vla("${LIBFIZMO_DIR}/interpreter/savegame.c" SAVEGAME_PATCHED
    "z_ucs filename[buffer_len + 1];"
    "z_ucs filename[MAXIMUM_SAVEGAME_NAME_LENGTH + 1]; if (buffer_len > MAXIMUM_SAVEGAME_NAME_LENGTH) buffer_len = MAXIMUM_SAVEGAME_NAME_LENGTH;")
patch_vla("${LIBFIZMO_DIR}/interpreter/text.c" TEXT_PATCHED
    "uint8_t tokenize_buffer[tokenize_buffer_length];"
    "uint8_t tokenize_buffer[6];")

add_library(zorkfizmo STATIC ${LIBFIZMO_SOURCES})
target_sources(zorkfizmo PRIVATE ${SAVEGAME_PATCHED} ${TEXT_PATCHED})

target_include_directories(zorkfizmo BEFORE PRIVATE
    "${PROJECT_ROOT}/src/msvc_shims"
    "${PROJECT_ROOT}/src"
)
target_include_directories(zorkfizmo PRIVATE
    "${LIBFIZMO_DIR}"
    # The patched VLA copies live in the build tree but use includes relative to
    # their original location (interpreter/), e.g. "../tools/tracelog.h". Add the
    # original interpreter dir so those relative paths resolve to the submodule.
    "${LIBFIZMO_DIR}/interpreter"
)

target_compile_definitions(zorkfizmo PRIVATE
    USE_FIZMO_BRIDGE=1
    __WIN32__=1
    DISABLE_BABEL=1
    DISABLE_FILELIST=1
    DISABLE_CONFIGFILES=1
    DISABLE_COMMAND_HISTORY=1
    DISABLE_OUTPUT_HISTORY=1
    DISABLE_PREFIX_COMMANDS=1
    DISABLE_BLOCKBUFFER=1
    _CRT_SECURE_NO_WARNINGS=1
    ZORK_STORY_PATH="${PROJECT_ROOT}/zork1.z3"
)

target_compile_options(zorkfizmo PRIVATE
    "/FI${PROJECT_ROOT}/src/fizmo_msvc_compat.h"
)

find_package(Threads REQUIRED)
target_link_libraries(zorkfizmo PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Slint C++ (prebuilt MSVC/ARM64 package) + the ZorkSlint application.
# ---------------------------------------------------------------------------
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/slint-install")
find_package(Slint REQUIRED)

add_executable(ZorkSlint WIN32
    app/main.cpp
    app/fizmo_slint_bridge.cpp
    app/utf8.cpp
)
target_include_directories(ZorkSlint PRIVATE
    "${PROJECT_ROOT}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
)
target_compile_definitions(ZorkSlint PRIVATE
    ZORK_STORY_PATH="${PROJECT_ROOT}/zork1.z3"
)

# CascadiaCode font path — used by main.cpp to register the font with Slint so
# that "Cascadia Code" resolves even without the font being pre-installed.
# Default: system-installed location on Windows.  Override at configure time:
#   cmake ... -DSLINT_FONT_PATH="C:/path/to/CascadiaCode.ttf"
if(NOT DEFINED SLINT_FONT_PATH)
    set(SLINT_FONT_PATH "C:/Windows/Fonts/CascadiaCode.ttf")
endif()
target_compile_definitions(ZorkSlint PRIVATE
    SLINT_FONT_PATH="${SLINT_FONT_PATH}"
)
target_link_libraries(ZorkSlint PRIVATE zorkfizmo Slint::Slint Threads::Threads)
slint_target_sources(ZorkSlint app/zork.slint)

# Copy the Slint runtime DLL next to the executable so it runs in place.
add_custom_command(TARGET ZorkSlint POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/slint-install/lib/slint_cpp.dll"
        "$<TARGET_FILE_DIR:ZorkSlint>"
    VERBATIM)

# WIN32 (GUI subsystem, no console) makes the linker look for WinMain, but we use
# a plain main(). Point the entry at the CRT startup that calls main() instead.
target_link_options(ZorkSlint PRIVATE "/ENTRY:mainCRTStartup")

# ---------------------------------------------------------------------------
# Unit test: UTF-32 -> UTF-8 encoder (pure, no fizmo/Slint dependency).
# ---------------------------------------------------------------------------
enable_testing()
add_executable(test_utf8 test/test_utf8.cpp app/utf8.cpp)
target_include_directories(test_utf8 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app")
add_test(NAME test_utf8 COMMAND test_utf8)
