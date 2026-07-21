#
# tests/CMakeLists.txt - Test configuration for codec module
#

# Test executable
add_executable(codec_tests
    test_framework.c
    test_main.c
    test_codec.c
    test_fix_codec.c
)

# Link with merged codec library (includes all dependencies)
target_link_libraries(codec_tests
    PRIVATE
        finkit_codec_static_interface
        m
)

target_compile_definitions(codec_tests PRIVATE
    FC_STATIC_BUILD
)

# Include directories
target_include_directories(codec_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/modules/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags for tests
target_compile_options(codec_tests PRIVATE -Wall -Wextra -Wpedantic)

# Run tests with ctest
add_test(
    NAME "codec_tests"
    COMMAND codec_tests
)

# Installation
install(TARGETS codec_tests
    RUNTIME DESTINATION bin
)
