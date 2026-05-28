/**
 * @file bench_framework.c
 * @brief Lightweight C benchmarking framework implementation
 */

#include "bench_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if FC_OS_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <time.h>
#endif

/*
 * High-resolution timing implementation
*/

#if FC_OS_WINDOWS

fc_bench_time_t fc_bench_time_now(void) {
    fc_bench_time_t t;
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    t.ticks = counter.QuadPart;
    (void)freq;
    return t;
}

uint64_t fc_bench_get_timer_resolution_ns(void) {
    LARGE_INTEGER freq;
    if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0) {
        return 1000000000ULL / freq.QuadPart;
    }
    return 1000;
}

#elif FC_OS_MACOS

#include <mach/mach_time.h>

fc_bench_time_t fc_bench_time_now(void) {
    fc_bench_time_t t;
    t.ticks = mach_absolute_time();
    return t;
}

uint64_t fc_bench_get_timer_resolution_ns(void) {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return info.numer / info.denom;
}

#else /* Linux and other Unix */

fc_bench_time_t fc_bench_time_now(void) {
    fc_bench_time_t t;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t.ticks = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return t;
}

uint64_t fc_bench_get_timer_resolution_ns(void) {
    return 1; /* nanosecond resolution with clock_gettime */
}

#endif

double fc_bench_time_elapsed(const fc_bench_time_t* start, const fc_bench_time_t* end) {
    uint64_t diff = end->ticks - start->ticks;
#if FC_OS_WINDOWS
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (double)diff / freq.QuadPart;
#elif FC_OS_MACOS
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return (double)diff * info.numer / info.denom / 1e9;
#else
    (void)start;
    (void)end;
    return (double)diff / 1e9;
#endif
}

double fc_bench_time_elapsed_ms(const fc_bench_time_t* start, const fc_bench_time_t* end) {
    return fc_bench_time_elapsed(start, end) * 1000.0;
}

uint64_t fc_bench_time_elapsed_ns(const fc_bench_time_t* start, const fc_bench_time_t* end) {
#if FC_OS_WINDOWS
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (uint64_t)((end->ticks - start->ticks) * 1e9 / freq.QuadPart);
#elif FC_OS_MACOS
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return (end->ticks - start->ticks) * info.numer / info.denom;
#else
    return end->ticks - start->ticks;
#endif
}

/*
 * Benchmark result functions
*/

void fc_bench_result_print(const fc_bench_result_t* result) {
    /* Go-style compact output format with adaptive time units */
    printf("%-50s\t%10lu\t",
           result->name,
           (unsigned long)result->iterations);

    /* Adaptive time unit display */
    if (result->mean_ns >= 1000000.0) {
        /* >= 1ms: show in milliseconds */
        printf("%12.2f ms/op", result->mean_ns / 1000000.0);
    } else if (result->mean_ns >= 1000.0) {
        /* >= 1μs: show in microseconds */
        printf("%12.2f μs/op", result->mean_ns / 1000.0);
    } else {
        /* < 1μs: show in nanoseconds */
        printf("%12.2f ns/op", result->mean_ns);
    }

    if (result->throughput_gb_s > 0) {
        printf("\t%10.2f MB/s", result->throughput_gb_s * 1024.0);
    }

    if (result->gflops > 0) {
        printf("\t%10.2f GFLOPS", result->gflops);
    }

    /* Only show memory allocation metrics if non-zero */
    if (result->bytes_per_op > 0 || result->allocs_per_op > 0) {
        printf("\t%8zu B/op\t%8zu allocs/op", result->bytes_per_op, result->allocs_per_op);
    }

    printf("\n");
}

void fc_bench_print_header(void) {
    printf("%-50s\t%10s\t%12s\t%12s\n", "Benchmark", "Iterations", "Time/op", "Throughput");
    printf("%-50s\t%10s\t%12s\t%12s\n", "---------", "----------", "-------", "----------");
}

void fc_bench_result_print_csv(const fc_bench_result_t* result, FILE* fp) {
    if (fp == NULL) fp = stdout;

    fprintf(fp, "%s,%zu,%lu,%.6f,%.2f,%.2f",
            result->name,
            result->data_size,
            (unsigned long)result->iterations,
            result->elapsed_ms,
            result->mean_ns,
            result->stddev_ns);

    if (result->throughput_gb_s > 0) {
        fprintf(fp, ",%.6f", result->throughput_gb_s);
    } else {
        fprintf(fp, ",");
    }

    if (result->gflops > 0) {
        fprintf(fp, ",%.6f", result->gflops);
    } else {
        fprintf(fp, ",");
    }

    fprintf(fp, "\n");
}

double fc_bench_result_compare(const fc_bench_result_t* a, const fc_bench_result_t* b) {
    if (a->mean_ns == 0) return 0;
    return b->mean_ns - a->mean_ns;
}

/*
 * Throughput and performance calculations
*/

double fc_bench_throughput_gb_s(size_t bytes, double elapsed_ms) {
    if (elapsed_ms <= 0) return 0;
    return (bytes / (1024.0 * 1024.0 * 1024.0)) / (elapsed_ms / 1000.0);
}

double fc_bench_gflops(double flops, double elapsed_ms) {
    if (elapsed_ms <= 0) return 0;
    return flops / (elapsed_ms / 1000.0) / 1e9;
}

double fc_bench_ops_per_sec(uint64_t ops, double elapsed_ms) {
    if (elapsed_ms <= 0) return 0;
    return ops / (elapsed_ms / 1000.0);
}

/*
 * Statistical functions (Welford's online algorithm)
*/

void fc_bench_stats_init(fc_bench_stats_t* stats) {
    stats->count = 0;
    stats->mean = 0;
    stats->m2 = 0;
    stats->min = 0;
    stats->max = 0;
}

void fc_bench_stats_add(fc_bench_stats_t* stats, double value) {
    if (stats->count == 0) {
        stats->min = value;
        stats->max = value;
    } else {
        if (value < stats->min) stats->min = value;
        if (value > stats->max) stats->max = value;
    }

    stats->count++;
    double delta = value - stats->mean;
    stats->mean += delta / stats->count;
    double delta2 = value - stats->mean;
    stats->m2 += delta * delta2;
}

double fc_bench_stats_mean(const fc_bench_stats_t* stats) {
    return stats->mean;
}

double fc_bench_stats_stddev(const fc_bench_stats_t* stats) {
    if (stats->count < 2) return 0;
    return sqrt(stats->m2 / (stats->count - 1));
}

double fc_bench_stats_variance(const fc_bench_stats_t* stats) {
    if (stats->count < 2) return 0;
    return stats->m2 / (stats->count - 1);
}

void fc_bench_stats_print(const fc_bench_stats_t* stats, const char* name) {
    printf("%s: mean=%.2f ns, stddev=%.2f ns, min=%.2f ns, max=%.2f ns, n=%lu\n",
           name,
           fc_bench_stats_mean(stats),
           fc_bench_stats_stddev(stats),
           stats->min,
           stats->max,
           (unsigned long)stats->count);
}

/*
 * Memory bandwidth estimation
*/

double fc_bench_mem_bandwidth_gb_s(
    size_t bytes_read,
    size_t bytes_written,
    double elapsed_ms
) {
    size_t total_bytes = bytes_read + bytes_written;
    return fc_bench_throughput_gb_s(total_bytes, elapsed_ms);
}

/*
 * Benchmark runner
*/

static int g_verbose = 1;
static FILE* g_output_file = NULL;

void fc_bench_set_verbose(int verbose) {
    g_verbose = verbose;
}

void fc_bench_set_output_file(const char* filename) {
    if (g_output_file != NULL && g_output_file != stdout) {
        fclose(g_output_file);
    }
    if (filename != NULL) {
        g_output_file = fopen(filename, "w");
        if (g_output_file != NULL) {
            fprintf(g_output_file, "name,data_size,iterations,time_ms,mean_ns,stddev_ns,throughput_gb_s,gflops\n");
        }
    } else {
        g_output_file = NULL;
    }
}

static void fc_bench_warmup(
    fc_bench_fn fn,
    void* user_data,
    double warmup_ms
) {
    if (warmup_ms <= 0) return;

    fc_bench_time_t start = fc_bench_time_now();
    double elapsed = 0;

    while (elapsed < warmup_ms) {
        fn(user_data);
        fc_bench_time_t now = fc_bench_time_now();
        elapsed = fc_bench_time_elapsed_ms(&start, &now);
    }
}

void fc_bench_run(
    const fc_bench_config_t* config,
    fc_bench_fn fn,
    void* user_data,
    fc_bench_result_t* result
) {
    if (config == NULL || fn == NULL || result == NULL) return;

    memset(result, 0, sizeof(*result));
    result->name = config->name;
    result->data_size = config->data_size;

    /* Warmup */
    fc_bench_warmup(fn, user_data, config->warmup_ms);

    /* Determine number of iterations */
    uint64_t iterations = config->min_iterations;

    /* Quick calibration run */
    fc_bench_time_t start = fc_bench_time_now();
    for (uint64_t i = 0; i < iterations; i++) {
        fn(user_data);
    }
    fc_bench_time_t end = fc_bench_time_now();
    double elapsed_ms = fc_bench_time_elapsed_ms(&start, &end);

    /* Adjust iterations if needed */
    if (elapsed_ms < config->min_time_ms && iterations < config->max_iterations) {
        uint64_t target_iters = (uint64_t)(config->min_time_ms / (elapsed_ms / iterations));
        if (target_iters > iterations && target_iters <= config->max_iterations) {
            iterations = target_iters;
        }
    }

    /* Main measurement run */
    fc_bench_stats_t stats = {0};
    if (config->enable_stats) {
        fc_bench_stats_init(&stats);
    }

    /* Multiple measurement passes */
    const int num_passes = config->enable_stats ? 5 : 1;

    for (int pass = 0; pass < num_passes; pass++) {
        fc_bench_time_t pass_start = fc_bench_time_now();
        for (uint64_t i = 0; i < iterations; i++) {
            fn(user_data);
        }
        fc_bench_time_t pass_end = fc_bench_time_now();
        double pass_elapsed_ms = fc_bench_time_elapsed_ms(&pass_start, &pass_end);
        double pass_mean_ns = pass_elapsed_ms * 1e6 / iterations;

        if (config->enable_stats) {
            fc_bench_stats_add(&stats, pass_mean_ns);
        }

        result->elapsed_ms += pass_elapsed_ms;
    }

    result->iterations = iterations;
    result->elapsed_ms /= num_passes;
    result->mean_ns = result->elapsed_ms * 1e6 / iterations;

    if (config->enable_stats && stats.count > 0) {
        result->stddev_ns = fc_bench_stats_stddev(&stats);
        result->min_ns = stats.min;
        result->max_ns = stats.max;
    }

    /* Calculate derived metrics */
    if (result->data_size > 0 && result->elapsed_ms > 0) {
        result->throughput_gb_s = fc_bench_throughput_gb_s(result->data_size * result->iterations, result->elapsed_ms);
    }

    /* Note: bytes_per_op and allocs_per_op should be set by the benchmark
     * if it performs memory allocations. They default to 0 (no allocations). */

    /* Print result */
    if (!config->quiet && g_verbose) {
        fc_bench_result_print(result);
    }

    /* Write to CSV file */
    if (g_output_file != NULL) {
        fc_bench_result_print_csv(result, g_output_file);
        fflush(g_output_file);
    }
}

void fc_bench_init(void) {
    g_verbose = 1;
    g_output_file = NULL;

    /* Print table header once at initialization */
    fc_bench_print_header();
}

void fc_bench_cleanup(void) {
    if (g_output_file != NULL && g_output_file != stdout) {
        fclose(g_output_file);
        g_output_file = NULL;
    }
}

void fc_bench_register_suite(const fc_bench_suite_t* suite) {
    (void)suite;
    /* TODO: Implement suite registration */
}

void fc_bench_run_all(void) {
    /* TODO: Implement suite runner */
}
