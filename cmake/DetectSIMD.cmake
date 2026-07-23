#
# DetectSIMD.cmake - CPU SIMD capability detection
#
# Detects build machine CPU capabilities and populates FC_SIMD_LEVEL
# Always compiles all SIMD variants; runtime dispatch selects appropriate one
#

include(CheckCSourceCompiles)

# Save original flags
set(_orig_cmake_required_flags "${CMAKE_REQUIRED_FLAGS}")

# Detect SSE4.2
set(CMAKE_REQUIRED_FLAGS "${_orig_cmake_required_flags} -msse4.2")
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m128i a = _mm_setzero_si128();
        return _mm_extract_epi32(a, 0);
    }
" FC_CPU_HAS_SSE42)

# Detect AVX2
set(CMAKE_REQUIRED_FLAGS "${_orig_cmake_required_flags} -mavx2 -mfma")
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m256i a = _mm256_setzero_si256();
        return _mm256_extract_epi32(a, 0);
    }
" FC_CPU_HAS_AVX2)

# Detect AVX-512
set(CMAKE_REQUIRED_FLAGS "${_orig_cmake_required_flags} -mavx512f -mavx512dq")
check_c_source_compiles("
    #include <immintrin.h>
    int main() {
        __m512i a = _mm512_setzero_si512();
        return _mm512_extract_epi32(a, 0);
    }
" FC_CPU_HAS_AVX512)

# Restore original flags
set(CMAKE_REQUIRED_FLAGS "${_orig_cmake_required_flags}")

# Determine the highest SIMD level supported by build machine
if(FC_CPU_HAS_AVX512)
    set(FC_SIMD_LEVEL "AVX512")
elseif(FC_CPU_HAS_AVX2)
    set(FC_SIMD_LEVEL "AVX2")
elseif(FC_CPU_HAS_SSE42)
    set(FC_SIMD_LEVEL "SSE42")
else()
    set(FC_SIMD_LEVEL "SCALAR")
endif()

# Promote to cache so it persists across re-runs and is visible to IDEs
set(FC_SIMD_LEVEL "${FC_SIMD_LEVEL}" CACHE STRING "Highest supported SIMD instruction set")
message(STATUS "SIMD level: ${FC_SIMD_LEVEL}")

# Force enable all SIMD levels for compilation
# This ensures all SIMD variants are compiled regardless of build machine CPU
# Runtime dispatch (fc_get_simd_level) will select the appropriate variant
set(FC_HAS_SSE42 1)
set(FC_HAS_AVX2 1)
set(FC_HAS_AVX512 1)
message(STATUS "Compiling all SIMD variants: SSE4.2, AVX2, AVX-512")
