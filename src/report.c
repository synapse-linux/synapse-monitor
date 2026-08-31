// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

__extension__ typedef unsigned __int128 mon_u128;

static uint64_t difference(uint64_t current, uint64_t previous) {
    return current >= previous ? current - previous : 0U;
}

static uint64_t multiply_divide(uint64_t value, uint64_t multiplier,
                                uint64_t divisor) {
    if (divisor == 0U) return 0U;
    mon_u128 result = (mon_u128)value * (mon_u128)multiplier / (mon_u128)divisor;
    return result > UINT64_MAX ? UINT64_MAX : (uint64_t)result;
}

static uint64_t rate_per_second(uint64_t current, uint64_t previous,
                                uint64_t elapsed_nanoseconds) {
    return multiply_divide(difference(current, previous), 1000000000U,
                           elapsed_nanoseconds);
}

static uint64_t elapsed_nanoseconds(const struct timespec *current,
                                    const struct timespec *previous) {
    if (!current || !previous) return 0U;
    if (current->tv_sec < previous->tv_sec
        || (current->tv_sec == previous->tv_sec
            && current->tv_nsec <= previous->tv_nsec)) return 0U;
    uint64_t seconds = (uint64_t)(current->tv_sec - previous->tv_sec);
    int64_t nanoseconds = current->tv_nsec - previous->tv_nsec;
    if (nanoseconds < 0) {
        if (seconds == 0U) return 0U;
        seconds--;
        nanoseconds += 1000000000L;
    }
    if (seconds > UINT64_MAX / 1000000000U) return UINT64_MAX;
    return seconds * 1000000000U + (uint64_t)nanoseconds;
}

static int wait_milliseconds(uint64_t milliseconds) {
    struct timespec request = {
        .tv_sec = (time_t)(milliseconds / 1000U),
        .tv_nsec = (long)((milliseconds % 1000U) * 1000000U)
    };
    while (nanosleep(&request, &request) != 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

static int compare_pid(const void *left_value, const void *right_value) {
    const mon_process *left = left_value;
    const mon_process *right = right_value;
    if (left->pid < right->pid) return -1;
    if (left->pid > right->pid) return 1;
    if (left->start_ticks < right->start_ticks) return -1;
    if (left->start_ticks > right->start_ticks) return 1;
    return 0;
}

static const mon_process *find_previous(const mon_process_snapshot *snapshot,
                                        int pid, uint64_t start_ticks) {
    size_t low = 0U;
    size_t high = snapshot->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        const mon_process *candidate = &snapshot->rows[middle];
        if (candidate->pid < pid
            || (candidate->pid == pid && candidate->start_ticks < start_ticks))
            low = middle + 1U;
        else high = middle;
    }
    if (low >= snapshot->count) return NULL;
    const mon_process *candidate = &snapshot->rows[low];
    return candidate->pid == pid && candidate->start_ticks == start_ticks
        ? candidate : NULL;
}

static void apply_process_rates(mon_process_snapshot *current,
                                mon_process_snapshot *previous,
                                const mon_host_sample *host_current,
                                const mon_host_sample *host_previous,
                                uint64_t elapsed_ns) {
    if (previous->count > 1U)
        qsort(previous->rows, previous->count, sizeof(*previous->rows), compare_pid);
    uint64_t total_delta = difference(host_current->cpu_total_ticks,
                                      host_previous->cpu_total_ticks);
    uint64_t cpu_scale = (uint64_t)host_current->processor_count * 100000U;
    for (size_t i = 0U; i < current->count; i++) {
        mon_process *process = &current->rows[i];
        const mon_process *old = find_previous(previous, process->pid,
                                               process->start_ticks);
        if (!old) continue;
        process->existed_for_sample = true;
        if (total_delta > 0U) {
            uint64_t process_delta = difference(process->cpu_ticks, old->cpu_ticks);
            process->cpu_available = true;
            process->cpu_percent_milli = multiply_divide(process_delta, cpu_scale,
                                                          total_delta);
            if (process->cpu_percent_milli > cpu_scale)
                process->cpu_percent_milli = cpu_scale;
        }
        if (process->io_available && old->io_available) {
            process->read_bytes_per_second = rate_per_second(process->read_bytes,
                                                              old->read_bytes,
                                                              elapsed_ns);
            process->write_bytes_per_second = rate_per_second(process->write_bytes,
                                                               old->write_bytes,
                                                               elapsed_ns);
        } else process->io_available = false;
    }
}

int mon_collect_report(const mon_roots *roots, uint64_t sample_milliseconds,
                       mon_report *report, char *error, size_t error_size) {
    if (!roots || !report || sample_milliseconds < MON_SAMPLE_MIN_MS
        || sample_milliseconds > MON_SAMPLE_MAX_MS) return -1;
    memset(report, 0, sizeof(*report));
    mon_host_sample host_previous;
    mon_host_sample host_current;
    mon_process_snapshot process_previous;
    mon_process_snapshot process_current;
    memset(&process_previous, 0, sizeof(process_previous));
    memset(&process_current, 0, sizeof(process_current));
    if (mon_probe_host(roots, &host_previous, error, error_size) != 0
        || mon_probe_processes(roots, &process_previous, error, error_size) != 0) {
        mon_process_snapshot_free(&process_previous);
        return -1;
    }
    if (wait_milliseconds(sample_milliseconds) != 0) {
        mon_process_snapshot_free(&process_previous);
        if (error && error_size > 0U) snprintf(error, error_size, "sample wait failed");
        return -1;
    }
    if (mon_probe_host(roots, &host_current, error, error_size) != 0
        || mon_probe_processes(roots, &process_current, error, error_size) != 0) {
        mon_process_snapshot_free(&process_previous);
        mon_process_snapshot_free(&process_current);
        return -1;
    }
    uint64_t elapsed_ns = elapsed_nanoseconds(&host_current.observed_at,
                                              &host_previous.observed_at);
    if (elapsed_ns == 0U) {
        mon_process_snapshot_free(&process_previous);
        mon_process_snapshot_free(&process_current);
        if (error && error_size > 0U) snprintf(error, error_size, "invalid sample duration");
        return -1;
    }
    report->sample_milliseconds = elapsed_ns / 1000000U;
    if (report->sample_milliseconds == 0U) report->sample_milliseconds = 1U;

    uint64_t total_delta = difference(host_current.cpu_total_ticks,
                                      host_previous.cpu_total_ticks);
    uint64_t idle_delta = difference(host_current.cpu_idle_ticks,
                                     host_previous.cpu_idle_ticks);
    report->cpu_available = host_current.cpu_available && host_previous.cpu_available
        && total_delta > 0U;
    report->processor_count = host_current.processor_count;
    if (report->cpu_available) {
        uint64_t busy_delta = idle_delta <= total_delta ? total_delta - idle_delta : 0U;
        report->cpu_busy_percent_milli = multiply_divide(busy_delta, 100000U,
                                                         total_delta);
        if (report->cpu_busy_percent_milli > 100000U)
            report->cpu_busy_percent_milli = 100000U;
    }
    report->cpu_count = host_current.cpu_count < MON_MAX_CPUS
        ? host_current.cpu_count : MON_MAX_CPUS;
    report->cpu_truncated = host_current.cpu_truncated || host_previous.cpu_truncated;
    for (size_t i = 0U; i < report->cpu_count; i++) {
        uint64_t entry_total = difference(host_current.cpus[i].total_ticks,
                                          host_previous.cpus[i].total_ticks);
        uint64_t entry_idle = difference(host_current.cpus[i].idle_ticks,
                                         host_previous.cpus[i].idle_ticks);
        if (entry_total == 0U) continue;
        uint64_t entry_busy = entry_idle <= entry_total
            ? entry_total - entry_idle : 0U;
        report->cpu_entry_available[i] = true;
        report->cpu_percent_milli[i] = multiply_divide(entry_busy, 100000U,
                                                        entry_total);
        if (report->cpu_percent_milli[i] > 100000U)
            report->cpu_percent_milli[i] = 100000U;
    }

    report->memory_available = host_current.memory_available;
    report->memory_total_bytes = host_current.memory_total_bytes;
    report->memory_available_bytes = host_current.memory_available_bytes;
    report->memory_used_bytes = host_current.memory_available_bytes
        <= host_current.memory_total_bytes
        ? host_current.memory_total_bytes - host_current.memory_available_bytes : 0U;

    report->disk_available = host_current.disk_available && host_previous.disk_available;
    report->disk_devices = host_current.disk_devices;
    report->disk_devices_skipped = host_current.disk_devices_skipped;
    report->disk_truncated = host_current.disk_truncated || host_previous.disk_truncated;
    if (report->disk_available) {
        report->disk_read_bytes_per_second = multiply_divide(
            difference(host_current.disk_read_sectors,
                       host_previous.disk_read_sectors),
            512000000000U, elapsed_ns);
        report->disk_write_bytes_per_second = multiply_divide(
            difference(host_current.disk_write_sectors,
                       host_previous.disk_write_sectors),
            512000000000U, elapsed_ns);
        size_t output = 0U;
        for (size_t i = 0U; i < host_current.disk_devices
             && output < MON_MAX_BLOCK_DEVICES; i++) {
            for (size_t j = 0U; j < host_previous.disk_devices; j++) {
                if (strcmp(host_current.disks[i].name,
                           host_previous.disks[j].name) != 0) continue;
                (void)snprintf(report->disks[output].name,
                               sizeof(report->disks[output].name), "%s",
                               host_current.disks[i].name);
                report->disks[output].read_bytes_per_second = multiply_divide(
                    difference(host_current.disks[i].read_sectors,
                               host_previous.disks[j].read_sectors),
                    512000000000U, elapsed_ns);
                report->disks[output].write_bytes_per_second = multiply_divide(
                    difference(host_current.disks[i].write_sectors,
                               host_previous.disks[j].write_sectors),
                    512000000000U, elapsed_ns);
                output++;
                break;
            }
        }
        report->disk_devices = output;
    }

    report->network_available = host_current.network_available
        && host_previous.network_available;
    report->network_interfaces = host_current.network_interfaces;
    report->network_truncated = host_current.network_truncated
        || host_previous.network_truncated;
    if (report->network_available) {
        report->network_rx_bytes_per_second = rate_per_second(
            host_current.network_rx_bytes, host_previous.network_rx_bytes,
            elapsed_ns);
        report->network_tx_bytes_per_second = rate_per_second(
            host_current.network_tx_bytes, host_previous.network_tx_bytes,
            elapsed_ns);
        size_t output = 0U;
        for (size_t i = 0U; i < host_current.network_interfaces
             && output < MON_MAX_INTERFACES; i++) {
            for (size_t j = 0U; j < host_previous.network_interfaces; j++) {
                if (strcmp(host_current.interfaces[i].name,
                           host_previous.interfaces[j].name) != 0) continue;
                (void)snprintf(report->interfaces[output].name,
                               sizeof(report->interfaces[output].name), "%s",
                               host_current.interfaces[i].name);
                report->interfaces[output].receive_bytes_per_second = rate_per_second(
                    host_current.interfaces[i].receive_bytes,
                    host_previous.interfaces[j].receive_bytes, elapsed_ns);
                report->interfaces[output].transmit_bytes_per_second = rate_per_second(
                    host_current.interfaces[i].transmit_bytes,
                    host_previous.interfaces[j].transmit_bytes, elapsed_ns);
                output++;
                break;
            }
        }
        report->network_interfaces = output;
    }

    report->gpu_present = host_current.gpu_present;
    report->gpu_available = host_current.gpu_available;
    report->gpu_card = host_current.gpu_card;
    report->gpu_busy_percent_milli = host_current.gpu_busy_percent_milli;
    report->gpu_memory_available = host_current.gpu_memory_available;
    report->gpu_memory_used_bytes = host_current.gpu_memory_used_bytes;
    report->gpu_memory_total_available = host_current.gpu_memory_total_available;
    report->gpu_memory_total_bytes = host_current.gpu_memory_total_bytes;
    report->gpu_count = host_current.gpu_count;
    report->gpu_truncated = host_current.gpu_truncated || host_previous.gpu_truncated;
    if (report->gpu_count > 0U)
        memcpy(report->gpus, host_current.gpus,
               report->gpu_count * sizeof(report->gpus[0]));
    report->temperature_count = host_current.temperature_count;
    report->temperature_truncated = host_current.temperature_truncated;
    if (report->temperature_count > 0U)
        memcpy(report->temperatures, host_current.temperatures,
               report->temperature_count * sizeof(report->temperatures[0]));
    report->fan_count = host_current.fan_count;
    report->fan_truncated = host_current.fan_truncated;
    if (report->fan_count > 0U)
        memcpy(report->fans, host_current.fans,
               report->fan_count * sizeof(report->fans[0]));
    report->hwmon_devices_seen = host_current.hwmon_devices_seen;
    report->sensor_permission_denied = host_current.sensor_permission_denied;
    report->sensor_malformed = host_current.sensor_malformed;

    apply_process_rates(&process_current, &process_previous, &host_current,
                        &host_previous, elapsed_ns);
    mon_process_snapshot_free(&process_previous);
    report->processes = process_current;
    report->observed_rows = process_current.count;
    report->matched_rows = process_current.count;
    return 0;
}

void mon_report_free(mon_report *report) {
    if (!report) return;
    mon_process_snapshot_free(&report->processes);
    memset(report, 0, sizeof(*report));
}

void mon_history_update(mon_history *history, const mon_report *report) {
    if (!history || !report) return;
    size_t index = history->next;
    history->cpu_available[index] = report->cpu_available;
    history->cpu[index] = report->cpu_available
        ? report->cpu_busy_percent_milli : 0U;
    history->memory_available[index] = report->memory_available
        && report->memory_total_bytes > 0U;
    history->memory[index] = history->memory_available[index]
        ? multiply_divide(report->memory_used_bytes, 100000U,
                          report->memory_total_bytes) : 0U;
    history->gpu_available[index] = report->gpu_available;
    history->gpu[index] = report->gpu_available
        ? report->gpu_busy_percent_milli : 0U;
    history->disk_available[index] = report->disk_available;
    history->disk[index] = report->disk_read_bytes_per_second
        > UINT64_MAX - report->disk_write_bytes_per_second
        ? UINT64_MAX : report->disk_read_bytes_per_second
          + report->disk_write_bytes_per_second;
    history->network_available[index] = report->network_available;
    history->network[index] = report->network_rx_bytes_per_second
        > UINT64_MAX - report->network_tx_bytes_per_second
        ? UINT64_MAX : report->network_rx_bytes_per_second
          + report->network_tx_bytes_per_second;
    history->next = (index + 1U) % MON_HISTORY_SAMPLES;
    if (history->count < MON_HISTORY_SAMPLES) history->count++;
}

static bool ascii_contains_casefold(const char *text, const char *needle) {
    if (!needle || !*needle) return true;
    size_t needle_length = strlen(needle);
    for (size_t offset = 0U; text[offset]; offset++) {
        size_t i = 0U;
        while (i < needle_length && text[offset + i]
               && tolower((unsigned char)text[offset + i])
                  == tolower((unsigned char)needle[i])) i++;
        if (i == needle_length) return true;
    }
    return false;
}

static mon_sort active_sort = MON_SORT_CPU;
static mon_group active_group = MON_GROUP_CLASS;

static int compare_metric(const mon_process *left, const mon_process *right) {
    uint64_t left_value = 0U;
    uint64_t right_value = 0U;
    switch (active_sort) {
        case MON_SORT_MEMORY:
            left_value = left->resident_bytes;
            right_value = right->resident_bytes;
            break;
        case MON_SORT_READ:
            left_value = left->read_bytes_per_second;
            right_value = right->read_bytes_per_second;
            break;
        case MON_SORT_WRITE:
            left_value = left->write_bytes_per_second;
            right_value = right->write_bytes_per_second;
            break;
        case MON_SORT_NAME: {
            int compared = strcmp(left->name, right->name);
            if (compared != 0) return compared;
            break;
        }
        case MON_SORT_CLASS: {
            int compared = strcmp(mon_class_id(left->process_class),
                                  mon_class_id(right->process_class));
            if (compared != 0) return compared;
            break;
        }
        case MON_SORT_USER:
            if (left->uid_available != right->uid_available)
                return left->uid_available ? -1 : 1;
            if (left->uid < right->uid) return -1;
            if (left->uid > right->uid) return 1;
            break;
        case MON_SORT_STATE:
            if (left->state < right->state) return -1;
            if (left->state > right->state) return 1;
            break;
        case MON_SORT_THREADS:
            left_value = left->threads;
            right_value = right->threads;
            break;
        case MON_SORT_PID:
            if (left->pid < right->pid) return -1;
            if (left->pid > right->pid) return 1;
            break;
        case MON_SORT_CPU:
        default:
            left_value = left->cpu_percent_milli;
            right_value = right->cpu_percent_milli;
            break;
    }
    if (left_value > right_value) return -1;
    if (left_value < right_value) return 1;
    int named = strcmp(left->name, right->name);
    if (named != 0) return named;
    if (left->pid < right->pid) return -1;
    if (left->pid > right->pid) return 1;
    return 0;
}

static int compare_report_row(const void *left_value, const void *right_value) {
    const mon_process *left = left_value;
    const mon_process *right = right_value;
    if (active_group == MON_GROUP_CLASS) {
        if (left->process_class < right->process_class) return -1;
        if (left->process_class > right->process_class) return 1;
    } else if (active_group == MON_GROUP_NAME) {
        int named = strcmp(left->name, right->name);
        if (named != 0) return named;
    }
    return compare_metric(left, right);
}

static void select_balanced_classes(mon_report *report, size_t limit) {
    size_t starts[3] = {0U, 0U, 0U};
    size_t counts[3] = {0U, 0U, 0U};
    size_t cursor = 0U;
    for (unsigned group = 0U; group < 3U; group++) {
        starts[group] = cursor;
        while (cursor < report->processes.count
               && (unsigned)report->processes.rows[cursor].process_class == group) {
            counts[group]++;
            cursor++;
        }
    }
    size_t nonempty = 0U;
    for (unsigned group = 0U; group < 3U; group++)
        if (counts[group] > 0U) nonempty++;
    if (nonempty == 0U || limit == 0U) {
        report->processes.count = 0U;
        return;
    }
    size_t selected[3] = {0U, 0U, 0U};
    size_t base = limit / nonempty;
    size_t remainder = limit % nonempty;
    for (unsigned group = 0U; group < 3U; group++) {
        if (counts[group] == 0U) continue;
        size_t wanted = base + (remainder > 0U ? 1U : 0U);
        if (remainder > 0U) remainder--;
        selected[group] = counts[group] < wanted ? counts[group] : wanted;
    }
    size_t total = selected[0] + selected[1] + selected[2];
    while (total < limit) {
        bool advanced = false;
        for (unsigned group = 0U; group < 3U && total < limit; group++) {
            if (selected[group] >= counts[group]) continue;
            selected[group]++;
            total++;
            advanced = true;
        }
        if (!advanced) break;
    }
    mon_process *balanced = malloc(total * sizeof(*balanced));
    if (!balanced) {
        report->processes.count = report->processes.count < limit
            ? report->processes.count : limit;
        return;
    }
    size_t output = 0U;
    for (unsigned group = 0U; group < 3U; group++) {
        for (size_t i = 0U; i < selected[group]; i++)
            balanced[output++] = report->processes.rows[starts[group] + i];
    }
    memcpy(report->processes.rows, balanced, total * sizeof(*balanced));
    free(balanced);
    report->processes.count = total;
}

void mon_prepare_report(mon_report *report, const mon_options *options) {
    if (!report || !options) return;
    size_t kept = 0U;
    for (size_t i = 0U; i < report->processes.count; i++) {
        mon_process process = report->processes.rows[i];
        if (!ascii_contains_casefold(process.name, options->filter)) continue;
        report->processes.rows[kept++] = process;
    }
    report->processes.count = kept;
    report->matched_rows = kept;
    active_sort = options->sort;
    active_group = options->group;
    if (kept > 1U)
        qsort(report->processes.rows, kept, sizeof(*report->processes.rows),
              compare_report_row);
    if (kept > options->limit) {
        if (options->group == MON_GROUP_CLASS)
            select_balanced_classes(report, options->limit);
        else report->processes.count = options->limit;
    }
}
