/**
 * @file bench_framework.c
 * @brief Lightweight C benchmarking framework implementation
 */

#include "bench_framework.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if FC_OS_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/time.h>
#    include <time.h>
#endif

#if FC_OS_WINDOWS

fc_bench_time_t fc_bench_time_now(void) {
    fc_bench_time_t t;
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    t.ticks = counter.QuadPart;
    (void) freq;
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

#    include <mach/mach_time.h>

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
    t.ticks = (uint64_t) ts.tv_sec * 1000000000ULL + ts.tv_nsec;
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
    return (double) diff / freq.QuadPart;
#elif FC_OS_MACOS
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return (double) diff * info.numer / info.denom / 1e9;
#else
    (void) start;
    (void) end;
    return (double) diff / 1e9;
#endif
}

double fc_bench_time_elapsed_ms(const fc_bench_time_t* start, const fc_bench_time_t* end) {
    return fc_bench_time_elapsed(start, end) * 1000.0;
}

uint64_t fc_bench_time_elapsed_ns(const fc_bench_time_t* start, const fc_bench_time_t* end) {
#if FC_OS_WINDOWS
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (uint64_t) ((end->ticks - start->ticks) * 1e9 / freq.QuadPart);
#elif FC_OS_MACOS
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return (end->ticks - start->ticks) * info.numer / info.denom;
#else
    return end->ticks - start->ticks;
#endif
}

void fc_bench_result_print(const fc_bench_result_t* result) {
    /* Go-style compact output format with adaptive time units */
    printf("%-50s\t%10lu\t", result->name, (unsigned long) result->iterations);

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
    if (fp == NULL)
        fp = stdout;

    fprintf(
        fp,
        "%s,%zu,%lu,%.6f,%.2f,%.2f",
        result->name,
        result->data_size,
        (unsigned long) result->iterations,
        result->elapsed_ms,
        result->mean_ns,
        result->stddev_ns
    );

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
    if (a->mean_ns == 0)
        return 0;
    return b->mean_ns - a->mean_ns;
}

double fc_bench_result_speedup(
    const fc_bench_result_t* baseline,
    const fc_bench_result_t* optimized
) {
    if (optimized->mean_ns == 0)
        return 0;
    return baseline->mean_ns / optimized->mean_ns;
}

void fc_bench_result_print_comparison(
    const char* name,
    const fc_bench_result_t* baseline,
    const fc_bench_result_t* optimized
) {
    double speedup         = fc_bench_result_speedup(baseline, optimized);
    double improvement_pct = (1.0 - (optimized->mean_ns / baseline->mean_ns)) * 100.0;

    printf("\n");
    printf("============================================================\n");
    printf("Benchmark Comparison: %s\n", name);
    printf("============================================================\n");
    printf(
        "Baseline:  %.2f ns/op (%lu iterations)\n",
        baseline->mean_ns,
        (unsigned long) baseline->iterations
    );
    printf(
        "Optimized: %.2f ns/op (%lu iterations)\n",
        optimized->mean_ns,
        (unsigned long) optimized->iterations
    );
    printf("------------------------------------------------------------\n");
    printf("Speedup:   %.2fx faster\n", speedup);
    printf("Improvement: %.1f%%\n", improvement_pct);

    if (baseline->throughput_gb_s > 0 && optimized->throughput_gb_s > 0) {
        printf(
            "Throughput: %.2f MB/s -> %.2f MB/s\n",
            baseline->throughput_gb_s * 1024.0,
            optimized->throughput_gb_s * 1024.0
        );
    }

    printf("============================================================\n");
}

int fc_bench_result_save(const fc_bench_result_t* result, const char* filename) {
    if (result == NULL || filename == NULL)
        return -1;

    FILE* fp = fopen(filename, "w");
    if (fp == NULL)
        return -1;

    fprintf(fp, "name=%s\n", result->name ? result->name : "");
    fprintf(fp, "data_size=%zu\n", result->data_size);
    fprintf(fp, "elapsed_ms=%.6f\n", result->elapsed_ms);
    fprintf(fp, "iterations=%lu\n", (unsigned long) result->iterations);
    fprintf(fp, "ops_per_sec=%.2f\n", result->ops_per_sec);
    fprintf(fp, "throughput_gb_s=%.6f\n", result->throughput_gb_s);
    fprintf(fp, "gflops=%.6f\n", result->gflops);
    fprintf(fp, "mean_ns=%.6f\n", result->mean_ns);
    fprintf(fp, "stddev_ns=%.6f\n", result->stddev_ns);
    fprintf(fp, "min_ns=%.6f\n", result->min_ns);
    fprintf(fp, "max_ns=%.6f\n", result->max_ns);
    fprintf(fp, "bytes_per_op=%zu\n", result->bytes_per_op);
    fprintf(fp, "allocs_per_op=%zu\n", result->allocs_per_op);

    fclose(fp);
    return 0;
}

int fc_bench_result_load(fc_bench_result_t* result, const char* filename) {
    if (result == NULL || filename == NULL)
        return -1;

    FILE* fp = fopen(filename, "r");
    if (fp == NULL)
        return -1;

    memset(result, 0, sizeof(*result));

    char line[512];
    static char name_buffer[256] = {0};

    while (fgets(line, sizeof(line), fp) != NULL) {
        char key[64];
        char value[256];

        if (sscanf(line, "%63[^=]=%255[^\n]", key, value) == 2) {
            if (strcmp(key, "name") == 0) {
                strncpy(name_buffer, value, sizeof(name_buffer) - 1);
                name_buffer[sizeof(name_buffer) - 1] = '\0';
                result->name                         = name_buffer;
            } else if (strcmp(key, "data_size") == 0) {
                result->data_size = (size_t) strtoull(value, NULL, 10);
            } else if (strcmp(key, "elapsed_ms") == 0) {
                result->elapsed_ms = strtod(value, NULL);
            } else if (strcmp(key, "iterations") == 0) {
                result->iterations = strtoull(value, NULL, 10);
            } else if (strcmp(key, "mean_ns") == 0) {
                result->mean_ns = strtod(value, NULL);
            } else if (strcmp(key, "stddev_ns") == 0) {
                result->stddev_ns = strtod(value, NULL);
            } else if (strcmp(key, "min_ns") == 0) {
                result->min_ns = strtod(value, NULL);
            } else if (strcmp(key, "max_ns") == 0) {
                result->max_ns = strtod(value, NULL);
            } else if (strcmp(key, "throughput_gb_s") == 0) {
                result->throughput_gb_s = strtod(value, NULL);
            } else if (strcmp(key, "gflops") == 0) {
                result->gflops = strtod(value, NULL);
            } else if (strcmp(key, "bytes_per_op") == 0) {
                result->bytes_per_op = (size_t) strtoull(value, NULL, 10);
            } else if (strcmp(key, "allocs_per_op") == 0) {
                result->allocs_per_op = (size_t) strtoull(value, NULL, 10);
            }
        }
    }

    fclose(fp);
    return 0;
}

double fc_bench_throughput_gb_s(size_t bytes, double elapsed_ms) {
    if (elapsed_ms <= 0)
        return 0;
    return (bytes / (1024.0 * 1024.0 * 1024.0)) / (elapsed_ms / 1000.0);
}

double fc_bench_gflops(double flops, double elapsed_ms) {
    if (elapsed_ms <= 0)
        return 0;
    return flops / (elapsed_ms / 1000.0) / 1e9;
}

double fc_bench_ops_per_sec(uint64_t ops, double elapsed_ms) {
    if (elapsed_ms <= 0)
        return 0;
    return ops / (elapsed_ms / 1000.0);
}

void fc_bench_stats_init(fc_bench_stats_t* stats) {
    stats->count    = 0;
    stats->mean     = 0;
    stats->m2       = 0;
    stats->min      = 0;
    stats->max      = 0;
    stats->samples  = NULL;
    stats->capacity = 0;
}

void fc_bench_stats_add(fc_bench_stats_t* stats, double value) {
    if (stats->count == 0) {
        stats->min = value;
        stats->max = value;
    } else {
        if (value < stats->min)
            stats->min = value;
        if (value > stats->max)
            stats->max = value;
    }

    stats->count++;
    double delta = value - stats->mean;
    stats->mean += delta / stats->count;
    double delta2 = value - stats->mean;
    stats->m2 += delta * delta2;

    /* Store sample for percentile calculation */
    if (stats->count > stats->capacity) {
        size_t new_capacity = stats->capacity == 0 ? 128 : stats->capacity * 2;
        double* new_samples = (double*) realloc(stats->samples, new_capacity * sizeof(double));
        if (new_samples != NULL) {
            stats->samples  = new_samples;
            stats->capacity = new_capacity;
        }
    }

    if (stats->samples != NULL && stats->count <= stats->capacity) {
        stats->samples[stats->count - 1] = value;
    }
}

double fc_bench_stats_mean(const fc_bench_stats_t* stats) {
    return stats->mean;
}

double fc_bench_stats_stddev(const fc_bench_stats_t* stats) {
    if (stats->count < 2)
        return 0;
    return sqrt(stats->m2 / (stats->count - 1));
}

double fc_bench_stats_variance(const fc_bench_stats_t* stats) {
    if (stats->count < 2)
        return 0;
    return stats->m2 / (stats->count - 1);
}

static int compare_double(const void* a, const void* b) {
    double da = *(const double*) a;
    double db = *(const double*) b;
    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

double fc_bench_stats_median(const fc_bench_stats_t* stats) {
    return fc_bench_stats_percentile(stats, 50.0);
}

double fc_bench_stats_percentile(const fc_bench_stats_t* stats, double percentile) {
    if (stats->count == 0 || stats->samples == NULL)
        return 0;

    if (percentile < 0)
        percentile = 0;
    if (percentile > 100)
        percentile = 100;

    /* Create a sorted copy of samples */
    double* sorted = (double*) malloc(stats->count * sizeof(double));
    if (sorted == NULL)
        return 0;

    memcpy(sorted, stats->samples, stats->count * sizeof(double));
    qsort(sorted, stats->count, sizeof(double), compare_double);

    double index  = (percentile / 100.0) * (stats->count - 1);
    size_t lower  = (size_t) index;
    size_t upper  = lower + 1;
    double weight = index - lower;

    double result;
    if (upper >= stats->count) {
        result = sorted[lower];
    } else {
        result = sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    }

    free(sorted);
    return result;
}

void fc_bench_stats_free(fc_bench_stats_t* stats) {
    if (stats->samples != NULL) {
        free(stats->samples);
        stats->samples  = NULL;
        stats->capacity = 0;
    }
}

void fc_bench_stats_print(const fc_bench_stats_t* stats, const char* name) {
    printf("%s statistics:\n", name);
    printf(
        "  mean=%.2f ns, stddev=%.2f ns\n", fc_bench_stats_mean(stats), fc_bench_stats_stddev(stats)
    );
    printf("  min=%.2f ns, max=%.2f ns\n", stats->min, stats->max);
    printf("  median=%.2f ns\n", fc_bench_stats_median(stats));
    printf(
        "  p95=%.2f ns, p99=%.2f ns\n",
        fc_bench_stats_percentile(stats, 95.0),
        fc_bench_stats_percentile(stats, 99.0)
    );
    printf("  n=%lu\n", (unsigned long) stats->count);
}

double fc_bench_mem_bandwidth_gb_s(size_t bytes_read, size_t bytes_written, double elapsed_ms) {
    size_t total_bytes = bytes_read + bytes_written;
    return fc_bench_throughput_gb_s(total_bytes, elapsed_ms);
}

static int g_verbose                  = 1;
static FILE* g_output_file            = NULL;
static const char* g_filter_pattern   = NULL;
static int g_track_allocations        = 0;
static size_t g_total_bytes_allocated = 0;
static size_t g_total_allocs          = 0;

#define FC_BENCH_MAX_SUITES 64

static const fc_bench_suite_t* g_suites[FC_BENCH_MAX_SUITES];
static int g_num_suites = 0;

void fc_bench_set_verbose(int verbose) {
    g_verbose = verbose;
}

void fc_bench_set_filter(const char* pattern) {
    g_filter_pattern = pattern;
}

void fc_bench_track_allocations(int enable) {
    g_track_allocations = enable;
    if (enable) {
        g_total_bytes_allocated = 0;
        g_total_allocs          = 0;
    }
}

void fc_bench_get_allocation_stats(size_t* bytes_per_op, size_t* allocs_per_op) {
    if (bytes_per_op) {
        *bytes_per_op = g_total_bytes_allocated;
    }
    if (allocs_per_op) {
        *allocs_per_op = g_total_allocs;
    }
}

/* Allocation tracking hooks - these would need to be integrated with a custom allocator
 * or memory profiling tool. For now, we provide the interface for manual tracking. */
void fc_bench_record_alloc(size_t bytes) {
    if (g_track_allocations) {
        g_total_bytes_allocated += bytes;
        g_total_allocs++;
    }
}

void fc_bench_set_output_file(const char* filename) {
    if (g_output_file != NULL && g_output_file != stdout) {
        fclose(g_output_file);
    }
    if (filename != NULL) {
        g_output_file = fopen(filename, "w");
        if (g_output_file != NULL) {
            fprintf(
                g_output_file,
                "name,data_size,iterations,time_ms,mean_ns,stddev_ns,throughput_gb_s,gflops\n"
            );
        }
    } else {
        g_output_file = NULL;
    }
}

static void fc_bench_warmup(fc_bench_fn fn, void* user_data, double warmup_ms) {
    if (warmup_ms <= 0)
        return;

    fc_bench_time_t start = fc_bench_time_now();
    double elapsed        = 0;

    while (elapsed < warmup_ms) {
        fn(user_data);
        fc_bench_time_t now = fc_bench_time_now();
        elapsed             = fc_bench_time_elapsed_ms(&start, &now);
    }
}

void fc_bench_run(
    const fc_bench_config_t* config,
    fc_bench_fn fn,
    void* user_data,
    fc_bench_result_t* result
) {
    if (config == NULL || fn == NULL || result == NULL)
        return;

    /* Check filter pattern if set */
    if (g_filter_pattern != NULL && config->name != NULL) {
        if (strstr(config->name, g_filter_pattern) == NULL) {
            /* Pattern not found, skip this benchmark */
            return;
        }
    }

    memset(result, 0, sizeof(*result));
    result->name      = config->name;
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
    double elapsed_ms   = fc_bench_time_elapsed_ms(&start, &end);

    /* Adjust iterations if needed */
    if (elapsed_ms < config->min_time_ms && iterations < config->max_iterations) {
        uint64_t target_iters = (uint64_t) (config->min_time_ms / (elapsed_ms / iterations));
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
        double pass_elapsed_ms   = fc_bench_time_elapsed_ms(&pass_start, &pass_end);
        double pass_mean_ns      = pass_elapsed_ms * 1e6 / iterations;

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
        result->min_ns    = stats.min;
        result->max_ns    = stats.max;
    }

    /* Free statistics resources */
    if (config->enable_stats) {
        fc_bench_stats_free(&stats);
    }

    /* Calculate derived metrics */
    if (result->data_size > 0 && result->elapsed_ms > 0) {
        result->throughput_gb_s =
            fc_bench_throughput_gb_s(result->data_size * result->iterations, result->elapsed_ms);
    }

    /* Get allocation stats if tracking is enabled */
    if (g_track_allocations && result->iterations > 0) {
        result->bytes_per_op  = g_total_bytes_allocated / result->iterations;
        result->allocs_per_op = g_total_allocs / result->iterations;
    }

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
    g_verbose        = 1;
    g_output_file    = NULL;
    g_filter_pattern = NULL;

    /* Print table header once at initialization */
    fc_bench_print_header();
}

static void print_usage(const char* program) {
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -v                  Verbose output (default)\n");
    printf("  -q                  Quiet mode\n");
    printf("  -filter <pattern>   Run only benchmarks matching pattern\n");
    printf("  -output <file>      Write results to CSV file\n");
    printf("  -iterations <n>     Set minimum iterations\n");
    printf("  -time <ms>          Set minimum run time in milliseconds\n");
    printf("  -h, -help           Show this help message\n");
}

void fc_bench_init_with_args(int argc, char** argv) {
    g_verbose        = 1;
    g_output_file    = NULL;
    g_filter_pattern = NULL;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            g_verbose = 1;
        } else if (strcmp(argv[i], "-q") == 0) {
            g_verbose = 0;
        } else if (strcmp(argv[i], "-filter") == 0 && i + 1 < argc) {
            g_filter_pattern = argv[++i];
        } else if (strcmp(argv[i], "-output") == 0 && i + 1 < argc) {
            fc_bench_set_output_file(argv[++i]);
        } else if (strcmp(argv[i], "-iterations") == 0 && i + 1 < argc) {
            /* Note: iterations setting would need to be passed to fc_bench_config_t */
            i++;
        } else if (strcmp(argv[i], "-time") == 0 && i + 1 < argc) {
            /* Note: time setting would need to be passed to fc_bench_config_t */
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            exit(1);
        }
    }

    fc_bench_print_header();
}

void fc_bench_cleanup(void) {
    if (g_output_file != NULL && g_output_file != stdout) {
        fclose(g_output_file);
        g_output_file = NULL;
    }
}

void fc_bench_register_suite(const fc_bench_suite_t* suite) {
    if (suite == NULL || g_num_suites >= FC_BENCH_MAX_SUITES) {
        return;
    }
    g_suites[g_num_suites++] = suite;
}

void fc_bench_run_all(void) {
    printf("\n");
    printf("============================================================\n");
    printf("Running all benchmark suites (%d suites)\n", g_num_suites);
    printf("============================================================\n");

    fc_bench_print_header();

    for (int i = 0; i < g_num_suites; i++) {
        const fc_bench_suite_t* suite = g_suites[i];

        printf("\nSuite: %s\n", suite->name);
        if (suite->description != NULL) {
            printf("  %s\n", suite->description);
        }
        printf("------------------------------------------------------------\n");

        if (suite->setup != NULL) {
            suite->setup();
        }

        /* Note: Individual benchmarks in the suite must be run manually
         * by calling fc_bench_run() for each benchmark function.
         * This design allows flexibility in benchmark organization. */

        if (suite->teardown != NULL) {
            suite->teardown();
        }
    }

    printf("\n");
}

static int g_bench_simd_level_override = -1;

int fc_bench_set_simd_level(int level) {
    int prev                    = g_bench_simd_level_override;
    g_bench_simd_level_override = level;
    return prev;
}

int fc_bench_get_simd_level(void) {
    return g_bench_simd_level_override;
}

void fc_bench_compare_simd(
    const char* name,
    const fc_bench_config_t* config,
    fc_bench_fn fn,
    void* user_data
) {
    const char* simd_names[] = {"Scalar", "SSE4.2", "AVX2", "AVX-512"};
    const int simd_levels[]  = {0, 1, 2, 3};
    const int num_levels     = 4;

    printf("\n");
    printf("============================================================\n");
    printf("SIMD Comparison: %s\n", name);
    printf("============================================================\n");

    fc_bench_result_t results[4];
    fc_bench_result_t baseline = {0};

    fc_bench_print_header();

    for (int i = 0; i < num_levels; i++) {
        int prev_level = fc_bench_set_simd_level(simd_levels[i]);

        fc_bench_config_t level_config = *config;
        char level_name[128];
        snprintf(level_name, sizeof(level_name), "%s [%s]", name, simd_names[i]);
        level_config.name = level_name;

        fc_bench_run(&level_config, fn, user_data, &results[i]);

        if (i == 0) {
            baseline = results[i];
        }

        fc_bench_set_simd_level(prev_level);
    }

    printf("\n");
    printf("Speedup relative to Scalar:\n");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < num_levels; i++) {
        double speedup = baseline.mean_ns / results[i].mean_ns;
        printf(
            "  %-10s: %.2fx speedup (%.2f ns/op vs %.2f ns/op)\n",
            simd_names[i],
            speedup,
            results[i].mean_ns,
            baseline.mean_ns
        );
    }

    printf("============================================================\n");
}
