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

static void cycle_sort(mon_options *options) {
    options->sort = (mon_sort)(((unsigned)options->sort + 1U) % 6U);
}

static void cycle_group(mon_options *options) {
    options->group = (mon_group)(((unsigned)options->group + 1U) % 3U);
}

static void cycle_columns(mon_options *options) {
    const uint32_t compact = MON_COLUMN_NAME | MON_COLUMN_PID | MON_COLUMN_CPU
        | MON_COLUMN_MEMORY;
    if (options->columns == compact) options->columns = MON_COLUMNS_ALL;
    else if (options->columns == MON_COLUMNS_ALL) options->columns = MON_COLUMNS_DEFAULT;
    else options->columns = compact;
}

static int read_filter(mon_options *options) {
    char original[MON_FILTER_MAX + 1U];
    memcpy(original, options->filter, sizeof(original));
    size_t length = strlen(options->filter);
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
            } else if (value >= 0x20U && value <= 0x7eU && length < MON_FILTER_MAX) {
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
        switch (bytes[i]) {
            case 'q':
            case 'Q': stop_requested = 1; break;
            case 's':
            case 'S': cycle_sort(options); break;
            case 'g':
            case 'G': cycle_group(options); break;
            case 'c':
            case 'C': cycle_columns(options); break;
            case '/':
                if (read_filter(options) != 0) return -1;
                break;
            default: break;
        }
    }
    return 0;
}

static size_t terminal_row_limit(size_t configured) {
    struct winsize size;
    memset(&size, 0, sizeof(size));
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_row <= 14U)
        return configured;
    size_t available = (size_t)size.ws_row - 12U;
    return available < configured ? available : configured;
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
    uint64_t completed = 0U;
    int exit_status = 0;
    while (!stop_requested && (iterations == 0U || completed < iterations)) {
        mon_report report;
        char error[MON_ERROR_MAX] = {0};
        if (mon_collect_report(roots, options->sample_milliseconds, &report,
                               error, sizeof(error)) != 0) {
            fprintf(stderr, "synapse-monitor: %s\n", error[0] ? error : "probe failed");
            exit_status = 1;
            break;
        }
        mon_options display = *options;
        if (interactive) display.limit = terminal_row_limit(options->limit);
        mon_prepare_report(&report, &display);
        if (interactive) {
            fputs("\033[H\033[2J", stdout);
        } else if (completed > 0U) fputc('\n', stdout);
        if (mon_render_report(&report, &display) != 0 || fflush(stdout) != 0) {
            mon_report_free(&report);
            exit_status = 1;
            break;
        }
        mon_report_free(&report);
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
