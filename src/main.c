// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <getopt.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (!setlocale(LC_ALL, "C")) {
        fputs("synapse-monitor: cannot select deterministic C locale\n", stderr);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("synapse-monitor " MON_VERSION);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        mon_usage(stdout);
        return 0;
    }
    if (argc < 2 || (strcmp(argv[1], "snapshot") != 0
                     && strcmp(argv[1], "watch") != 0)) {
        fputs("synapse-monitor: expected snapshot or watch\n", stderr);
        mon_usage(stderr);
        return 2;
    }
    bool watch = strcmp(argv[1], "watch") == 0;
    mon_options options = {
        .format = MON_FORMAT_TEXT,
        .sort = MON_SORT_CPU,
        .group = MON_GROUP_CLASS,
        .columns = MON_COLUMNS_DEFAULT,
        .filter = "",
        .limit = 40U,
        .sample_milliseconds = 250U,
        .interval_milliseconds = 1000U,
        .iterations = 0U
    };
    enum {
        OPT_FORMAT = 1000,
        OPT_SORT,
        OPT_GROUP,
        OPT_COLUMNS,
        OPT_FILTER,
        OPT_LIMIT,
        OPT_SAMPLE,
        OPT_INTERVAL,
        OPT_ITERATIONS,
        OPT_HELP,
        OPT_VERSION
    };
    static const struct option long_options[] = {
        {"format", required_argument, NULL, OPT_FORMAT},
        {"sort", required_argument, NULL, OPT_SORT},
        {"group", required_argument, NULL, OPT_GROUP},
        {"columns", required_argument, NULL, OPT_COLUMNS},
        {"filter", required_argument, NULL, OPT_FILTER},
        {"limit", required_argument, NULL, OPT_LIMIT},
        {"sample-ms", required_argument, NULL, OPT_SAMPLE},
        {"interval-ms", required_argument, NULL, OPT_INTERVAL},
        {"iterations", required_argument, NULL, OPT_ITERATIONS},
        {"help", no_argument, NULL, OPT_HELP},
        {"version", no_argument, NULL, OPT_VERSION},
        {NULL, 0, NULL, 0}
    };
    bool interval_set = false;
    bool iterations_set = false;
    opterr = 0;
    optind = 2;
    int option = 0;
    while ((option = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        uint64_t parsed = 0U;
        switch (option) {
            case OPT_FORMAT:
                if (mon_parse_format(optarg, &options.format) != 0) goto invalid;
                break;
            case OPT_SORT:
                if (mon_parse_sort(optarg, &options.sort) != 0) goto invalid;
                break;
            case OPT_GROUP:
                if (mon_parse_group(optarg, &options.group) != 0) goto invalid;
                break;
            case OPT_COLUMNS:
                if (mon_parse_columns(optarg, &options.columns) != 0) goto invalid;
                break;
            case OPT_FILTER:
                if (mon_parse_filter(optarg, options.filter) != 0) goto invalid;
                break;
            case OPT_LIMIT:
                if (mon_parse_u64(optarg, 1U, MON_MAX_OUTPUT_ROWS, &parsed) != 0)
                    goto invalid;
                options.limit = (size_t)parsed;
                break;
            case OPT_SAMPLE:
                if (mon_parse_u64(optarg, MON_SAMPLE_MIN_MS, MON_SAMPLE_MAX_MS,
                                  &options.sample_milliseconds) != 0) goto invalid;
                break;
            case OPT_INTERVAL:
                if (mon_parse_u64(optarg, MON_INTERVAL_MIN_MS, MON_INTERVAL_MAX_MS,
                                  &options.interval_milliseconds) != 0) goto invalid;
                interval_set = true;
                break;
            case OPT_ITERATIONS:
                if (mon_parse_u64(optarg, 1U, 1000000U, &options.iterations) != 0)
                    goto invalid;
                iterations_set = true;
                break;
            case OPT_HELP:
                mon_usage(stdout);
                return 0;
            case OPT_VERSION:
                puts("synapse-monitor " MON_VERSION);
                return 0;
            default: goto invalid;
        }
    }
    if (optind != argc || (!watch && (interval_set || iterations_set))
        || (watch && options.format != MON_FORMAT_TEXT)) goto invalid;

    mon_roots roots;
    char error[MON_ERROR_MAX] = {0};
    if (mon_roots_from_environment(&roots, error, sizeof(error)) != 0) {
        fprintf(stderr, "synapse-monitor: %s\n", error[0] ? error : "invalid roots");
        return 1;
    }
    if (watch) return mon_run_watch(&roots, &options);

    mon_report report;
    if (mon_collect_report(&roots, options.sample_milliseconds, &report,
                           error, sizeof(error)) != 0) {
        fprintf(stderr, "synapse-monitor: %s\n", error[0] ? error : "probe failed");
        return 1;
    }
    mon_prepare_report(&report, &options);
    int result = mon_render_report(&report, &options);
    mon_report_free(&report);
    return result;

invalid:
    fputs("synapse-monitor: invalid arguments\n", stderr);
    mon_usage(stderr);
    return 2;
}
