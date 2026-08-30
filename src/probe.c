// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static uint64_t add_saturating(uint64_t left, uint64_t right) {
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static uint64_t multiply_saturating(uint64_t left, uint64_t right) {
    return left != 0U && right > UINT64_MAX / left ? UINT64_MAX : left * right;
}

static bool safe_absolute_root(const char *value) {
    if (!value || value[0] != '/' || strlen(value) >= MON_PATH_MAX - 128U) return false;
    if (value[1] == '\0') return true;
    const char *cursor = value + 1;
    while (*cursor) {
        const char *slash = strchr(cursor, '/');
        size_t length = slash ? (size_t)(slash - cursor) : strlen(cursor);
        if (length == 0U || (length == 1U && cursor[0] == '.')
            || (length == 2U && cursor[0] == '.' && cursor[1] == '.')) return false;
        if (!slash) break;
        cursor = slash + 1;
        if (!*cursor) return false;
    }
    return true;
}

int mon_roots_from_environment(mon_roots *roots, char *error, size_t error_size) {
    if (!roots) return -1;
    roots->proc_root = "/proc";
    roots->sys_root = "/sys";
    const char *allow = getenv("SYNAPSE_MONITOR_ALLOW_TEST_ROOTS");
    if (!allow || strcmp(allow, "1") != 0) return 0;
    const char *proc_root = getenv("SYNAPSE_MONITOR_PROC_ROOT");
    const char *sys_root = getenv("SYNAPSE_MONITOR_SYS_ROOT");
    if (!safe_absolute_root(proc_root) || !safe_absolute_root(sys_root)) {
        if (error && error_size > 0U)
            snprintf(error, error_size, "invalid absolute normalized test root");
        return -1;
    }
    roots->proc_root = proc_root;
    roots->sys_root = sys_root;
    return 0;
}

static int join_path(char *output, size_t output_size, const char *root,
                     const char *suffix) {
    int written = snprintf(output, output_size, "%s%s", root, suffix);
    return written < 0 || (size_t)written >= output_size ? -1 : 0;
}

static char *read_bounded_file(const char *path, size_t limit, size_t *size_out) {
    if (size_out) *size_out = 0U;
    int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) return NULL;
    char *text = malloc(limit + 1U);
    if (!text) {
        int saved = errno;
        close(descriptor);
        errno = saved;
        return NULL;
    }
    size_t used = 0U;
    while (used < limit) {
        ssize_t count = read(descriptor, text + used, limit - used);
        if (count > 0) {
            used += (size_t)count;
            continue;
        }
        if (count == 0) break;
        if (errno == EINTR) continue;
        int saved = errno;
        free(text);
        close(descriptor);
        errno = saved;
        return NULL;
    }
    if (used == limit) {
        char extra;
        ssize_t count;
        do count = read(descriptor, &extra, 1U); while (count < 0 && errno == EINTR);
        if (count > 0) {
            free(text);
            close(descriptor);
            errno = EFBIG;
            return NULL;
        }
        if (count < 0) {
            int saved = errno;
            free(text);
            close(descriptor);
            errno = saved;
            return NULL;
        }
    }
    close(descriptor);
    text[used] = '\0';
    if (size_out) *size_out = used;
    return text;
}

static bool parse_u64_exact(const char *text, uint64_t *value) {
    if (!text || !*text || !value) return false;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; cursor++)
        if (!isdigit(*cursor)) return false;
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno || end == text || *end != '\0') return false;
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_mem_kib(const char *line, const char *key, uint64_t *bytes) {
    size_t key_length = strlen(key);
    if (strncmp(line, key, key_length) != 0 || line[key_length] != ':') return false;
    const char *cursor = line + key_length + 1U;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (!isdigit((unsigned char)*cursor)) return false;
    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(cursor, &end, 10);
    if (errno || end == cursor) return false;
    while (*end == ' ' || *end == '\t') end++;
    if (strcmp(end, "kB") != 0) return false;
    *bytes = multiply_saturating((uint64_t)value, 1024U);
    return true;
}

static int probe_cpu(const mon_roots *roots, mon_host_sample *sample) {
    char path[MON_PATH_MAX];
    if (join_path(path, sizeof(path), roots->proc_root, "/stat") != 0) return -1;
    size_t size = 0U;
    char *text = read_bounded_file(path, MON_STAT_FILE_LIMIT, &size);
    (void)size;
    if (!text) return -1;
    char *newline = strchr(text, '\n');
    if (newline) *newline = '\0';
    char *save = NULL;
    char *token = strtok_r(text, " \t", &save);
    if (!token || strcmp(token, "cpu") != 0) {
        free(text);
        return -1;
    }
    uint64_t values[10] = {0U};
    size_t count = 0U;
    while (count < 10U && (token = strtok_r(NULL, " \t", &save)) != NULL) {
        if (!parse_u64_exact(token, &values[count])) {
            free(text);
            return -1;
        }
        count++;
    }
    if (count < 4U) {
        free(text);
        return -1;
    }
    uint64_t total = 0U;
    for (size_t i = 0U; i < count; i++) total = add_saturating(total, values[i]);
    sample->cpu_total_ticks = total;
    sample->cpu_idle_ticks = add_saturating(values[3], count > 4U ? values[4] : 0U);
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    sample->processor_count = processors > 0 && processors <= 4096L
        ? (unsigned)processors : 1U;
    sample->cpu_available = true;
    free(text);
    return 0;
}

static int probe_memory(const mon_roots *roots, mon_host_sample *sample) {
    char path[MON_PATH_MAX];
    if (join_path(path, sizeof(path), roots->proc_root, "/meminfo") != 0) return -1;
    size_t size = 0U;
    char *text = read_bounded_file(path, MON_STAT_FILE_LIMIT, &size);
    (void)size;
    if (!text) return -1;
    bool total_seen = false;
    bool available_seen = false;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        uint64_t bytes = 0U;
        if (parse_mem_kib(line, "MemTotal", &bytes)) {
            sample->memory_total_bytes = bytes;
            total_seen = true;
        } else if (parse_mem_kib(line, "MemAvailable", &bytes)) {
            sample->memory_available_bytes = bytes;
            available_seen = true;
        }
    }
    free(text);
    if (!total_seen || !available_seen || sample->memory_total_bytes == 0U) return -1;
    if (sample->memory_available_bytes > sample->memory_total_bytes)
        sample->memory_available_bytes = sample->memory_total_bytes;
    sample->memory_available = true;
    return 0;
}

static bool safe_kernel_name(const char *name) {
    if (!name || !*name || strlen(name) > 63U) return false;
    for (const unsigned char *cursor = (const unsigned char *)name; *cursor; cursor++) {
        if (!isalnum(*cursor) && *cursor != '_' && *cursor != '-'
            && *cursor != '.') return false;
    }
    return true;
}

static bool block_device_is_physical(const mon_roots *roots, const char *name) {
    if (!safe_kernel_name(name) || strncmp(name, "loop", 4U) == 0
        || strncmp(name, "ram", 3U) == 0 || strncmp(name, "zram", 4U) == 0)
        return false;
    char suffix[256];
    char path[MON_PATH_MAX];
    int written = snprintf(suffix, sizeof(suffix), "/class/block/%s/partition", name);
    if (written < 0 || (size_t)written >= sizeof(suffix)
        || join_path(path, sizeof(path), roots->sys_root, suffix) != 0) return false;
    struct stat status;
    if (stat(path, &status) == 0) return false;
    written = snprintf(suffix, sizeof(suffix), "/class/block/%s/device", name);
    if (written < 0 || (size_t)written >= sizeof(suffix)
        || join_path(path, sizeof(path), roots->sys_root, suffix) != 0) return false;
    return stat(path, &status) == 0;
}

static void probe_disk(const mon_roots *roots, mon_host_sample *sample) {
    char path[MON_PATH_MAX];
    if (join_path(path, sizeof(path), roots->proc_root, "/diskstats") != 0) return;
    size_t size = 0U;
    char *text = read_bounded_file(path, MON_DISKSTATS_LIMIT, &size);
    (void)size;
    if (!text) return;
    char *line_save = NULL;
    for (char *line = strtok_r(text, "\n", &line_save); line;
         line = strtok_r(NULL, "\n", &line_save)) {
        char *tokens[16] = {0};
        size_t count = 0U;
        char *token_save = NULL;
        for (char *token = strtok_r(line, " \t", &token_save); token && count < 16U;
             token = strtok_r(NULL, " \t", &token_save)) tokens[count++] = token;
        if (count < 10U || !block_device_is_physical(roots, tokens[2])) {
            sample->disk_devices_skipped++;
            continue;
        }
        if (sample->disk_devices >= MON_MAX_BLOCK_DEVICES) {
            sample->disk_truncated = true;
            continue;
        }
        uint64_t read_sectors = 0U;
        uint64_t write_sectors = 0U;
        if (!parse_u64_exact(tokens[5], &read_sectors)
            || !parse_u64_exact(tokens[9], &write_sectors)) {
            sample->disk_devices_skipped++;
            continue;
        }
        sample->disk_read_sectors = add_saturating(sample->disk_read_sectors,
                                                    read_sectors);
        sample->disk_write_sectors = add_saturating(sample->disk_write_sectors,
                                                     write_sectors);
        sample->disk_devices++;
    }
    sample->disk_available = sample->disk_devices > 0U;
    free(text);
}

static void probe_network(const mon_roots *roots, mon_host_sample *sample) {
    char path[MON_PATH_MAX];
    if (join_path(path, sizeof(path), roots->proc_root, "/net/dev") != 0) return;
    size_t size = 0U;
    char *text = read_bounded_file(path, MON_NETDEV_LIMIT, &size);
    (void)size;
    if (!text) return;
    size_t line_number = 0U;
    char *line_save = NULL;
    for (char *line = strtok_r(text, "\n", &line_save); line;
         line = strtok_r(NULL, "\n", &line_save)) {
        line_number++;
        if (line_number <= 2U) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *name = line;
        while (*name == ' ' || *name == '\t') name++;
        char *end = colon - 1;
        while (end >= name && (*end == ' ' || *end == '\t')) *end-- = '\0';
        if (strcmp(name, "lo") == 0) continue;
        if (sample->network_interfaces >= MON_MAX_INTERFACES) {
            sample->network_truncated = true;
            continue;
        }
        char *values[16] = {0};
        size_t count = 0U;
        char *token_save = NULL;
        for (char *token = strtok_r(colon + 1, " \t", &token_save);
             token && count < 16U; token = strtok_r(NULL, " \t", &token_save))
            values[count++] = token;
        uint64_t receive = 0U;
        uint64_t transmit = 0U;
        if (count < 9U || !parse_u64_exact(values[0], &receive)
            || !parse_u64_exact(values[8], &transmit)) continue;
        sample->network_rx_bytes = add_saturating(sample->network_rx_bytes, receive);
        sample->network_tx_bytes = add_saturating(sample->network_tx_bytes, transmit);
        sample->network_interfaces++;
    }
    sample->network_available = sample->network_interfaces > 0U;
    free(text);
}

static bool read_sysfs_u64(const char *path, uint64_t maximum, uint64_t *value) {
    size_t size = 0U;
    char *text = read_bounded_file(path, 128U, &size);
    (void)size;
    if (!text) return false;
    char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    char *end = cursor + strlen(cursor);
    while (end > cursor && isspace((unsigned char)end[-1])) *--end = '\0';
    uint64_t parsed = 0U;
    bool valid = parse_u64_exact(cursor, &parsed) && parsed <= maximum;
    free(text);
    if (!valid) return false;
    *value = parsed;
    return true;
}

static void probe_gpu(const mon_roots *roots, mon_host_sample *sample) {
    for (unsigned card = 0U; card < 16U; card++) {
        const char *busy_names[] = {"gpu_busy_percent", "gt_busy_percent"};
        for (size_t i = 0U; i < sizeof(busy_names) / sizeof(busy_names[0]); i++) {
            char suffix[256];
            char path[MON_PATH_MAX];
            int written = snprintf(suffix, sizeof(suffix),
                                   "/class/drm/card%u/device/%s", card,
                                   busy_names[i]);
            if (written < 0 || (size_t)written >= sizeof(suffix)
                || join_path(path, sizeof(path), roots->sys_root, suffix) != 0) continue;
            uint64_t percent = 0U;
            if (!read_sysfs_u64(path, 100U, &percent)) continue;
            sample->gpu_available = true;
            sample->gpu_card = card;
            sample->gpu_busy_percent_milli = percent * 1000U;

            written = snprintf(suffix, sizeof(suffix),
                               "/class/drm/card%u/device/mem_info_vram_total", card);
            if (written >= 0 && (size_t)written < sizeof(suffix)
                && join_path(path, sizeof(path), roots->sys_root, suffix) == 0) {
                uint64_t total = 0U;
                if (read_sysfs_u64(path, UINT64_MAX, &total)) {
                    written = snprintf(suffix, sizeof(suffix),
                                       "/class/drm/card%u/device/mem_info_vram_used",
                                       card);
                    uint64_t used = 0U;
                    if (written >= 0 && (size_t)written < sizeof(suffix)
                        && join_path(path, sizeof(path), roots->sys_root, suffix) == 0
                        && read_sysfs_u64(path, UINT64_MAX, &used)) {
                        sample->gpu_memory_available = true;
                        sample->gpu_memory_total_bytes = total;
                        sample->gpu_memory_used_bytes = used <= total ? used : total;
                    }
                }
            }
            return;
        }
    }
}

int mon_probe_host(const mon_roots *roots, mon_host_sample *sample,
                   char *error, size_t error_size) {
    if (!roots || !sample) return -1;
    memset(sample, 0, sizeof(*sample));
    if (clock_gettime(CLOCK_MONOTONIC, &sample->observed_at) != 0) {
        if (error && error_size > 0U) snprintf(error, error_size, "monotonic clock failed");
        return -1;
    }
    if (probe_cpu(roots, sample) != 0) {
        if (error && error_size > 0U) snprintf(error, error_size, "cannot read bounded CPU counters");
        return -1;
    }
    if (probe_memory(roots, sample) != 0) {
        if (error && error_size > 0U) snprintf(error, error_size, "cannot read bounded memory counters");
        return -1;
    }
    probe_disk(roots, sample);
    probe_network(roots, sample);
    probe_gpu(roots, sample);
    return 0;
}

static bool numeric_pid(const char *name, int *pid) {
    if (!name || !*name) return false;
    unsigned long value = 0UL;
    for (const unsigned char *cursor = (const unsigned char *)name; *cursor; cursor++) {
        if (!isdigit(*cursor)) return false;
        value = value * 10UL + (unsigned long)(*cursor - '0');
        if (value > 4194304UL) return false;
    }
    if (value == 0UL) return false;
    *pid = (int)value;
    return true;
}

static void sanitize_name(const char *input, size_t input_size,
                          char output[MON_NAME_MAX + 1U]) {
    size_t used = 0U;
    for (size_t i = 0U; i < input_size && input[i] && used < MON_NAME_MAX; i++) {
        unsigned char value = (unsigned char)input[i];
        output[used++] = value >= 0x20U && value <= 0x7eU ? (char)value : '?';
    }
    while (used > 0U && output[used - 1U] == ' ') used--;
    size_t begin = 0U;
    while (begin < used && output[begin] == ' ') begin++;
    if (begin > 0U) {
        memmove(output, output + begin, used - begin);
        used -= begin;
    }
    if (used == 0U) output[used++] = '?';
    output[used] = '\0';
}

static bool parse_process_stat(char *text, mon_process *process) {
    char *open_paren = strchr(text, '(');
    char *close_paren = strrchr(text, ')');
    if (!open_paren || !close_paren || close_paren <= open_paren
        || close_paren[1] != ' ') return false;
    sanitize_name(open_paren + 1, (size_t)(close_paren - open_paren - 1),
                  process->name);
    char *tokens[32] = {0};
    size_t count = 0U;
    char *save = NULL;
    for (char *token = strtok_r(close_paren + 2, " ", &save); token && count < 32U;
         token = strtok_r(NULL, " ", &save)) tokens[count++] = token;
    if (count < 22U || strlen(tokens[0]) != 1U) return false;
    uint64_t user_ticks = 0U, system_ticks = 0U, threads = 0U, start = 0U;
    if (!parse_u64_exact(tokens[11], &user_ticks)
        || !parse_u64_exact(tokens[12], &system_ticks)
        || !parse_u64_exact(tokens[17], &threads)
        || !parse_u64_exact(tokens[19], &start)) return false;
    errno = 0;
    char *end = NULL;
    long long resident_pages = strtoll(tokens[21], &end, 10);
    if (errno || end == tokens[21] || *end || resident_pages < 0) return false;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return false;
    process->state = tokens[0][0];
    process->threads = threads;
    process->cpu_ticks = add_saturating(user_ticks, system_ticks);
    process->start_ticks = start;
    process->resident_bytes = multiply_saturating((uint64_t)resident_pages,
                                                   (uint64_t)page_size);
    return true;
}

static void probe_process_status(const char *text, mon_process *process) {
    char *copy = strdup(text);
    if (!copy) return;
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "Uid:", 4U) != 0) continue;
        char *cursor = line + 4;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        errno = 0;
        char *end = NULL;
        unsigned long value = strtoul(cursor, &end, 10);
        if (!errno && end != cursor && value <= UINT_MAX) {
            process->uid_available = true;
            process->uid = (unsigned)value;
        }
        break;
    }
    free(copy);
}

static bool parse_io_counter(const char *text, const char *key, uint64_t *value) {
    size_t key_length = strlen(key);
    const char *cursor = text;
    while (*cursor) {
        const char *newline = strchr(cursor, '\n');
        size_t length = newline ? (size_t)(newline - cursor) : strlen(cursor);
        if (length > key_length && strncmp(cursor, key, key_length) == 0) {
            const char *number = cursor + key_length;
            while (*number == ' ' || *number == '\t') number++;
            char buffer[64];
            size_t number_length = length - (size_t)(number - cursor);
            if (number_length == 0U || number_length >= sizeof(buffer)) return false;
            memcpy(buffer, number, number_length);
            buffer[number_length] = '\0';
            return parse_u64_exact(buffer, value);
        }
        if (!newline) break;
        cursor = newline + 1;
    }
    return false;
}

static int reserve_process(mon_process_snapshot *snapshot, size_t *capacity) {
    if (snapshot->count < *capacity) return 0;
    if (*capacity >= MON_MAX_PROCESSES) return -1;
    size_t next = *capacity == 0U ? 256U : *capacity * 2U;
    if (next > MON_MAX_PROCESSES) next = MON_MAX_PROCESSES;
    mon_process *rows = realloc(snapshot->rows, next * sizeof(*rows));
    if (!rows) return -1;
    snapshot->rows = rows;
    *capacity = next;
    return 0;
}

int mon_probe_processes(const mon_roots *roots, mon_process_snapshot *snapshot,
                        char *error, size_t error_size) {
    if (!roots || !snapshot) return -1;
    memset(snapshot, 0, sizeof(*snapshot));
    DIR *directory = opendir(roots->proc_root);
    if (!directory) {
        if (error && error_size > 0U)
            snprintf(error, error_size, "cannot open proc root: %s", strerror(errno));
        return -1;
    }
    size_t capacity = 0U;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        snapshot->directories_seen++;
        int pid = 0;
        if (!numeric_pid(entry->d_name, &pid)) continue;
        snapshot->numeric_directories++;
        if (snapshot->count >= MON_MAX_PROCESSES) {
            snapshot->truncated = true;
            continue;
        }
        char path[MON_PATH_MAX];
        int written = snprintf(path, sizeof(path), "%s/%d/stat", roots->proc_root, pid);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            snapshot->malformed++;
            continue;
        }
        errno = 0;
        size_t size = 0U;
        char *stat_text = read_bounded_file(path, 4096U, &size);
        (void)size;
        if (!stat_text) {
            if (errno == EACCES || errno == EPERM) snapshot->permission_denied++;
            else if (errno == ENOENT || errno == ESRCH) snapshot->vanished++;
            else snapshot->malformed++;
            continue;
        }
        mon_process process;
        memset(&process, 0, sizeof(process));
        process.pid = pid;
        if (!parse_process_stat(stat_text, &process)) {
            free(stat_text);
            snapshot->malformed++;
            continue;
        }
        free(stat_text);

        written = snprintf(path, sizeof(path), "%s/%d/status", roots->proc_root, pid);
        if (written >= 0 && (size_t)written < sizeof(path)) {
            char *status_text = read_bounded_file(path, MON_PROCESS_FILE_LIMIT, &size);
            if (status_text) {
                probe_process_status(status_text, &process);
                free(status_text);
            }
        }

        bool cmdline_known = false;
        bool cmdline_empty = false;
        written = snprintf(path, sizeof(path), "%s/%d/cmdline", roots->proc_root, pid);
        if (written >= 0 && (size_t)written < sizeof(path)) {
            char *cmdline = read_bounded_file(path, 4096U, &size);
            if (cmdline) {
                cmdline_known = true;
                cmdline_empty = size == 0U;
                free(cmdline);
            }
        }
        if (cmdline_known && cmdline_empty) process.process_class = MON_CLASS_KERNEL;
        else if (process.uid_available && process.uid == (unsigned)geteuid())
            process.process_class = MON_CLASS_APPLICATION;
        else process.process_class = MON_CLASS_SYSTEM;

        written = snprintf(path, sizeof(path), "%s/%d/io", roots->proc_root, pid);
        if (written >= 0 && (size_t)written < sizeof(path)) {
            errno = 0;
            char *io_text = read_bounded_file(path, MON_PROCESS_FILE_LIMIT, &size);
            if (io_text) {
                uint64_t read_bytes = 0U, write_bytes = 0U;
                if (parse_io_counter(io_text, "read_bytes:", &read_bytes)
                    && parse_io_counter(io_text, "write_bytes:", &write_bytes)) {
                    process.io_available = true;
                    process.read_bytes = read_bytes;
                    process.write_bytes = write_bytes;
                } else snapshot->io_unavailable++;
                free(io_text);
            } else snapshot->io_unavailable++;
        } else snapshot->io_unavailable++;

        if (reserve_process(snapshot, &capacity) != 0) {
            closedir(directory);
            mon_process_snapshot_free(snapshot);
            if (error && error_size > 0U)
                snprintf(error, error_size, "process inventory allocation failed");
            return -1;
        }
        snapshot->rows[snapshot->count++] = process;
    }
    closedir(directory);
    return 0;
}

void mon_process_snapshot_free(mon_process_snapshot *snapshot) {
    if (!snapshot) return;
    free(snapshot->rows);
    memset(snapshot, 0, sizeof(*snapshot));
}
