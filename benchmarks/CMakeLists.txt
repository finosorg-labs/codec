#
# benchmarks/CMakeLists.txt - Benchmark configuration for codec module
#

# Benchmark framework library
add_library(bench_framework OBJECT
    bench_framework.c
)

target_include_directories(bench_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/modules/platform/include
)

target_link_libraries(bench_framework
    PUBLIC
        finkit_codec_static_base
        finkit::platform
)

# Benchmark executable
add_executable(codec_benchmarks
    bench_main.c
    bench_codec.c
    bench_fix_codec.c
)

# Link with benchmark framework and codec library
target_link_libraries(codec_benchmarks
    PRIVATE
        bench_framework
        finkit_codec_static_base
        finkit::platform
        m
)

target_compile_definitions(codec_benchmarks PRIVATE
    FC_STATIC_BUILD
)

# Include directories
target_include_directories(codec_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/modules/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags for benchmarks
target_compile_options(codec_benchmarks PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)

# Installation
install(TARGETS codec_benchmarks
    RUNTIME DESTINATION bin
)
