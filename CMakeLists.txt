cmake_minimum_required(VERSION 3.16)
project(finkit-codec VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include SIMD detection and compiler flags
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DetectSIMD.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerFlags.cmake)

# Sanitizer configuration
if(FC_ENABLE_SANITIZERS)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers.cmake)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers.cmake)
    endif()
endif()

# Generate cgo_source.go with detected SIMD flags
if(FC_SIMD_LEVEL STREQUAL "AVX512")
    set(FC_SIMD_CFLAGS "${FC_AVX512_FLAGS}")
elseif(FC_SIMD_LEVEL STREQUAL "AVX2")
    set(FC_SIMD_CFLAGS "${FC_AVX2_FLAGS}")
elseif(FC_SIMD_LEVEL STREQUAL "SSE42")
    set(FC_SIMD_CFLAGS "${FC_SSE42_FLAGS}")
else()
    set(FC_SIMD_CFLAGS "")
endif()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cgo_source.go.in
    ${CMAKE_CURRENT_SOURCE_DIR}/cgo_source.go
    @ONLY
)

# Use prebuilt platform library
if(NOT TARGET finkit::platform)
    # Detect platform
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(PLATFORM_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/platform/build/darwin_arm64")
        else()
            set(PLATFORM_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/platform/build/darwin_x86_64")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(PLATFORM_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/platform/build/linux_amd64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(PLATFORM_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/platform/build/windows_amd64")
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    # Import prebuilt platform library
    add_library(finkit_platform_static STATIC IMPORTED)
    set_target_properties(finkit_platform_static PROPERTIES
        IMPORTED_LOCATION "${PLATFORM_BUILD_DIR}/libfinkit_platform_static.a"
    )
    target_include_directories(finkit_platform_static INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/modules/platform/include"
    )
    add_library(finkit::platform ALIAS finkit_platform_static)
endif()

# Codec module sources
set(CODEC_SOURCES
    codec-c/fix_codec.c
)

# Create object library
add_library(finkit_codec OBJECT ${CODEC_SOURCES})

target_include_directories(finkit_codec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/modules/platform/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(finkit_codec PRIVATE FC_STATIC_BUILD)

# Set compile flags for SIMD variants based on detected level
if(FC_SIMD_LEVEL STREQUAL "AVX512")
    set_source_files_properties(codec-c/fix_codec.c PROPERTIES COMPILE_FLAGS "${FC_AVX512_FLAGS}")
elseif(FC_SIMD_LEVEL STREQUAL "AVX2")
    set_source_files_properties(codec-c/fix_codec.c PROPERTIES COMPILE_FLAGS "${FC_AVX2_FLAGS}")
elseif(FC_SIMD_LEVEL STREQUAL "SSE42")
    set_source_files_properties(codec-c/fix_codec.c PROPERTIES COMPILE_FLAGS "${FC_SSE42_FLAGS}")
endif()

# Create static library
add_library(finkit_codec_static STATIC $<TARGET_OBJECTS:finkit_codec>)
target_include_directories(finkit_codec_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link platform and math library
target_link_libraries(finkit_codec_static PUBLIC finkit::platform m)

# Create alias
add_library(finkit::codec ALIAS finkit_codec_static)

# Install
install(TARGETS finkit_codec_static
    EXPORT finkit-codec-targets
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/codec
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT finkit-codec-targets
    FILE finkit-codecTargets.cmake
    NAMESPACE finkit::
    DESTINATION lib/cmake/finkit-codec
)

# Config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/finkit-codecConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finkit-codecConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/finkit-codecConfig.cmake"
    INSTALL_DESTINATION lib/cmake/finkit-codec
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/finkit-codecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/finkit-codecConfigVersion.cmake"
    DESTINATION lib/cmake/finkit-codec
)

# Tests
option(FC_BUILD_TESTS "Build tests" ON)
if(FC_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
option(FC_BUILD_BENCHMARKS "Build benchmarks" ON)
if(FC_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
endif()

# Print configuration
message(STATUS "")
message(STATUS "============================================================")
message(STATUS " finkit-codec ${PROJECT_VERSION}")
message(STATUS "============================================================")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "============================================================")
message(STATUS "")
