// SPDX-License-Identifier: MIT
#ifndef SYNAPSE_MONITOR_H
#define SYNAPSE_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define MON_VERSION SYNAPSE_MONITOR_VERSION
#define MON_MAX_PROCESSES 32768U
#define MON_MAX_OUTPUT_ROWS 512U
#define MON_NAME_MAX 64U
#define MON_FILTER_MAX 64U
#define MON_PATH_MAX 4096U
#define MON_ERROR_MAX 256U
#define MON_MAX_BLOCK_DEVICES 256U
#define MON_MAX_INTERFACES 256U
#define MON_STAT_FILE_LIMIT ((size_t)64U * 1024U)
#define MON_PROCESS_FILE_LIMIT ((size_t)32U * 1024U)
#define MON_DISKSTATS_LIMIT ((size_t)256U * 1024U)
#define MON_NETDEV_LIMIT ((size_t)128U * 1024U)
#define MON_SAMPLE_MIN_MS 100U
#define MON_SAMPLE_MAX_MS 2000U
#define MON_INTERVAL_MIN_MS 250U
#define MON_INTERVAL_MAX_MS 10000U

#define MON_COLUMN_NAME    (1U << 0)
#define MON_COLUMN_PID     (1U << 1)
#define MON_COLUMN_USER    (1U << 2)
#define MON_COLUMN_STATE   (1U << 3)
#define MON_COLUMN_THREADS (1U << 4)
#define MON_COLUMN_CPU     (1U << 5)
#define MON_COLUMN_MEMORY  (1U << 6)
#define MON_COLUMN_READ    (1U << 7)
#define MON_COLUMN_WRITE   (1U << 8)
#define MON_COLUMNS_ALL ((1U << 9) - 1U)
#define MON_COLUMNS_DEFAULT (MON_COLUMN_NAME | MON_COLUMN_PID | MON_COLUMN_USER \
                             | MON_COLUMN_CPU | MON_COLUMN_MEMORY \
                             | MON_COLUMN_READ | MON_COLUMN_WRITE)

typedef enum {
    MON_FORMAT_TEXT = 0,
    MON_FORMAT_JSON
} mon_format;

typedef enum {
    MON_SORT_CPU = 0,
    MON_SORT_MEMORY,
    MON_SORT_READ,
    MON_SORT_WRITE,
    MON_SORT_NAME,
    MON_SORT_PID
} mon_sort;

typedef enum {
    MON_GROUP_CLASS = 0,
    MON_GROUP_NAME,
    MON_GROUP_NONE
} mon_group;

typedef enum {
    MON_CLASS_APPLICATION = 0,
    MON_CLASS_SYSTEM,
    MON_CLASS_KERNEL
} mon_process_class;

typedef struct {
    const char *proc_root;
    const char *sys_root;
} mon_roots;

typedef struct {
    bool cpu_available;
    uint64_t cpu_total_ticks;
    uint64_t cpu_idle_ticks;
    unsigned processor_count;

    bool memory_available;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;

    bool disk_available;
    uint64_t disk_read_sectors;
    uint64_t disk_write_sectors;
    size_t disk_devices;
    size_t disk_devices_skipped;
    bool disk_truncated;

    bool network_available;
    uint64_t network_rx_bytes;
    uint64_t network_tx_bytes;
    size_t network_interfaces;
    bool network_truncated;

    bool gpu_available;
    unsigned gpu_card;
    uint64_t gpu_busy_percent_milli;
    bool gpu_memory_available;
    uint64_t gpu_memory_used_bytes;
    uint64_t gpu_memory_total_bytes;

    struct timespec observed_at;
} mon_host_sample;

typedef struct {
    int pid;
    bool uid_available;
    unsigned uid;
    char name[MON_NAME_MAX + 1U];
    char state;
    uint64_t threads;
    uint64_t cpu_ticks;
    uint64_t start_ticks;
    uint64_t resident_bytes;
    bool io_available;
    uint64_t read_bytes;
    uint64_t write_bytes;
    mon_process_class process_class;

    bool existed_for_sample;
    bool cpu_available;
    uint64_t cpu_percent_milli;
    uint64_t read_bytes_per_second;
    uint64_t write_bytes_per_second;
} mon_process;

typedef struct {
    mon_process *rows;
    size_t count;
    size_t directories_seen;
    size_t numeric_directories;
    size_t permission_denied;
    size_t vanished;
    size_t malformed;
    size_t io_unavailable;
    bool truncated;
} mon_process_snapshot;

typedef struct {
    uint64_t sample_milliseconds;
    bool cpu_available;
    uint64_t cpu_busy_percent_milli;
    unsigned processor_count;
    bool memory_available;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;
    uint64_t memory_used_bytes;
    bool disk_available;
    uint64_t disk_read_bytes_per_second;
    uint64_t disk_write_bytes_per_second;
    size_t disk_devices;
    size_t disk_devices_skipped;
    bool disk_truncated;
    bool network_available;
    uint64_t network_rx_bytes_per_second;
    uint64_t network_tx_bytes_per_second;
    size_t network_interfaces;
    bool network_truncated;
    bool gpu_available;
    unsigned gpu_card;
    uint64_t gpu_busy_percent_milli;
    bool gpu_memory_available;
    uint64_t gpu_memory_used_bytes;
    uint64_t gpu_memory_total_bytes;
    mon_process_snapshot processes;
    size_t observed_rows;
    size_t matched_rows;
} mon_report;

typedef struct {
    mon_format format;
    mon_sort sort;
    mon_group group;
    uint32_t columns;
    char filter[MON_FILTER_MAX + 1U];
    size_t limit;
    uint64_t sample_milliseconds;
    uint64_t interval_milliseconds;
    uint64_t iterations;
} mon_options;

int mon_roots_from_environment(mon_roots *roots, char *error, size_t error_size);
int mon_probe_host(const mon_roots *roots, mon_host_sample *sample,
                   char *error, size_t error_size);
int mon_probe_processes(const mon_roots *roots, mon_process_snapshot *snapshot,
                        char *error, size_t error_size);
void mon_process_snapshot_free(mon_process_snapshot *snapshot);
int mon_collect_report(const mon_roots *roots, uint64_t sample_milliseconds,
                       mon_report *report, char *error, size_t error_size);
void mon_report_free(mon_report *report);
void mon_prepare_report(mon_report *report, const mon_options *options);

int mon_parse_format(const char *value, mon_format *format);
int mon_parse_sort(const char *value, mon_sort *sort);
int mon_parse_group(const char *value, mon_group *group);
int mon_parse_columns(const char *value, uint32_t *columns);
int mon_parse_u64(const char *value, uint64_t minimum, uint64_t maximum,
                  uint64_t *result);
int mon_parse_filter(const char *value, char output[MON_FILTER_MAX + 1U]);
const char *mon_sort_id(mon_sort sort);
const char *mon_group_id(mon_group group);
const char *mon_class_id(mon_process_class process_class);

int mon_render_report(const mon_report *report, const mon_options *options);
void mon_usage(FILE *output);
int mon_run_watch(const mon_roots *roots, mon_options *options);

#endif
