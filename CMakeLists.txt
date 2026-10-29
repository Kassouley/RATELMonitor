cmake_minimum_required(VERSION 3.13)

project(RATELMonitor LANGUAGES C VERSION 1.0.0)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)

add_library(ratelmonitor
    src/ratelmonitor.c
    src/ratelmonitor_drm.c
    src/ratelmonitor_kfd.c
    src/ratelmonitor_sampling.c
    src/metrics/ratelmonitor_gpu_metrics.c
    src/metrics/ratelmonitor_gpu_sysfs.c
)

target_include_directories(ratelmonitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/metrics
)

target_link_libraries(ratelmonitor PUBLIC Threads::Threads)

target_compile_options(ratelmonitor PRIVATE -Wall -Wextra -O2)

add_executable(ratelmonitor_cli cli/ratelmonitor_cli.c cli/ratelmonitor_cli.h)
target_link_libraries(ratelmonitor_cli PRIVATE ratelmonitor)

install(TARGETS ratelmonitor ratelmonitor_cli
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/
    DESTINATION include/ratelmonitor
    FILES_MATCHING PATTERN "*.h"
)
