cmake_minimum_required(VERSION 3.20)

project(
    fmcw_radar_detector
    VERSION 0.1.0
    DESCRIPTION "C target detector module for the FMCW radar mini project"
    LANGUAGES C
)

# Use the C11 standard.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build a shared library by default:
# Linux   -> libradar_detector.so
# Windows -> radar_detector.dll
# macOS   -> libradar_detector.dylib
option(BUILD_SHARED_LIBS "Build radar detector as a shared library" ON)

# Allow C tests to be disabled temporarily.
option(BUILD_C_TESTS "Build C unit tests" OFF)

add_library(
    radar_detector
    csrc/detector.c
)

target_include_directories(
    radar_detector
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/csrc
)

set_target_properties(
    radar_detector
    PROPERTIES
        OUTPUT_NAME "radar_detector"
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# Enable strict compiler warnings.
if(MSVC)
    target_compile_options(
        radar_detector
        PRIVATE
            /W4
    )
else()
    target_compile_options(
        radar_detector
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
    )
endif()

# Optional C test executable.
if(BUILD_C_TESTS)
    enable_testing()

    add_executable(
        detector_c_test
        ctests/test_detector.c
    )

    target_link_libraries(
        detector_c_test
        PRIVATE
            radar_detector
    )

    add_test(
        NAME detector_c_test
        COMMAND detector_c_test
    )
endif()