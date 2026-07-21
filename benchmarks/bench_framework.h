/**
 * @file bench_framework.h
 * @brief Lightweight C benchmarking framework for fin-kit
 *
 * Provides performance measurement utilities:
 * - High-resolution timing
 * - Throughput calculation
 * - Statistical analysis (mean, stddev, min, max)
 * - Memory bandwidth estimation
 * - FLOPS calculation
 */

#ifndef FC_BENCH_FRAMEWORK_H
#define FC_BENCH_FRAMEWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <platform.h>

#define FC_BENCH_VERSION "1.0.0"

/**
 * @brief High-resolution timestamp
 */
typedef struct {
    uint64_t ticks;
} fc_bench_time_t;

/**
 * @brief Get current high-resolution time
 */
fc_bench_time_t fc_bench_time_now(void);

/**
 * @brief Get elapsed time in seconds
 */
double fc_bench_time_elapsed(const fc_bench_time_t* start, const fc_bench_time_t* end);

/**
 * @brief Get elapsed time in milliseconds
 */
double fc_bench_time_elapsed_ms(const fc_bench_time_t* start, const fc_bench_time_t* end);

/**
 * @brief Get elapsed time in nanoseconds
 */
uint64_t fc_bench_time_elapsed_ns(const fc_bench_time_t* start, const fc_bench_time_t* end);

/**
 * @brief Get timer resolution in nanoseconds
 */
uint64_t fc_bench_get_timer_resolution_ns(void);

/**
 * @brief Statistics for a benchmark run
 */
typedef struct {
    const char* name;
    size_t data_size;       /* bytes processed */
    double elapsed_ms;      /* total elapsed time */
    uint64_t iterations;    /* number of iterations */
    double ops_per_sec;     /* operations per second */
    double throughput_gb_s; /* gigabytes per second */
    double gflops;          /* gigaflops (if applicable) */
    double mean_ns;         /* mean time per iteration */
    double stddev_ns;       /* standard deviation */
    double min_ns;          /* minimum time */
    double max_ns;          /* maximum time */
    size_t bytes_per_op;    /* bytes allocated per operation */
    size_t allocs_per_op;   /* number of allocations per operation */
} fc_bench_result_t;

/**
 * @brief Print benchmark result to stdout
 */
void fc_bench_result_print(const fc_bench_result_t* result);

/**
 * @brief Print benchmark result in CSV format
 */
void fc_bench_result_print_csv(const fc_bench_result_t* result, FILE* fp);

/**
 * @brief Compare two benchmark results
 *
 * @return positive if a is faster, negative if b is faster
 */
double fc_bench_result_compare(const fc_bench_result_t* a, const fc_bench_result_t* b);

/**
 * @brief Calculate speedup ratio between two results
 *
 * @param baseline Baseline result
 * @param optimized Optimized result
 * @return Speedup ratio (baseline/optimized)
 */
double fc_bench_result_speedup(
    const fc_bench_result_t* baseline,
    const fc_bench_result_t* optimized
);

/**
 * @brief Print comparison of two benchmark results
 *
 * @param name Comparison name
 * @param baseline Baseline result
 * @param optimized Optimized result
 */
void fc_bench_result_print_comparison(
    const char* name,
    const fc_bench_result_t* baseline,
    const fc_bench_result_t* optimized
);

/**
 * @brief Save benchmark result to file
 *
 * @param result Result to save
 * @param filename Output filename
 * @return 0 on success, non-zero on error
 */
int fc_bench_result_save(const fc_bench_result_t* result, const char* filename);

/**
 * @brief Load benchmark result from file
 *
 * @param result Output result structure
 * @param filename Input filename
 * @return 0 on success, non-zero on error
 */
int fc_bench_result_load(fc_bench_result_t* result, const char* filename);

/**
 * @brief Benchmark configuration
 */
typedef struct {
    const char* name;
    size_t data_size;        /* Size of data being processed */
    uint64_t min_iterations; /* Minimum iterations to run */
    uint64_t max_iterations; /* Maximum iterations to run */
    double min_time_ms;      /* Minimum total time to run */
    double warmup_ms;        /* Warmup time before measurement */
    int enable_stats;        /* Enable statistical analysis */
    int quiet;               /* Suppress output */
} fc_bench_config_t;

/**
 * @brief Default benchmark configuration
 */
#define FC_BENCH_CONFIG_DEFAULT                                                                    \
    {                                                                                              \
        .name = "benchmark", .data_size = 0, .min_iterations = 10, .max_iterations = 1000000,      \
        .min_time_ms = 100.0, .warmup_ms = 10.0, .enable_stats = 1, .quiet = 0                     \
    }

/**
 * @brief Benchmark function type
 */
typedef void (*fc_bench_fn)(void* user_data);

/**
 * @brief Run a benchmark
 *
 * @param config Benchmark configuration
 * @param fn Benchmark function to measure
 * @param user_data User data passed to benchmark function
 * @param result Output result structure
 */
void fc_bench_run(
    const fc_bench_config_t* config,
    fc_bench_fn fn,
    void* user_data,
    fc_bench_result_t* result
);

/**
 * @brief Print benchmark table header
 *
 * Call this once before running a series of benchmarks to print column headers.
 */
void fc_bench_print_header(void);

/**
 * @brief Simple benchmark with fixed iterations
 */
#define FC_BENCH_SIMPLE(name, n_iterations, code)                                                  \
    do {                                                                                           \
        fc_bench_time_t _start, _end;                                                              \
        _start = fc_bench_time_now();                                                              \
        for (int _i = 0; _i < (n_iterations); _i++) {                                              \
            code;                                                                                  \
        }                                                                                          \
        _end               = fc_bench_time_now();                                                  \
        double _elapsed_ms = fc_bench_time_elapsed_ms(&_start, &_end);                             \
        printf(                                                                                    \
            "%s: %.2f ms (%d iterations, %.2f ns/iter)\n",                                         \
            name,                                                                                  \
            _elapsed_ms,                                                                           \
            n_iterations,                                                                          \
            _elapsed_ms * 1000000.0 / (n_iterations)                                               \
        );                                                                                         \
    } while (0)

/**
 * @brief Benchmark with auto-iteration adjustment
 */
#define FC_BENCH_AUTO(name, code)                                                                  \
    do {                                                                                           \
        fc_bench_time_t _start, _end;                                                              \
        int _iterations = 1;                                                                       \
        _start          = fc_bench_time_now();                                                     \
        code;                                                                                      \
        _end               = fc_bench_time_now();                                                  \
        double _elapsed_ms = fc_bench_time_elapsed_ms(&_start, &_end);                             \
        if (_elapsed_ms < 10.0) {                                                                  \
            _iterations = (int) (10.0 / _elapsed_ms) + 1;                                          \
            _start      = fc_bench_time_now();                                                     \
            for (int _i = 0; _i < _iterations; _i++) {                                             \
                code;                                                                              \
            }                                                                                      \
            _end        = fc_bench_time_now();                                                     \
            _elapsed_ms = fc_bench_time_elapsed_ms(&_start, &_end);                                \
        }                                                                                          \
        printf(                                                                                    \
            "%s: %.2f ms (%d iterations, %.2f ns/iter)\n",                                         \
            name,                                                                                  \
            _elapsed_ms,                                                                           \
            _iterations,                                                                           \
            _elapsed_ms * 1000000.0 / (_iterations)                                                \
        );                                                                                         \
    } while (0)

/**
 * @brief Calculate throughput in GB/s
 *
 * @param bytes Number of bytes processed
 * @param elapsed_ms Elapsed time in milliseconds
 * @return Throughput in GB/s
 */
double fc_bench_throughput_gb_s(size_t bytes, double elapsed_ms);

/**
 * @brief Calculate GFLOPS
 *
 * @param flops Number of floating-point operations
 * @param elapsed_ms Elapsed time in milliseconds
 * @return GFLOPS
 */
double fc_bench_gflops(double flops, double elapsed_ms);

/**
 * @brief Calculate operations per second
 *
 * @param ops Number of operations
 * @param elapsed_ms Elapsed time in milliseconds
 * @return Ops per second
 */
double fc_bench_ops_per_sec(uint64_t ops, double elapsed_ms);

/**
 * @brief Running statistics accumulator
 */
typedef struct {
    uint64_t count;
    double mean;
    double m2; /* sum of squared differences */
    double min;
    double max;
    double* samples; /* for percentile calculation */
    size_t capacity;
} fc_bench_stats_t;

/**
 * @brief Initialize statistics accumulator
 */
void fc_bench_stats_init(fc_bench_stats_t* stats);

/**
 * @brief Add a sample to statistics
 */
void fc_bench_stats_add(fc_bench_stats_t* stats, double value);

/**
 * @brief Get mean value
 */
double fc_bench_stats_mean(const fc_bench_stats_t* stats);

/**
 * @brief Get standard deviation
 */
double fc_bench_stats_stddev(const fc_bench_stats_t* stats);

/**
 * @brief Get variance
 */
double fc_bench_stats_variance(const fc_bench_stats_t* stats);

/**
 * @brief Get median value
 */
double fc_bench_stats_median(const fc_bench_stats_t* stats);

/**
 * @brief Get percentile value
 *
 * @param stats Statistics accumulator
 * @param percentile Percentile to get (0-100)
 * @return Percentile value
 */
double fc_bench_stats_percentile(const fc_bench_stats_t* stats, double percentile);

/**
 * @brief Free statistics resources
 */
void fc_bench_stats_free(fc_bench_stats_t* stats);

/**
 * @brief Print statistics summary
 */
void fc_bench_stats_print(const fc_bench_stats_t* stats, const char* name);

/**
 * @brief Estimate memory bandwidth in GB/s
 *
 * @param bytes_read Bytes read from memory
 * @param bytes_written Bytes written to memory
 * @param elapsed_ms Elapsed time in milliseconds
 * @return Estimated bandwidth in GB/s
 */
double fc_bench_mem_bandwidth_gb_s(size_t bytes_read, size_t bytes_written, double elapsed_ms);

/**
 * @brief Benchmark suite structure
 */
typedef struct {
    const char* name;
    const char* description;
    void (*setup)(void);
    void (*teardown)(void);
} fc_bench_suite_t;

/**
 * @brief Register a benchmark suite
 */
void fc_bench_register_suite(const fc_bench_suite_t* suite);

/**
 * @brief Run all registered benchmarks
 */
void fc_bench_run_all(void);

/**
 * @brief Set benchmark output file (CSV format)
 */
void fc_bench_set_output_file(const char* filename);

/**
 * @brief Enable/disable verbose output
 */
void fc_bench_set_verbose(int verbose);

/**
 * @brief Initialize benchmark framework
 */
void fc_bench_init(void);

/**
 * @brief Initialize benchmark framework with command-line arguments
 *
 * @param argc Argument count
 * @param argv Argument vector
 */
void fc_bench_init_with_args(int argc, char** argv);

/**
 * @brief Set filter pattern for benchmarks
 *
 * @param pattern Filter pattern (substring match)
 */
void fc_bench_set_filter(const char* pattern);

/**
 * @brief Cleanup benchmark framework
 */
void fc_bench_cleanup(void);

/**
 * @brief Enable automatic memory allocation tracking
 *
 * When enabled, tracks malloc/free calls during benchmark execution.
 *
 * @param enable 1 to enable, 0 to disable
 */
void fc_bench_track_allocations(int enable);

/**
 * @brief Get tracked allocation statistics
 *
 * @param bytes_per_op Output: bytes allocated per operation
 * @param allocs_per_op Output: number of allocations per operation
 */
void fc_bench_get_allocation_stats(size_t* bytes_per_op, size_t* allocs_per_op);

/**
 * @brief Override SIMD level for benchmarking
 *
 * Allows benchmarking different SIMD implementations by forcing a specific level.
 *
 * @param level SIMD level to force (0=scalar, 1=SSE4.2, 2=AVX2, 3=AVX-512)
 * @return Previous SIMD level
 */
int fc_bench_set_simd_level(int level);

/**
 * @brief Get current SIMD level
 *
 * @return Current SIMD level
 */
int fc_bench_get_simd_level(void);

/**
 * @brief Compare performance across SIMD levels
 *
 * Runs a benchmark at different SIMD levels and prints comparison.
 *
 * @param name Benchmark name
 * @param config Benchmark configuration
 * @param fn Benchmark function
 * @param user_data User data
 */
void fc_bench_compare_simd(
    const char* name,
    const fc_bench_config_t* config,
    fc_bench_fn fn,
    void* user_data
);

#endif /* FC_BENCH_FRAMEWORK_H */
