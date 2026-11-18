cmake_minimum_required(VERSION 3.13)

project(RATELMonitor LANGUAGES C VERSION 1.0.0)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)

# --------------------------------------------------
# Core library
# --------------------------------------------------

add_library(ratelmonitor
    src/ratelmonitor.c
    src/ratelmonitor_drm.c
    src/ratelmonitor_kfd.c
    src/ratelmonitor_sampling.c
    src/metrics/ratelmonitor_gpu_metrics.c
    src/metrics/ratelmonitor_gpu_sysfs.c
    src/daemon/ratelmonitor_daemon.c
)

target_include_directories(ratelmonitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/metrics
)

target_link_libraries(ratelmonitor PUBLIC Threads::Threads)

target_compile_options(ratelmonitor PRIVATE -Wall -Wextra -O3)

# --------------------------------------------------
# Preload shared library
# --------------------------------------------------

add_library(ratelmonitor_preload SHARED
    cli/ratelmonitor_preload.c
)

target_link_libraries(ratelmonitor_preload PRIVATE ratelmonitor)
target_compile_options(ratelmonitor_preload PRIVATE -Wall -Wextra -O3)

set_target_properties(ratelmonitor_preload PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "ratelmonitor_preload"
)

# --------------------------------------------------
# Embed preload library into an ELF object
# --------------------------------------------------

set(EMBEDDED_OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/ratelmonitor_preload_embedded.o
)

add_custom_command(
    OUTPUT ${EMBEDDED_OBJECT}

    COMMAND ${CMAKE_LINKER}
            -r
            -b binary
            $<TARGET_FILE_NAME:ratelmonitor_preload>
            -o ${EMBEDDED_OBJECT}

    COMMAND ${CMAKE_OBJCOPY}
            --add-section .note.GNU-stack=/dev/null
            ${EMBEDDED_OBJECT}

    DEPENDS ratelmonitor_preload

    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}

    COMMENT "Embedding ratelmonitor_preload.so"
    VERBATIM
)

add_custom_target(ratelmonitor_preload_embedded
    DEPENDS ${EMBEDDED_OBJECT}
)

# --------------------------------------------------
# Main CLI
# --------------------------------------------------

add_executable(ratelmon
    cli/ratelmonitor_cli.c
    cli/ratelmonitor_cli.h
    ${EMBEDDED_OBJECT}
)

add_dependencies(ratelmon ratelmonitor_preload_embedded)

target_link_libraries(ratelmon PRIVATE ratelmonitor)

# --------------------------------------------------
# Installation
# --------------------------------------------------

install(TARGETS
    ratelmonitor
    ratelmon

    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/
    DESTINATION include/ratelmonitor
    FILES_MATCHING PATTERN "*.h"
)