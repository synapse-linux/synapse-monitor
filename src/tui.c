// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;
static struct termios saved_terminal;
static bool terminal_changed = false;

static void restore_terminal(void) {
    if (terminal_changed) {
        (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_terminal);
        terminal_changed = false;
    }
}

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static int enable_raw_terminal(void) {
    if (tcgetattr(STDIN_FILENO, &saved_terminal) != 0) return -1;
    struct termios raw = saved_terminal;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    terminal_changed = true;
    return 0;
}

static int wait_without_input(uint64_t milliseconds) {
    int timeout = milliseconds > (uint64_t)INT_MAX ? INT_MAX : (int)milliseconds;
    int result;
    do result = poll(NULL, 0U, timeout); while (result < 0 && errno == EINTR
                                               && !stop_requested);
    return result < 0 && errno != EINTR ? -1 : 0;
}

static void select_view(mon_options *options, mon_view view) {
    if (!options || view >= MON_VIEW_COUNT) return;
    options->view = view;
    options->sort = mon_default_sort(view);
    options->columns = mon_default_columns(view);
    options->filter[0] = '\0';
    if (view != MON_VIEW_PROCESSES) options->group = MON_GROUP_NONE;
    else options->group = MON_GROUP_CLASS;
}

static void cycle_sort(mon_options *options) {
    static const mon_sort process[] = {
        MON_SORT_CPU, MON_SORT_MEMORY, MON_SORT_READ, MON_SORT_WRITE,
        MON_SORT_NAME, MON_SORT_PID, MON_SORT_USER, MON_SORT_STATE,
        MON_SORT_THREADS
    };
    static const mon_sort services[] = {
        MON_SORT_NAME, MON_SORT_DESCRIPTION, MON_SORT_STATUS, MON_SORT_STARTUP,
        MON_SORT_PID, MON_SORT_USER, MON_SORT_EXECUTABLE
    };
    static const mon_sort startup[] = {
        MON_SORT_NAME, MON_SORT_PUBLISHER, MON_SORT_STATUS, MON_SORT_TYPE,
        MON_SORT_SCOPE, MON_SORT_COMMAND
    };
    static const mon_sort connections[] = {
        MON_SORT_PROTOCOL, MON_SORT_LOCAL, MON_SORT_REMOTE, MON_SORT_STATUS,
        MON_SORT_PID, MON_SORT_PROCESS
    };
    const mon_sort *values = process;
    size_t count = sizeof(process) / sizeof(process[0]);
    if (options->view == MON_VIEW_SERVICES) {
        values = services;
        count = sizeof(services) / sizeof(services[0]);
    } else if (options->view == MON_VIEW_STARTUP) {
        values = startup;
        count = sizeof(startup) / sizeof(startup[0]);
    } else if (options->view == MON_VIEW_CONNECTIONS) {
        values = connections;
        count = sizeof(connections) / sizeof(connections[0]);
    } else if (options->view == MON_VIEW_PERFORMANCE
               || options->view == MON_VIEW_INFORMATION) return;
    size_t index = 0U;
    while (index < count && values[index] != options->sort) index++;
    options->sort = values[(index + 1U) % count];
}

static void cycle_group(mon_options *options) {
    if (options->view == MON_VIEW_PROCESSES)
        options->group = (mon_group)(((unsigned)options->group + 1U) % 3U);
}

static void cycle_columns(mon_options *options) {
    uint32_t compact = 0U;
    uint32_t complete;
    if (options->view == MON_VIEW_PROCESSES) {
        compact = MON_COLUMN_NAME | MON_COLUMN_PID | MON_COLUMN_CPU
            | MON_COLUMN_MEMORY;
        complete = MON_PROCESS_COLUMNS_ALL;
    } else if (options->view == MON_VIEW_SERVICES) {
        compact = MON_SERVICE_COLUMN_NAME | MON_SERVICE_COLUMN_STATUS
            | MON_SERVICE_COLUMN_STARTUP | MON_SERVICE_COLUMN_PID;
        complete = MON_SERVICE_COLUMNS_ALL;
    } else if (options->view == MON_VIEW_STARTUP) {
        compact = MON_STARTUP_COLUMN_NAME | MON_STARTUP_COLUMN_STATUS
            | MON_STARTUP_COLUMN_SCOPE | MON_STARTUP_COLUMN_LAUNCH;
        complete = MON_STARTUP_COLUMNS_ALL;
    } else if (options->view == MON_VIEW_CONNECTIONS) {
        compact = MON_CONNECTION_COLUMN_PROTOCOL | MON_CONNECTION_COLUMN_LOCAL
            | MON_CONNECTION_COLUMN_STATE | MON_CONNECTION_COLUMN_PROCESS;
        complete = MON_CONNECTION_COLUMNS_ALL;
    } else return;
    options->columns = options->columns == compact ? complete : compact;
}

static void cycle_layout(mon_options *options) {
    options->layout = (mon_layout)(((unsigned)options->layout + 1U) % 3U);
}

static void cycle_theme(mon_options *options) {
    options->theme = (mon_theme)(((unsigned)options->theme + 1U) % 3U);
}

static int read_filter(mon_options *options, int initial) {
    if (options->view == MON_VIEW_INFORMATION
        || options->view == MON_VIEW_PERFORMANCE) return 0;
    char original[MON_FILTER_MAX + 1U];
    memcpy(original, options->filter, sizeof(original));
    size_t length = strlen(options->filter);
    if (initial >= 0 && initial >= 0x20 && initial <= 0x7e
        && length < MON_FILTER_MAX) {
        options->filter[length++] = (char)initial;
        options->filter[length] = '\0';
    }
    printf("\r\033[KFilter: %s", options->filter);
    fflush(stdout);
    while (!stop_requested) {
        struct pollfd descriptor = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
        int result;
        do result = poll(&descriptor, 1U, -1); while (result < 0 && errno == EINTR
                                                     && !stop_requested);
        if (result < 0 && errno != EINTR) return -1;
        if (result <= 0) continue;
        if (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL)) return -1;
        if (!(descriptor.revents & POLLIN)) continue;
        unsigned char bytes[32];
        ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));
        if (count <= 0) continue;
        for (ssize_t i = 0; i < count; i++) {
            unsigned char value = bytes[i];
            if (value == '\r' || value == '\n') {
                fputc('\n', stdout);
                return 0;
            }
            if (value == 0x1bU) {
                memcpy(options->filter, original, sizeof(original));
                fputc('\n', stdout);
                return 0;
            }
            if (value == 0x7fU || value == 0x08U) {
                if (length > 0U) options->filter[--length] = '\0';
            } else if (value >= 0x20U && value <= 0x7eU
                       && length < MON_FILTER_MAX) {
                options->filter[length++] = (char)value;
                options->filter[length] = '\0';
            }
        }
        printf("\r\033[KFilter: %s", options->filter);
        fflush(stdout);
    }
    return 0;
}

static int wait_for_key(uint64_t milliseconds, mon_options *options) {
    int timeout = milliseconds > (uint64_t)INT_MAX ? INT_MAX : (int)milliseconds;
    struct pollfd descriptor = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
    int result;
    do result = poll(&descriptor, 1U, timeout); while (result < 0 && errno == EINTR
                                                      && !stop_requested);
    if (result < 0 && errno != EINTR) return -1;
    if (result <= 0) return 0;
    if (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        stop_requested = 1;
        return 0;
    }
    if (!(descriptor.revents & POLLIN)) return 0;
    unsigned char bytes[32];
    ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));
    if (count <= 0) return 0;
    for (ssize_t i = 0; i < count; i++) {
        unsigned char value = bytes[i];
        switch (value) {
            case 'Q': stop_requested = 1; break;
            case 'S': cycle_sort(options); break;
            case 'G': cycle_group(options); break;
            case 'C': cycle_columns(options); break;
            case 'L': cycle_layout(options); break;
            case 'T': cycle_theme(options); break;
            case '1': select_view(options, MON_VIEW_PROCESSES); break;
            case '2': select_view(options, MON_VIEW_PERFORMANCE); break;
            case '3': select_view(options, MON_VIEW_SERVICES); break;
            case '4': select_view(options, MON_VIEW_STARTUP); break;
            case '5': select_view(options, MON_VIEW_CONNECTIONS); break;
            case '6': select_view(options, MON_VIEW_INFORMATION); break;
            case '\t':
                select_view(options,
                            (mon_view)(((unsigned)options->view + 1U)
                                       % (unsigned)MON_VIEW_COUNT));
                break;
            case '/':
                if (read_filter(options, -1) != 0) return -1;
                break;
            case 0x1bU:
                i = count;
                break;
            default:
                if (value >= 0x20U && value <= 0x7eU
                    && read_filter(options, value) != 0) return -1;
                break;
        }
    }
    return 0;
}

static size_t terminal_row_limit(size_t configured, mon_layout layout) {
    struct winsize size;
    memset(&size, 0, sizeof(size));
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_row <= 16U)
        return configured;
    size_t reserved = layout == MON_LAYOUT_DENSE ? 10U
        : (layout == MON_LAYOUT_WIDE ? 16U : 13U);
    size_t available = (size_t)size.ws_row > reserved
        ? (size_t)size.ws_row - reserved : 1U;
    return available < configured ? available : configured;
}

static int collect_and_render(const mon_roots *roots, mon_options *options,
                              mon_history *history, char *error,
                              size_t error_size) {
    if (options->view == MON_VIEW_PROCESSES
        || options->view == MON_VIEW_PERFORMANCE) {
        mon_report report;
        if (mon_collect_report(roots, options->sample_milliseconds, &report,
                               error, error_size) != 0) return -1;
        mon_history_update(history, &report);
        mon_options display = *options;
        display.history = history;
        if (options->view == MON_VIEW_PROCESSES)
            mon_prepare_report(&report, &display);
        int result = mon_render_report(&report, &display);
        mon_report_free(&report);
        return result == 0 ? 0 : -1;
    }
    if (options->view == MON_VIEW_SERVICES) {
        mon_service_snapshot snapshot;
        if (mon_collect_services(roots, &snapshot, error, error_size) != 0) return -1;
        mon_prepare_services(&snapshot, options);
        int result = mon_render_services(&snapshot, options);
        mon_service_snapshot_free(&snapshot);
        return result == 0 ? 0 : -1;
    }
    if (options->view == MON_VIEW_STARTUP) {
        mon_startup_snapshot snapshot;
        if (mon_collect_startup(roots, &snapshot, error, error_size) != 0) return -1;
        mon_prepare_startup(&snapshot, options);
        int result = mon_render_startup(&snapshot, options);
        mon_startup_snapshot_free(&snapshot);
        return result == 0 ? 0 : -1;
    }
    if (options->view == MON_VIEW_CONNECTIONS) {
        mon_connection_snapshot snapshot;
        if (mon_collect_connections(roots, &snapshot, error, error_size) != 0)
            return -1;
        mon_prepare_connections(&snapshot, options);
        int result = mon_render_connections(&snapshot, options);
        mon_connection_snapshot_free(&snapshot);
        return result == 0 ? 0 : -1;
    }
    if (options->view == MON_VIEW_INFORMATION) {
        mon_information information;
        if (mon_collect_information(roots, &information, error, error_size) != 0)
            return -1;
        return mon_render_information(&information, options) == 0 ? 0 : -1;
    }
    return -1;
}

int mon_run_watch(const mon_roots *roots, mon_options *options) {
    if (!roots || !options || options->format != MON_FORMAT_TEXT) return 2;
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    uint64_t iterations = options->iterations;
    if (!interactive && iterations == 0U) iterations = 1U;
    if (interactive) {
        if (enable_raw_terminal() != 0) {
            fputs("synapse-monitor: cannot configure interactive terminal\n", stderr);
            return 1;
        }
        if (atexit(restore_terminal) != 0) {
            restore_terminal();
            fputs("synapse-monitor: cannot register terminal restoration\n", stderr);
            return 1;
        }
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = request_stop;
        sigemptyset(&action.sa_mask);
        (void)sigaction(SIGHUP, &action, NULL);
        (void)sigaction(SIGINT, &action, NULL);
        (void)sigaction(SIGQUIT, &action, NULL);
        (void)sigaction(SIGTERM, &action, NULL);
    }
    mon_history history;
    memset(&history, 0, sizeof(history));
    uint64_t completed = 0U;
    int exit_status = 0;
    while (!stop_requested && (iterations == 0U || completed < iterations)) {
        mon_options display = *options;
        display.interactive_output = interactive;
        if (interactive)
            display.limit = terminal_row_limit(options->limit, options->layout);
        if (interactive) fputs("\033[H\033[2J", stdout);
        else if (completed > 0U) fputc('\n', stdout);
        char error[MON_ERROR_MAX] = {0};
        if (collect_and_render(roots, &display, &history, error, sizeof(error)) != 0) {
            fprintf(stderr, "synapse-monitor: %s\n",
                    error[0] ? error : "view probe failed");
            exit_status = 1;
            break;
        }
        if (interactive) {
            printf("\nKeys: 1-6 views  Tab next  type or / filter  S sort  G group  "
                   "C columns  L layout  T theme  Q quit\n"
                   "View=%s  Layout=%s  Theme=%s\n",
                   mon_view_id(options->view), mon_layout_id(options->layout),
                   mon_theme_id(options->theme));
        }
        if (fflush(stdout) != 0) {
            exit_status = 1;
            break;
        }
        completed++;
        if (iterations != 0U && completed >= iterations) break;
        int waited = interactive
            ? wait_for_key(options->interval_milliseconds, options)
            : wait_without_input(options->interval_milliseconds);
        if (waited != 0) {
            fputs("synapse-monitor: refresh wait failed\n", stderr);
            exit_status = 1;
            break;
        }
    }
    restore_terminal();
    return exit_status;
}
