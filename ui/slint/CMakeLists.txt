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
    "${LIBFIZMO_DIR}/interpreter/savegame.c"
    "${LIBFIZMO_DIR}/interpreter/sound.c"
    "${LIBFIZMO_DIR}/interpreter/stack.c"
    "${LIBFIZMO_DIR}/interpreter/streams.c"
    "${LIBFIZMO_DIR}/interpreter/table.c"
    "${LIBFIZMO_DIR}/interpreter/text.c"
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

add_library(zorkfizmo STATIC ${LIBFIZMO_SOURCES})

target_include_directories(zorkfizmo BEFORE PRIVATE
    "${PROJECT_ROOT}/src/msvc_shims"
    "${PROJECT_ROOT}/src"
)
target_include_directories(zorkfizmo PRIVATE
    "${LIBFIZMO_DIR}"
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

add_executable(zork_smoke app/smoke_main.c)
target_include_directories(zork_smoke PRIVATE "${PROJECT_ROOT}/src")
target_compile_definitions(zork_smoke PRIVATE
    ZORK_STORY_PATH="${PROJECT_ROOT}/zork1.z3"
    _CRT_SECURE_NO_WARNINGS=1
)
target_link_libraries(zork_smoke PRIVATE zorkfizmo Threads::Threads)
