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
#define MON_LABEL_MAX 128U
#define MON_FILTER_MAX 64U
#define MON_PATH_MAX 4096U
#define MON_ERROR_MAX 256U
#define MON_MAX_CPUS 4096U
#define MON_MAX_BLOCK_DEVICES 256U
#define MON_MAX_INTERFACES 256U
#define MON_MAX_GPUS 16U
#define MON_MAX_TEMPERATURES 256U
#define MON_MAX_FANS 128U
#define MON_MAX_HWMON_DEVICES 256U
#define MON_MAX_SENSOR_CHANNELS 32U
#define MON_SENSOR_LABEL_MAX 96U
#define MON_MAX_SERVICES 4096U
#define MON_MAX_STARTUP_ITEMS 1024U
#define MON_MAX_CONNECTIONS 4096U
#define MON_MAX_MODULES 512U
#define MON_MAX_FD_LINKS 131072U
#define MON_HISTORY_SAMPLES 60U
#define MON_STREAM_LINE_MAX ((size_t)2U * 1024U * 1024U)
#define MON_STAT_FILE_LIMIT ((size_t)64U * 1024U)
#define MON_PROCESS_FILE_LIMIT ((size_t)32U * 1024U)
#define MON_UNIT_FILE_LIMIT ((size_t)64U * 1024U)
#define MON_DESKTOP_FILE_LIMIT ((size_t)64U * 1024U)
#define MON_DISKSTATS_LIMIT ((size_t)256U * 1024U)
#define MON_NETDEV_LIMIT ((size_t)128U * 1024U)
#define MON_CONNECTION_FILE_LIMIT ((size_t)512U * 1024U)
#define MON_MAPS_FILE_LIMIT ((size_t)2U * 1024U * 1024U)
#define MON_PCI_IDS_LIMIT ((size_t)4U * 1024U * 1024U)
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
#define MON_PROCESS_COLUMNS_ALL ((1U << 9) - 1U)
#define MON_PROCESS_COLUMNS_DEFAULT (MON_COLUMN_NAME | MON_COLUMN_PID \
    | MON_COLUMN_USER | MON_COLUMN_CPU | MON_COLUMN_MEMORY \
    | MON_COLUMN_READ | MON_COLUMN_WRITE)

#define MON_SERVICE_COLUMN_NAME        (1U << 0)
#define MON_SERVICE_COLUMN_DESCRIPTION (1U << 1)
#define MON_SERVICE_COLUMN_STATUS      (1U << 2)
#define MON_SERVICE_COLUMN_STARTUP     (1U << 3)
#define MON_SERVICE_COLUMN_PID         (1U << 4)
#define MON_SERVICE_COLUMN_USER        (1U << 5)
#define MON_SERVICE_COLUMN_EXECUTABLE  (1U << 6)
#define MON_SERVICE_COLUMNS_ALL ((1U << 7) - 1U)
#define MON_SERVICE_COLUMNS_DEFAULT MON_SERVICE_COLUMNS_ALL

#define MON_STARTUP_COLUMN_NAME      (1U << 0)
#define MON_STARTUP_COLUMN_PUBLISHER (1U << 1)
#define MON_STARTUP_COLUMN_STATUS    (1U << 2)
#define MON_STARTUP_COLUMN_TYPE      (1U << 3)
#define MON_STARTUP_COLUMN_LOCATION  (1U << 4)
#define MON_STARTUP_COLUMN_COMMAND   (1U << 5)
#define MON_STARTUP_COLUMN_SCOPE MON_STARTUP_COLUMN_LOCATION
#define MON_STARTUP_COLUMN_LAUNCH MON_STARTUP_COLUMN_COMMAND
#define MON_STARTUP_COLUMNS_ALL ((1U << 6) - 1U)
#define MON_STARTUP_COLUMNS_DEFAULT MON_STARTUP_COLUMNS_ALL

#define MON_CONNECTION_COLUMN_PROTOCOL (1U << 0)
#define MON_CONNECTION_COLUMN_LOCAL    (1U << 1)
#define MON_CONNECTION_COLUMN_REMOTE   (1U << 2)
#define MON_CONNECTION_COLUMN_STATE    (1U << 3)
#define MON_CONNECTION_COLUMN_PID      (1U << 4)
#define MON_CONNECTION_COLUMN_PROCESS  (1U << 5)
#define MON_CONNECTION_COLUMNS_ALL ((1U << 6) - 1U)
#define MON_CONNECTION_COLUMNS_DEFAULT MON_CONNECTION_COLUMNS_ALL

typedef enum {
    MON_FORMAT_TEXT = 0,
    MON_FORMAT_JSON,
    MON_FORMAT_NDJSON
} mon_format;

typedef enum {
    MON_VIEW_PROCESSES = 0,
    MON_VIEW_PERFORMANCE,
    MON_VIEW_SERVICES,
    MON_VIEW_STARTUP,
    MON_VIEW_CONNECTIONS,
    MON_VIEW_INFORMATION,
    MON_VIEW_COUNT
} mon_view;

typedef enum {
    MON_THEME_DEFAULT = 0,
    MON_THEME_CONTRAST,
    MON_THEME_MONO
} mon_theme;

typedef enum {
    MON_LAYOUT_DENSE = 0,
    MON_LAYOUT_BALANCED,
    MON_LAYOUT_WIDE
} mon_layout;

typedef enum {
    MON_SORT_CPU = 0,
    MON_SORT_MEMORY,
    MON_SORT_READ,
    MON_SORT_WRITE,
    MON_SORT_NAME,
    MON_SORT_PID,
    MON_SORT_STATUS,
    MON_SORT_STARTUP,
    MON_SORT_PROTOCOL,
    MON_SORT_LOCAL,
    MON_SORT_REMOTE,
    MON_SORT_SCOPE,
    MON_SORT_TYPE,
    MON_SORT_DESCRIPTION,
    MON_SORT_USER,
    MON_SORT_STATE,
    MON_SORT_THREADS,
    MON_SORT_EXECUTABLE,
    MON_SORT_PUBLISHER,
    MON_SORT_COMMAND,
    MON_SORT_PROCESS,
    MON_SORT_CLASS,
    MON_SORT_LOCATION
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
    const char *etc_root;
    const char *usr_root;
    const char *run_root;
    const char *home_root;
} mon_roots;

typedef struct {
    uint64_t total_ticks;
    uint64_t idle_ticks;
} mon_cpu_counter;

typedef struct {
    char name[MON_NAME_MAX + 1U];
    uint64_t read_sectors;
    uint64_t write_sectors;
} mon_disk_counter;

typedef struct {
    char name[MON_NAME_MAX + 1U];
    uint64_t receive_bytes;
    uint64_t transmit_bytes;
} mon_interface_counter;

typedef enum {
    MON_THERMAL_CPU_PACKAGE = 0,
    MON_THERMAL_CPU_CORE,
    MON_THERMAL_GPU,
    MON_THERMAL_STORAGE,
    MON_THERMAL_BATTERY,
    MON_THERMAL_SYSTEM,
    MON_THERMAL_OTHER
} mon_thermal_class;

typedef struct {
    unsigned card;
    char vendor_id[8];
    char device_id[8];
    char driver[MON_NAME_MAX + 1U];
    char model[MON_LABEL_MAX + 1U];
    bool utilization_available;
    uint64_t utilization_percent_milli;
    bool memory_available;
    uint64_t memory_used_bytes;
    uint64_t memory_total_bytes;
    bool temperature_available;
    int64_t temperature_millidegrees_celsius;
    char temperature_label[MON_SENSOR_LABEL_MAX + 1U];
    bool core_clock_available;
    uint64_t core_clock_hz;
    bool core_clock_max_available;
    uint64_t core_clock_max_hz;
    bool memory_clock_available;
    uint64_t memory_clock_hz;
    bool power_available;
    uint64_t power_microwatts;
    bool power_cap_available;
    uint64_t power_cap_microwatts;
    bool fan_available;
    uint64_t fan_rpm;
} mon_gpu;

typedef struct {
    mon_thermal_class sensor_class;
    char source[MON_NAME_MAX + 1U];
    char label[MON_SENSOR_LABEL_MAX + 1U];
    int64_t temperature_millidegrees_celsius;
    bool maximum_available;
    int64_t maximum_millidegrees_celsius;
    bool critical_available;
    int64_t critical_millidegrees_celsius;
} mon_temperature;

typedef struct {
    char source[MON_NAME_MAX + 1U];
    char label[MON_SENSOR_LABEL_MAX + 1U];
    uint64_t rpm;
} mon_fan;

typedef struct {
    bool cpu_available;
    uint64_t cpu_total_ticks;
    uint64_t cpu_idle_ticks;
    unsigned processor_count;
    mon_cpu_counter cpus[MON_MAX_CPUS];
    size_t cpu_count;
    bool cpu_truncated;

    bool memory_available;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;

    bool disk_available;
    uint64_t disk_read_sectors;
    uint64_t disk_write_sectors;
    mon_disk_counter disks[MON_MAX_BLOCK_DEVICES];
    size_t disk_devices;
    size_t disk_devices_skipped;
    bool disk_truncated;

    bool network_available;
    uint64_t network_rx_bytes;
    uint64_t network_tx_bytes;
    mon_interface_counter interfaces[MON_MAX_INTERFACES];
    size_t network_interfaces;
    bool network_truncated;

    bool gpu_present;
    bool gpu_available;
    unsigned gpu_card;
    uint64_t gpu_busy_percent_milli;
    bool gpu_memory_available;
    uint64_t gpu_memory_used_bytes;
    uint64_t gpu_memory_total_bytes;
    mon_gpu gpus[MON_MAX_GPUS];
    size_t gpu_count;
    bool gpu_truncated;

    mon_temperature temperatures[MON_MAX_TEMPERATURES];
    size_t temperature_count;
    bool temperature_truncated;
    mon_fan fans[MON_MAX_FANS];
    size_t fan_count;
    bool fan_truncated;
    size_t hwmon_devices_seen;
    size_t sensor_permission_denied;
    size_t sensor_malformed;

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
    char name[MON_NAME_MAX + 1U];
    uint64_t read_bytes_per_second;
    uint64_t write_bytes_per_second;
} mon_disk_rate;

typedef struct {
    char name[MON_NAME_MAX + 1U];
    uint64_t receive_bytes_per_second;
    uint64_t transmit_bytes_per_second;
} mon_interface_rate;

typedef struct {
    uint64_t sample_milliseconds;
    bool cpu_available;
    uint64_t cpu_busy_percent_milli;
    unsigned processor_count;
    uint64_t cpu_percent_milli[MON_MAX_CPUS];
    bool cpu_entry_available[MON_MAX_CPUS];
    size_t cpu_count;
    bool cpu_truncated;
    bool memory_available;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;
    uint64_t memory_used_bytes;
    bool disk_available;
    uint64_t disk_read_bytes_per_second;
    uint64_t disk_write_bytes_per_second;
    mon_disk_rate disks[MON_MAX_BLOCK_DEVICES];
    size_t disk_devices;
    size_t disk_devices_skipped;
    bool disk_truncated;
    bool network_available;
    uint64_t network_rx_bytes_per_second;
    uint64_t network_tx_bytes_per_second;
    mon_interface_rate interfaces[MON_MAX_INTERFACES];
    size_t network_interfaces;
    bool network_truncated;
    bool gpu_present;
    bool gpu_available;
    unsigned gpu_card;
    uint64_t gpu_busy_percent_milli;
    bool gpu_memory_available;
    uint64_t gpu_memory_used_bytes;
    uint64_t gpu_memory_total_bytes;
    mon_gpu gpus[MON_MAX_GPUS];
    size_t gpu_count;
    bool gpu_truncated;
    mon_temperature temperatures[MON_MAX_TEMPERATURES];
    size_t temperature_count;
    bool temperature_truncated;
    mon_fan fans[MON_MAX_FANS];
    size_t fan_count;
    bool fan_truncated;
    size_t hwmon_devices_seen;
    size_t sensor_permission_denied;
    size_t sensor_malformed;
    mon_process_snapshot processes;
    size_t observed_rows;
    size_t matched_rows;
} mon_report;

typedef struct {
    uint64_t cpu[MON_HISTORY_SAMPLES];
    uint64_t memory[MON_HISTORY_SAMPLES];
    uint64_t gpu[MON_HISTORY_SAMPLES];
    uint64_t disk[MON_HISTORY_SAMPLES];
    uint64_t network[MON_HISTORY_SAMPLES];
    bool cpu_available[MON_HISTORY_SAMPLES];
    bool memory_available[MON_HISTORY_SAMPLES];
    bool gpu_available[MON_HISTORY_SAMPLES];
    bool disk_available[MON_HISTORY_SAMPLES];
    bool network_available[MON_HISTORY_SAMPLES];
    size_t count;
    size_t next;
} mon_history;

typedef struct {
    char name[MON_LABEL_MAX + 1U];
    char description[MON_LABEL_MAX + 1U];
    char status[16];
    char startup[16];
    char user[MON_NAME_MAX + 1U];
    char executable[MON_LABEL_MAX + 1U];
    bool pid_available;
    int pid;
} mon_service;

typedef struct {
    mon_service *rows;
    size_t count;
    size_t files_seen;
    size_t malformed;
    size_t denied;
    size_t matched;
    bool truncated;
} mon_service_snapshot;

typedef struct {
    char id[MON_LABEL_MAX + 1U];
    char name[MON_LABEL_MAX + 1U];
    char publisher[MON_LABEL_MAX + 1U];
    char status[16];
    char type[16];
    char scope[16];
    char location[32];
    char command[MON_LABEL_MAX + 1U];
    bool launch_present;
} mon_startup_item;

typedef struct {
    mon_startup_item *rows;
    size_t count;
    size_t files_seen;
    size_t malformed;
    size_t denied;
    size_t matched;
    bool truncated;
} mon_startup_snapshot;

typedef struct {
    char protocol[8];
    char local[64];
    char remote[64];
    char state[20];
    uint64_t inode;
    bool pid_available;
    int pid;
    char process[MON_NAME_MAX + 1U];
} mon_connection;

typedef struct {
    mon_connection *rows;
    size_t count;
    size_t rows_seen;
    size_t malformed;
    size_t denied;
    size_t identity_unavailable;
    size_t owner_fds_seen;
    size_t matched;
    bool truncated;
    bool owner_scan_truncated;
} mon_connection_snapshot;

typedef struct {
    int pid;
    uint64_t start_ticks;
    char name[MON_NAME_MAX + 1U];
    bool credentials_available;
    unsigned uids[4];
    unsigned gids[4];
    char capability_inheritable[65];
    char capability_permitted[65];
    char capability_effective[65];
    char capability_bounding[65];
    char capability_ambient[65];
    bool no_new_privileges_available;
    bool no_new_privileges;
    bool seccomp_available;
    unsigned seccomp_mode;
    char modules[MON_MAX_MODULES][MON_NAME_MAX + 1U];
    size_t module_count;
    size_t module_rows_seen;
    bool module_truncated;
    bool module_denied;
    size_t fd_count;
    size_t socket_count;
    bool fd_truncated;
    bool fd_denied;
} mon_process_inspection;

typedef struct {
    char operating_system[MON_LABEL_MAX + 1U];
    char kernel[MON_LABEL_MAX + 1U];
    char architecture[32];
    char processor[MON_LABEL_MAX + 1U];
    char system_vendor[MON_LABEL_MAX + 1U];
    char system_model[MON_LABEL_MAX + 1U];
    char firmware_vendor[MON_LABEL_MAX + 1U];
    char firmware_version[MON_LABEL_MAX + 1U];
    char firmware_date[32];
    bool memory_available;
    uint64_t memory_total_bytes;
    bool uptime_available;
    uint64_t uptime_seconds;
    size_t unavailable_fields;
} mon_information;

typedef struct {
    mon_format format;
    mon_view view;
    mon_sort sort;
    mon_group group;
    mon_theme theme;
    mon_layout layout;
    uint32_t columns;
    char filter[MON_FILTER_MAX + 1U];
    size_t limit;
    uint64_t sample_milliseconds;
    uint64_t interval_milliseconds;
    uint64_t iterations;
    bool interactive_output;
    bool stream_output;
    uint64_t stream_sequence;
    const mon_history *history;
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
void mon_history_update(mon_history *history, const mon_report *report);

int mon_collect_services(const mon_roots *roots, mon_service_snapshot *snapshot,
                         char *error, size_t error_size);
void mon_prepare_services(mon_service_snapshot *snapshot,
                          const mon_options *options);
void mon_service_snapshot_free(mon_service_snapshot *snapshot);
int mon_collect_startup(const mon_roots *roots, mon_startup_snapshot *snapshot,
                        char *error, size_t error_size);
void mon_prepare_startup(mon_startup_snapshot *snapshot,
                         const mon_options *options);
void mon_startup_snapshot_free(mon_startup_snapshot *snapshot);
int mon_collect_connections(const mon_roots *roots,
                            mon_connection_snapshot *snapshot,
                            char *error, size_t error_size);
void mon_prepare_connections(mon_connection_snapshot *snapshot,
                             const mon_options *options);
void mon_connection_snapshot_free(mon_connection_snapshot *snapshot);
int mon_collect_process_inspection(const mon_roots *roots, int pid,
                                   mon_process_inspection *inspection,
                                   char *error, size_t error_size);
int mon_collect_information(const mon_roots *roots, mon_information *information,
                            char *error, size_t error_size);

int mon_parse_format(const char *value, mon_format *format);
int mon_parse_view(const char *value, mon_view *view);
int mon_parse_sort(const char *value, mon_sort *sort);
int mon_parse_group(const char *value, mon_group *group);
int mon_parse_theme(const char *value, mon_theme *theme);
int mon_parse_layout(const char *value, mon_layout *layout);
int mon_parse_columns_for_view(mon_view view, const char *value,
                               uint32_t *columns);
int mon_parse_columns(const char *value, uint32_t *columns);
int mon_parse_u64(const char *value, uint64_t minimum, uint64_t maximum,
                  uint64_t *result);
int mon_parse_filter(const char *value, char output[MON_FILTER_MAX + 1U]);
uint32_t mon_default_columns(mon_view view);
mon_sort mon_default_sort(mon_view view);
const char *mon_view_id(mon_view view);
const char *mon_sort_id(mon_sort sort);
const char *mon_group_id(mon_group group);
const char *mon_theme_id(mon_theme theme);
const char *mon_layout_id(mon_layout layout);
const char *mon_class_id(mon_process_class process_class);
const char *mon_thermal_class_id(mon_thermal_class sensor_class);

int mon_render_report(const mon_report *report, const mon_options *options);
int mon_render_services(const mon_service_snapshot *snapshot,
                        const mon_options *options);
int mon_render_startup(const mon_startup_snapshot *snapshot,
                       const mon_options *options);
int mon_render_connections(const mon_connection_snapshot *snapshot,
                           const mon_options *options);
int mon_render_process_inspection(const mon_process_inspection *inspection,
                                  mon_format format);
int mon_render_information(const mon_information *information,
                           const mon_options *options);
int mon_render_presentation(void);
void mon_render_stream_metadata(const mon_options *options);
void mon_usage(FILE *output);
int mon_run_watch(const mon_roots *roots, mon_options *options);
int mon_run_stream(const mon_roots *roots, mon_options *options);

#endif
