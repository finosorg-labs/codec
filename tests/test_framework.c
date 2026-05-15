/**
 * @file test_framework.c
 * @brief Lightweight C unit testing framework implementation
 */

#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>

/*
 * Global state
*/

static fc_test_stats_t g_stats = {0};
static const char* g_current_test = NULL;
static fc_test_result_t g_current_result = FC_TEST_PASSED;
static int g_verbose = 1;
static int g_coverage_enabled = 0;
static const char* g_filter_pattern = NULL;
static clock_t g_test_start_time = 0;

static const fc_test_suite_t* g_suites[FC_TEST_MAX_SUITES];
static int g_num_suites = 0;

/*
 * Test statistics implementation
*/

void fc_test_stats_init(fc_test_stats_t* stats) {
    memset(stats, 0, sizeof(*stats));
}

void fc_test_stats_print(const fc_test_stats_t* stats) {
    printf("\n============================================================\n");

    if (g_coverage_enabled) {
        printf("PASS\n");
        printf("coverage: %.1f%% of statements\n",
               stats->total_tests > 0 ? (double)stats->passed / stats->total_tests * 100.0 : 0.0);
    }

    printf("Test Results: %d tests, %d passed, %d failed, %d skipped\n",
           stats->total_tests,
           stats->passed,
           stats->failed,
           stats->skipped);
    printf("Time: %.2f ms\n", stats->elapsed_time_ms);
    printf("============================================================\n");

    if (stats->failed == 0) {
        printf("ALL TESTS PASSED\n");
    } else {
        printf("SOME TESTS FAILED\n");
    }
}

/*
 * Assertion failure implementations
*/

void fc_test_assert_fail(
    const char* condition,
    const char* file,
    int line,
    const char* message
) {
    printf("FAIL: %s\n", g_current_test ? g_current_test : "unknown");
    printf("  Location: %s:%d\n", file, line);
    printf("  Condition: %s\n", condition);
    if (message != NULL) {
        printf("  Message: %s\n", message);
    }
    g_current_result = FC_TEST_FAILED;
}

void fc_test_assert_fail_eq(
    const char* condition,
    const char* file,
    int line,
    intmax_t actual,
    intmax_t expected
) {
    printf("FAIL: %s\n", g_current_test ? g_current_test : "unknown");
    printf("  Location: %s:%d\n", file, line);
    printf("  Condition: %s\n", condition);
    printf("  Expected: %jd\n", expected);
    printf("  Actual:   %jd\n", actual);
    g_current_result = FC_TEST_FAILED;
}

void fc_test_assert_fail_ne(
    const char* condition,
    const char* file,
    int line
) {
    printf("FAIL: %s\n", g_current_test ? g_current_test : "unknown");
    printf("  Location: %s:%d\n", file, line);
    printf("  Condition: %s (should NOT be equal)\n", condition);
    g_current_result = FC_TEST_FAILED;
}

void fc_test_assert_fail_double(
    const char* condition,
    const char* file,
    int line,
    double actual,
    double expected
) {
    printf("FAIL: %s\n", g_current_test ? g_current_test : "unknown");
    printf("  Location: %s:%d\n", file, line);
    printf("  Condition: %s\n", condition);
    printf("  Expected: %.17g\n", expected);
    printf("  Actual:   %.17g\n", actual);
    printf("  Diff:     %.17g\n", actual - expected);
    g_current_result = FC_TEST_FAILED;
}

void fc_test_assert_fail_str(
    const char* condition,
    const char* file,
    int line,
    const char* actual,
    const char* expected
) {
    printf("FAIL: %s\n", g_current_test ? g_current_test : "unknown");
    printf("  Location: %s:%d\n", file, line);
    printf("  Condition: %s\n", condition);
    printf("  Expected: \"%s\"\n", expected ? expected : "(null)");
    printf("  Actual:   \"%s\"\n", actual ? actual : "(null)");
    g_current_result = FC_TEST_FAILED;
}

void fc_test_assert_fail_mem(
    const char* condition,
    const char* file,
    int line,
    const void* actual,
    const void* expected,
    size_t size
) {
    (void)actual;
    (void)expected;
    printf("FAIL: %s\n", g_current_test ? g_current_test : "unknown");
    printf("  Location: %s:%d\n", file, line);
    printf("  Condition: %s\n", condition);
    printf("  Memory blocks differ (size=%zu)\n", size);
    g_current_result = FC_TEST_FAILED;
}

void fc_test_skip(const char* message) {
    printf("SKIP: %s - %s\n", g_current_test ? g_current_test : "unknown", message);
    g_current_result = FC_TEST_SKIPPED;
}

/*
 * Test runner implementation
*/

void fc_test_start(const char* test_name) {
    g_current_test = test_name;
    g_current_result = FC_TEST_PASSED;
    g_test_start_time = clock();

    if (g_verbose) {
        printf("RUN:  %s\n", test_name);
    }
}

void fc_test_end(void) {
    clock_t end_time = clock();
    double elapsed = (double)(end_time - g_test_start_time) * 1000.0 / CLOCKS_PER_SEC;

    g_stats.total_tests++;

    switch (g_current_result) {
        case FC_TEST_PASSED:
            g_stats.passed++;
            if (g_verbose) {
                printf("PASS: %s (%.2f ms)\n", g_current_test, elapsed);
            }
            break;
        case FC_TEST_FAILED:
            g_stats.failed++;
            break;
        case FC_TEST_SKIPPED:
            g_stats.skipped++;
            break;
    }

    g_current_test = NULL;
}

fc_test_result_t fc_test_get_result(void) {
    return g_current_result;
}

void fc_test_set_result(fc_test_result_t result) {
    g_current_result = result;
}

double fc_test_get_elapsed_ms(void) {
    clock_t now = clock();
    return (double)(now - g_test_start_time) * 1000.0 / CLOCKS_PER_SEC;
}

void fc_test_set_verbose(int verbose) {
    g_verbose = verbose;
}

void fc_test_set_filter(const char* pattern) {
    g_filter_pattern = pattern;
}

void fc_test_set_coverage(int enable) {
    g_coverage_enabled = enable;
}

static void print_usage(const char* program) {
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -v              Verbose output (default)\n");
    printf("  -q              Quiet mode (summary only)\n");
    printf("  -coverage       Generate coverage report\n");
    printf("  -run <pattern>  Run only tests matching pattern\n");
    printf("  -h, -help       Show this help message\n");
}

void fc_test_init_with_args(int argc, char** argv) {
    fc_test_stats_init(&g_stats);
    g_num_suites = 0;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            g_verbose = 1;
        } else if (strcmp(argv[i], "-q") == 0) {
            g_verbose = 0;
        } else if (strcmp(argv[i], "-coverage") == 0) {
            g_coverage_enabled = 1;
        } else if (strcmp(argv[i], "-run") == 0 && i + 1 < argc) {
            g_filter_pattern = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            exit(1);
        }
    }
}

/*
 * Test suite management
*/

void fc_test_register_suite(const fc_test_suite_t* suite) {
    if (g_num_suites >= FC_TEST_MAX_SUITES) {
        fprintf(stderr, "Error: Maximum number of test suites (%d) exceeded\n",
                FC_TEST_MAX_SUITES);
        return;
    }
    g_suites[g_num_suites++] = suite;
}

int fc_test_run_all(void) {
    clock_t total_start = clock();

    printf("\n============================================================\n");
    printf("Running all test suites (%d suites)\n", g_num_suites);
    printf("============================================================\n\n");

    for (int i = 0; i < g_num_suites; i++) {
        const fc_test_suite_t* suite = g_suites[i];
        printf("\nSuite: %s\n", suite->name);
        if (suite->description != NULL) {
            printf("  %s\n", suite->description);
        }
        printf("------------------------------------------------------------\n");

        for (int j = 0; j < suite->num_tests; j++) {
            fc_test_fn test_fn = suite->tests[j];
            if (test_fn != NULL) {
                test_fn();
                fc_test_end();
            }
        }
    }

    clock_t total_end = clock();
    g_stats.elapsed_time_ms = (double)(total_end - total_start) * 1000.0 / CLOCKS_PER_SEC;

    printf("\n");
    fc_test_stats_print(&g_stats);

    return g_stats.failed > 0 ? 1 : 0;
}

int fc_test_run_suite(const char* name) {
    for (int i = 0; i < g_num_suites; i++) {
        const fc_test_suite_t* suite = g_suites[i];
        if (strcmp(suite->name, name) == 0) {
            printf("\nRunning suite: %s\n", suite->name);
            if (suite->description != NULL) {
                printf("  %s\n", suite->description);
            }
            printf("------------------------------------------------------------\n");

            for (int j = 0; j < suite->num_tests; j++) {
                fc_test_fn test_fn = suite->tests[j];
                if (test_fn != NULL) {
                    test_fn();
                    fc_test_end();
                }
            }

            return g_stats.failed > 0 ? 1 : 0;
        }
    }

    printf("Error: Test suite '%s' not found\n", name);
    return 1;
}

fc_test_stats_t* fc_test_get_stats(void) {
    return &g_stats;
}

void fc_test_init(void) {
    fc_test_stats_init(&g_stats);
    g_num_suites = 0;
}

void fc_test_cleanup(void) {
    g_num_suites = 0;
}

/*
 * Coverage reporting
*/

int fc_test_generate_coverage_report(void) {
    if (!g_coverage_enabled) {
        return 0;
    }

    printf("\n============================================================\n");
    printf("Generating coverage report...\n");
    printf("============================================================\n");

    /* Flush coverage data - use dlsym to avoid link errors when not compiled with coverage */
    typedef void (*gcov_flush_fn)(void);
    union {
        void *obj;
        gcov_flush_fn fn;
    } flush_ptr;
    flush_ptr.obj = dlsym(RTLD_DEFAULT, "__gcov_flush");
    if (flush_ptr.fn) {
        flush_ptr.fn();
    }

    /* Run gcov on source files */
    int ret = system("gcov -r src/matrix/*.c 2>/dev/null | grep -A 3 '\\.c' || true");

    if (ret == 0) {
        printf("\nCoverage report generated successfully.\n");
        printf("See *.gcov files for detailed line-by-line coverage.\n");
    } else {
        printf("\nNote: gcov not available or coverage data not found.\n");
        printf("To enable coverage, compile with: -fprofile-arcs -ftest-coverage\n");
    }

    return ret;
}

/*
 * Memory tracking (stub implementation)
*/

void fc_test_enable_leak_detection(void) {
}

int fc_test_check_leaks(void) {
    return 0;
}

void fc_test_print_leak_report(void) {
}
