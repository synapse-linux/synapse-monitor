// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <getopt.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

static bool sort_valid(mon_view view, mon_sort sort) {
    switch (view) {
        case MON_VIEW_PROCESSES:
            return sort == MON_SORT_CPU || sort == MON_SORT_MEMORY
                || sort == MON_SORT_READ || sort == MON_SORT_WRITE
                || sort == MON_SORT_NAME || sort == MON_SORT_PID
                || sort == MON_SORT_USER || sort == MON_SORT_STATE
                || sort == MON_SORT_THREADS;
        case MON_VIEW_SERVICES:
            return sort == MON_SORT_NAME || sort == MON_SORT_STATUS
                || sort == MON_SORT_STARTUP || sort == MON_SORT_PID
                || sort == MON_SORT_DESCRIPTION || sort == MON_SORT_USER
                || sort == MON_SORT_EXECUTABLE;
        case MON_VIEW_STARTUP:
            return sort == MON_SORT_NAME || sort == MON_SORT_STATUS
                || sort == MON_SORT_SCOPE || sort == MON_SORT_TYPE
                || sort == MON_SORT_PUBLISHER || sort == MON_SORT_COMMAND;
        case MON_VIEW_CONNECTIONS:
            return sort == MON_SORT_PROTOCOL || sort == MON_SORT_LOCAL
                || sort == MON_SORT_REMOTE || sort == MON_SORT_STATUS
                || sort == MON_SORT_PID || sort == MON_SORT_PROCESS;
        case MON_VIEW_PERFORMANCE:
        case MON_VIEW_INFORMATION:
        case MON_VIEW_COUNT:
        default: return sort == MON_SORT_NAME;
    }
}

static int run_describe(int argc, char **argv) {
    if (argc == 2 || (argc == 3 && strcmp(argv[2], "--format=json") == 0)
        || (argc == 4 && strcmp(argv[2], "--format") == 0
            && strcmp(argv[3], "json") == 0))
        return mon_render_presentation();
    if (argc == 3 && strcmp(argv[2], "--help") == 0) {
        mon_usage(stdout);
        return 0;
    }
    if (argc == 3 && strcmp(argv[2], "--version") == 0) {
        puts("synapse-monitor " MON_VERSION);
        return 0;
    }
    fputs("synapse-monitor: invalid arguments\n", stderr);
    mon_usage(stderr);
    return 2;
}

static int run_inspect(int argc, char **argv) {
    mon_format format = MON_FORMAT_TEXT;
    uint64_t pid = 0U;
    bool pid_set = false;
    enum { INSPECT_FORMAT = 2000, INSPECT_PID, INSPECT_HELP, INSPECT_VERSION };
    static const struct option inspect_options[] = {
        {"format", required_argument, NULL, INSPECT_FORMAT},
        {"pid", required_argument, NULL, INSPECT_PID},
        {"help", no_argument, NULL, INSPECT_HELP},
        {"version", no_argument, NULL, INSPECT_VERSION},
        {NULL, 0, NULL, 0}
    };
    opterr = 0;
    optind = 2;
    int option = 0;
    while ((option = getopt_long(argc, argv, "", inspect_options, NULL)) != -1) {
        switch (option) {
            case INSPECT_FORMAT:
                if (mon_parse_format(optarg, &format) != 0) goto invalid;
                break;
            case INSPECT_PID:
                if (mon_parse_u64(optarg, 1U, 4194304U, &pid) != 0) goto invalid;
                pid_set = true;
                break;
            case INSPECT_HELP:
                mon_usage(stdout);
                return 0;
            case INSPECT_VERSION:
                puts("synapse-monitor " MON_VERSION);
                return 0;
            default: goto invalid;
        }
    }
    if (optind != argc || !pid_set || format == MON_FORMAT_NDJSON) goto invalid;
    mon_roots roots;
    char error[MON_ERROR_MAX] = {0};
    if (mon_roots_from_environment(&roots, error, sizeof(error)) != 0) {
        fprintf(stderr, "synapse-monitor: %s\n",
                error[0] ? error : "invalid roots");
        return 1;
    }
    mon_process_inspection inspection;
    if (mon_collect_process_inspection(&roots, (int)pid, &inspection,
                                       error, sizeof(error)) != 0) {
        fprintf(stderr, "synapse-monitor: %s\n",
                error[0] ? error : "process inspection failed");
        return 1;
    }
    return mon_render_process_inspection(&inspection, format);
invalid:
    fputs("synapse-monitor: invalid arguments\n", stderr);
    mon_usage(stderr);
    return 2;
}

static int render_snapshot(const mon_roots *roots, mon_options *options,
                           char *error, size_t error_size) {
    if (options->view == MON_VIEW_PROCESSES
        || options->view == MON_VIEW_PERFORMANCE) {
        mon_report report;
        if (mon_collect_report(roots, options->sample_milliseconds, &report,
                               error, error_size) != 0) return 1;
        if (options->view == MON_VIEW_PROCESSES)
            mon_prepare_report(&report, options);
        int result = mon_render_report(&report, options);
        mon_report_free(&report);
        return result;
    }
    if (options->view == MON_VIEW_SERVICES) {
        mon_service_snapshot snapshot;
        if (mon_collect_services(roots, &snapshot, error, error_size) != 0) return 1;
        mon_prepare_services(&snapshot, options);
        int result = mon_render_services(&snapshot, options);
        mon_service_snapshot_free(&snapshot);
        return result;
    }
    if (options->view == MON_VIEW_STARTUP) {
        mon_startup_snapshot snapshot;
        if (mon_collect_startup(roots, &snapshot, error, error_size) != 0) return 1;
        mon_prepare_startup(&snapshot, options);
        int result = mon_render_startup(&snapshot, options);
        mon_startup_snapshot_free(&snapshot);
        return result;
    }
    if (options->view == MON_VIEW_CONNECTIONS) {
        mon_connection_snapshot snapshot;
        if (mon_collect_connections(roots, &snapshot, error, error_size) != 0)
            return 1;
        mon_prepare_connections(&snapshot, options);
        int result = mon_render_connections(&snapshot, options);
        mon_connection_snapshot_free(&snapshot);
        return result;
    }
    if (options->view == MON_VIEW_INFORMATION) {
        mon_information information;
        if (mon_collect_information(roots, &information, error, error_size) != 0)
            return 1;
        return mon_render_information(&information, options);
    }
    return 1;
}

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
    if (argc >= 2 && strcmp(argv[1], "describe") == 0)
        return run_describe(argc, argv);
    if (argc >= 2 && strcmp(argv[1], "inspect") == 0)
        return run_inspect(argc, argv);
    if (argc < 2 || (strcmp(argv[1], "snapshot") != 0
                     && strcmp(argv[1], "stream") != 0
                     && strcmp(argv[1], "watch") != 0)) {
        fputs("synapse-monitor: expected snapshot, stream, watch, describe or inspect\n",
              stderr);
        mon_usage(stderr);
        return 2;
    }
    bool watch = strcmp(argv[1], "watch") == 0;
    bool stream = strcmp(argv[1], "stream") == 0;
    mon_options options = {
        .format = stream ? MON_FORMAT_NDJSON : MON_FORMAT_TEXT,
        .view = MON_VIEW_PROCESSES,
        .sort = MON_SORT_CPU,
        .group = MON_GROUP_CLASS,
        .theme = MON_THEME_DEFAULT,
        .layout = MON_LAYOUT_BALANCED,
        .columns = MON_PROCESS_COLUMNS_DEFAULT,
        .filter = "",
        .limit = 40U,
        .sample_milliseconds = 250U,
        .interval_milliseconds = 1000U,
        .iterations = 0U,
        .interactive_output = false,
        .stream_output = false,
        .stream_sequence = 0U,
        .history = NULL
    };
    enum {
        OPT_VIEW = 1000,
        OPT_FORMAT,
        OPT_SORT,
        OPT_GROUP,
        OPT_COLUMNS,
        OPT_THEME,
        OPT_LAYOUT,
        OPT_FILTER,
        OPT_LIMIT,
        OPT_SAMPLE,
        OPT_INTERVAL,
        OPT_ITERATIONS,
        OPT_HELP,
        OPT_VERSION
    };
    static const struct option long_options[] = {
        {"view", required_argument, NULL, OPT_VIEW},
        {"format", required_argument, NULL, OPT_FORMAT},
        {"sort", required_argument, NULL, OPT_SORT},
        {"group", required_argument, NULL, OPT_GROUP},
        {"columns", required_argument, NULL, OPT_COLUMNS},
        {"theme", required_argument, NULL, OPT_THEME},
        {"layout", required_argument, NULL, OPT_LAYOUT},
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
    bool sample_set = false;
    bool sort_set = false;
    bool group_set = false;
    bool columns_set = false;
    const char *columns_value = NULL;
    opterr = 0;
    optind = 2;
    int option = 0;
    while ((option = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        uint64_t parsed = 0U;
        switch (option) {
            case OPT_VIEW:
                if (mon_parse_view(optarg, &options.view) != 0) goto invalid;
                break;
            case OPT_FORMAT:
                if (mon_parse_format(optarg, &options.format) != 0) goto invalid;
                break;
            case OPT_SORT:
                if (mon_parse_sort(optarg, &options.sort) != 0) goto invalid;
                sort_set = true;
                break;
            case OPT_GROUP:
                if (mon_parse_group(optarg, &options.group) != 0) goto invalid;
                group_set = true;
                break;
            case OPT_COLUMNS:
                columns_value = optarg;
                columns_set = true;
                break;
            case OPT_THEME:
                if (mon_parse_theme(optarg, &options.theme) != 0) goto invalid;
                break;
            case OPT_LAYOUT:
                if (mon_parse_layout(optarg, &options.layout) != 0) goto invalid;
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
                sample_set = true;
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
    if (optind != argc
        || (!watch && !stream && (interval_set || iterations_set))
        || (watch && options.format != MON_FORMAT_TEXT)
        || (stream && options.format != MON_FORMAT_NDJSON)
        || (!watch && !stream && options.format == MON_FORMAT_NDJSON)) goto invalid;
    if (!sort_set) options.sort = mon_default_sort(options.view);
    if (!sort_valid(options.view, options.sort)) goto invalid;
    if (group_set && options.view != MON_VIEW_PROCESSES) goto invalid;
    if (columns_set) {
        if (mon_parse_columns_for_view(options.view, columns_value,
                                       &options.columns) != 0) goto invalid;
    } else options.columns = mon_default_columns(options.view);
    if (!watch && sample_set && options.view != MON_VIEW_PROCESSES
        && options.view != MON_VIEW_PERFORMANCE) goto invalid;
    if ((options.view == MON_VIEW_INFORMATION
         || options.view == MON_VIEW_PERFORMANCE)
        && options.filter[0] != '\0') goto invalid;

    mon_roots roots;
    char error[MON_ERROR_MAX] = {0};
    if (mon_roots_from_environment(&roots, error, sizeof(error)) != 0) {
        fprintf(stderr, "synapse-monitor: %s\n",
                error[0] ? error : "invalid roots");
        return 1;
    }
    if (watch) return mon_run_watch(&roots, &options);
    if (stream) return mon_run_stream(&roots, &options);
    int result = render_snapshot(&roots, &options, error, sizeof(error));
    if (result != 0 && error[0]) fprintf(stderr, "synapse-monitor: %s\n", error);
    return result;

invalid:
    fputs("synapse-monitor: invalid arguments\n", stderr);
    mon_usage(stderr);
    return 2;
}
