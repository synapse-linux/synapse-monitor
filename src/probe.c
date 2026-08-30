// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
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

static char resolved_home[MON_PATH_MAX];

static int apply_optional_root(const char *environment_name, const char **target,
                               char *error, size_t error_size) {
    const char *value = getenv(environment_name);
    if (!value) return 0;
    if (!safe_absolute_root(value)) {
        if (error && error_size > 0U)
            snprintf(error, error_size, "invalid absolute normalized test root");
        return -1;
    }
    *target = value;
    return 0;
}

int mon_roots_from_environment(mon_roots *roots, char *error, size_t error_size) {
    if (!roots) return -1;
    memset(roots, 0, sizeof(*roots));
    roots->proc_root = "/proc";
    roots->sys_root = "/sys";
    roots->etc_root = "/etc";
    roots->usr_root = "/usr";
    roots->run_root = "/run";
    roots->home_root = "/";
    struct passwd password;
    struct passwd *result = NULL;
    char buffer[16384];
    if (getpwuid_r(geteuid(), &password, buffer, sizeof(buffer), &result) == 0
        && result && safe_absolute_root(result->pw_dir)
        && snprintf(resolved_home, sizeof(resolved_home), "%s", result->pw_dir) > 0)
        roots->home_root = resolved_home;
    const char *allow = getenv("SYNAPSE_MONITOR_ALLOW_TEST_ROOTS");
    if (!allow || strcmp(allow, "1") != 0) return 0;
    if (apply_optional_root("SYNAPSE_MONITOR_PROC_ROOT", &roots->proc_root,
                            error, error_size) != 0
        || apply_optional_root("SYNAPSE_MONITOR_SYS_ROOT", &roots->sys_root,
                               error, error_size) != 0
        || apply_optional_root("SYNAPSE_MONITOR_ETC_ROOT", &roots->etc_root,
                               error, error_size) != 0
        || apply_optional_root("SYNAPSE_MONITOR_USR_ROOT", &roots->usr_root,
                               error, error_size) != 0
        || apply_optional_root("SYNAPSE_MONITOR_RUN_ROOT", &roots->run_root,
                               error, error_size) != 0
        || apply_optional_root("SYNAPSE_MONITOR_HOME_ROOT", &roots->home_root,
                               error, error_size) != 0) return -1;
    return 0;
}

static int join_path(char *output, size_t output_size, const char *root,
                     const char *suffix) {
    int written = snprintf(output, output_size, "%s%s", root, suffix);
    return written < 0 || (size_t)written >= output_size ? -1 : 0;
}

static char *read_bounded_file(const char *path, size_t limit, size_t *size_out) {
    if (size_out) *size_out = 0U;
    if (!path || limit == 0U || limit > MON_DISKSTATS_LIMIT) {
        errno = EINVAL;
        return NULL;
    }
    int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) return NULL;
    char *text = malloc(limit + 2U);
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
    if (used > limit) {
        free(text);
        errno = EOVERFLOW;
        return NULL;
    }
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

static bool parse_cpu_line(char *line, const char *expected,
                           uint64_t *total, uint64_t *idle) {
    char *save = NULL;
    char *token = strtok_r(line, " \t", &save);
    if (!token || strcmp(token, expected) != 0) return false;
    uint64_t values[10] = {0U};
    size_t count = 0U;
    while (count < 10U && (token = strtok_r(NULL, " \t", &save)) != NULL) {
        if (!parse_u64_exact(token, &values[count])) return false;
        count++;
    }
    if (count < 4U) return false;
    *total = 0U;
    for (size_t i = 0U; i < count; i++)
        *total = add_saturating(*total, values[i]);
    *idle = add_saturating(values[3], count > 4U ? values[4] : 0U);
    return true;
}

static int probe_cpu(const mon_roots *roots, mon_host_sample *sample) {
    char path[MON_PATH_MAX];
    if (join_path(path, sizeof(path), roots->proc_root, "/stat") != 0) return -1;
    size_t size = 0U;
    char *text = read_bounded_file(path, MON_STAT_FILE_LIMIT, &size);
    (void)size;
    if (!text) return -1;
    bool aggregate_seen = false;
    char *line_save = NULL;
    for (char *line = strtok_r(text, "\n", &line_save); line;
         line = strtok_r(NULL, "\n", &line_save)) {
        if (strncmp(line, "cpu", 3U) != 0) continue;
        char label[32];
        size_t length = strcspn(line, " \t");
        if (length == 0U || length >= sizeof(label)) continue;
        memcpy(label, line, length);
        label[length] = '\0';
        char copy[1024];
        if (strlen(line) >= sizeof(copy)) continue;
        (void)snprintf(copy, sizeof(copy), "%s", line);
        uint64_t total = 0U;
        uint64_t idle = 0U;
        if (!parse_cpu_line(copy, label, &total, &idle)) continue;
        if (strcmp(label, "cpu") == 0) {
            sample->cpu_total_ticks = total;
            sample->cpu_idle_ticks = idle;
            aggregate_seen = true;
            continue;
        }
        if (strncmp(label, "cpu", 3U) != 0 || !isdigit((unsigned char)label[3]))
            continue;
        uint64_t index = 0U;
        if (!parse_u64_exact(label + 3, &index)) continue;
        if (index >= MON_MAX_CPUS) {
            sample->cpu_truncated = true;
            continue;
        }
        sample->cpus[index].total_ticks = total;
        sample->cpus[index].idle_ticks = idle;
        if ((size_t)index + 1U > sample->cpu_count)
            sample->cpu_count = (size_t)index + 1U;
    }
    free(text);
    if (!aggregate_seen) return -1;
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    sample->processor_count = sample->cpu_count > 0U
        ? (unsigned)sample->cpu_count
        : (processors > 0 && processors <= (long)MON_MAX_CPUS
           ? (unsigned)processors : 1U);
    sample->cpu_available = true;
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
        size_t index = sample->disk_devices;
        (void)snprintf(sample->disks[index].name,
                       sizeof(sample->disks[index].name), "%s", tokens[2]);
        sample->disks[index].read_sectors = read_sectors;
        sample->disks[index].write_sectors = write_sectors;
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
        if (!safe_kernel_name(name) || strcmp(name, "lo") == 0) continue;
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
        size_t index = sample->network_interfaces;
        (void)snprintf(sample->interfaces[index].name,
                       sizeof(sample->interfaces[index].name), "%s", name);
        sample->interfaces[index].receive_bytes = receive;
        sample->interfaces[index].transmit_bytes = transmit;
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

static bool parse_i64_exact(const char *text, int64_t *value) {
    if (!text || !*text || !value) return false;
    const unsigned char *cursor = (const unsigned char *)text;
    if (*cursor == '-') cursor++;
    if (!isdigit(*cursor)) return false;
    for (; *cursor; cursor++) if (!isdigit(*cursor)) return false;
    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);
    if (errno || end == text || *end != '\0') return false;
    *value = (int64_t)parsed;
    return true;
}

static bool read_sysfs_i64(const char *path, int64_t minimum, int64_t maximum,
                           int64_t *value) {
    size_t size = 0U;
    char *text = read_bounded_file(path, 128U, &size);
    (void)size;
    if (!text) return false;
    char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    char *end = cursor + strlen(cursor);
    while (end > cursor && isspace((unsigned char)end[-1])) *--end = '\0';
    int64_t parsed = 0;
    bool valid = parse_i64_exact(cursor, &parsed)
        && parsed >= minimum && parsed <= maximum;
    free(text);
    if (!valid) {
        errno = EINVAL;
        return false;
    }
    *value = parsed;
    return true;
}

static bool safe_sensor_character(unsigned char value) {
    return isalnum(value) || value == ' ' || value == '_' || value == '-'
        || value == '.' || value == '+' || value == '(' || value == ')'
        || value == '[' || value == ']' || value == ',' || value == ':'
        || value == '/' || value == '#';
}

static bool copy_safe_text(const char *text, char *output, size_t output_size) {
    if (!text || !output || output_size < 2U) return false;
    const char *start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    size_t output_used = 0U;
    for (const char *cursor = start;
         cursor < end && output_used + 1U < output_size; cursor++) {
        unsigned char value = (unsigned char)*cursor;
        output[output_used++] = safe_sensor_character(value) ? (char)value : '?';
    }
    output[output_used] = '\0';
    if (output_used == 0U) {
        errno = EINVAL;
        return false;
    }
    return true;
}

static bool read_safe_text(const char *path, char *output, size_t output_size) {
    size_t size = 0U;
    char *text = read_bounded_file(path, 512U, &size);
    (void)size;
    if (!text) return false;
    bool valid = copy_safe_text(text, output, output_size);
    free(text);
    return valid;
}

static bool read_hex_id(const char *path, char output[8]) {
    char value[32];
    if (!read_safe_text(path, value, sizeof(value))) return false;
    size_t length = strlen(value);
    if (length < 3U || length > 7U || value[0] != '0' || value[1] != 'x') {
        errno = EINVAL;
        return false;
    }
    for (size_t i = 2U; i < length; i++) {
        if (!isxdigit((unsigned char)value[i])) {
            errno = EINVAL;
            return false;
        }
        value[i] = (char)tolower((unsigned char)value[i]);
    }
    memcpy(output, value, length + 1U);
    return true;
}

static bool ascii_contains(const char *text, const char *needle) {
    if (!text || !needle || !*needle) return false;
    size_t length = strlen(needle);
    for (size_t offset = 0U; text[offset]; offset++) {
        size_t i = 0U;
        while (i < length && text[offset + i]
               && tolower((unsigned char)text[offset + i])
                  == tolower((unsigned char)needle[i])) i++;
        if (i == length) return true;
    }
    return false;
}

static int make_sysfs_path(const mon_roots *roots, char *path, size_t path_size,
                           const char *format, unsigned first, unsigned second) {
    char suffix[256];
    int written = snprintf(suffix, sizeof(suffix), format, first, second);
    if (written < 0 || (size_t)written >= sizeof(suffix)) return -1;
    return join_path(path, path_size, roots->sys_root, suffix);
}

static int make_gpu_path(const mon_roots *roots, char *path, size_t path_size,
                         unsigned card, const char *name) {
    char suffix[256];
    int written = snprintf(suffix, sizeof(suffix), "/class/drm/card%u/device/%s",
                           card, name);
    if (written < 0 || (size_t)written >= sizeof(suffix)) return -1;
    return join_path(path, path_size, roots->sys_root, suffix);
}

static bool read_gpu_u64(const mon_roots *roots, unsigned card, const char *name,
                         uint64_t maximum, uint64_t *value) {
    char path[MON_PATH_MAX];
    return make_gpu_path(roots, path, sizeof(path), card, name) == 0
        && read_sysfs_u64(path, maximum, value);
}

static bool read_gpu_hwmon_u64(const mon_roots *roots, unsigned card,
                               unsigned hwmon, const char *name,
                               uint64_t maximum, uint64_t *value) {
    char suffix[256];
    char path[MON_PATH_MAX];
    int written = snprintf(suffix, sizeof(suffix),
                           "/class/drm/card%u/device/hwmon/hwmon%u/%s",
                           card, hwmon, name);
    return written >= 0 && (size_t)written < sizeof(suffix)
        && join_path(path, sizeof(path), roots->sys_root, suffix) == 0
        && read_sysfs_u64(path, maximum, value);
}

static bool read_gpu_hwmon_i64(const mon_roots *roots, unsigned card,
                               unsigned hwmon, const char *name,
                               int64_t minimum, int64_t maximum, int64_t *value) {
    char suffix[256];
    char path[MON_PATH_MAX];
    int written = snprintf(suffix, sizeof(suffix),
                           "/class/drm/card%u/device/hwmon/hwmon%u/%s",
                           card, hwmon, name);
    return written >= 0 && (size_t)written < sizeof(suffix)
        && join_path(path, sizeof(path), roots->sys_root, suffix) == 0
        && read_sysfs_i64(path, minimum, maximum, value);
}

static void probe_gpu_driver(const mon_roots *roots, mon_gpu *gpu) {
    char path[MON_PATH_MAX];
    if (make_gpu_path(roots, path, sizeof(path), gpu->card, "uevent") != 0) return;
    size_t size = 0U;
    char *text = read_bounded_file(path, 4096U, &size);
    (void)size;
    if (!text) return;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "DRIVER=", 7U) != 0 || !safe_kernel_name(line + 7U))
            continue;
        (void)snprintf(gpu->driver, sizeof(gpu->driver), "%s", line + 7U);
        break;
    }
    free(text);
}

static int compare_unsigned(const void *left_value, const void *right_value) {
    unsigned left = *(const unsigned *)left_value;
    unsigned right = *(const unsigned *)right_value;
    return left < right ? -1 : left > right ? 1 : 0;
}

static size_t gpu_hwmon_indices(const mon_roots *roots, unsigned card,
                                unsigned indices[64]) {
    char suffix[128];
    char path[MON_PATH_MAX];
    int written = snprintf(suffix, sizeof(suffix),
                           "/class/drm/card%u/device/hwmon", card);
    if (written < 0 || (size_t)written >= sizeof(suffix)
        || join_path(path, sizeof(path), roots->sys_root, suffix) != 0) return 0U;
    DIR *directory = opendir(path);
    if (!directory) return 0U;
    size_t count = 0U;
    errno = 0;
    for (struct dirent *entry = readdir(directory); entry;
         entry = readdir(directory)) {
        if (strncmp(entry->d_name, "hwmon", 5U) != 0) continue;
        uint64_t index = 0U;
        if (!parse_u64_exact(entry->d_name + 5U, &index)
            || index >= MON_MAX_HWMON_DEVICES || count >= 64U) continue;
        bool duplicate = false;
        for (size_t i = 0U; i < count; i++)
            if (indices[i] == (unsigned)index) duplicate = true;
        if (!duplicate) indices[count++] = (unsigned)index;
    }
    (void)closedir(directory);
    if (count > 1U) qsort(indices, count, sizeof(indices[0]), compare_unsigned);
    return count;
}

static void probe_gpu_hwmon(const mon_roots *roots, mon_gpu *gpu) {
    unsigned indices[64];
    size_t count = gpu_hwmon_indices(roots, gpu->card, indices);
    for (size_t index = 0U; index < count; index++) {
        unsigned hwmon = indices[index];
        char path[MON_PATH_MAX];
        if (make_sysfs_path(roots, path, sizeof(path),
                            "/class/drm/card%u/device/hwmon/hwmon%u/name",
                            gpu->card, hwmon) != 0) continue;
        char name[MON_NAME_MAX + 1U];
        if (!read_safe_text(path, name, sizeof(name))) continue;
        for (unsigned channel = 1U; channel <= MON_MAX_SENSOR_CHANNELS; channel++) {
            char attribute[64];
            int written = snprintf(attribute, sizeof(attribute), "temp%u_input",
                                   channel);
            int64_t temperature = 0;
            if (written < 0 || (size_t)written >= sizeof(attribute)
                || !read_gpu_hwmon_i64(roots, gpu->card, hwmon, attribute,
                                       -100000, 250000, &temperature)) continue;
            if (!gpu->temperature_available
                || temperature > gpu->temperature_millidegrees_celsius) {
                gpu->temperature_available = true;
                gpu->temperature_millidegrees_celsius = temperature;
                char suffix[256];
                written = snprintf(suffix, sizeof(suffix),
                    "/class/drm/card%u/device/hwmon/hwmon%u/temp%u_label",
                    gpu->card, hwmon, channel);
                if (written < 0 || (size_t)written >= sizeof(suffix)
                    || join_path(path, sizeof(path), roots->sys_root, suffix) != 0
                    || !read_safe_text(path, gpu->temperature_label,
                                       sizeof(gpu->temperature_label)))
                    (void)snprintf(gpu->temperature_label,
                                   sizeof(gpu->temperature_label), "temperature");
            }
        }
        uint64_t value = 0U;
        if (!gpu->core_clock_available
            && read_gpu_hwmon_u64(roots, gpu->card, hwmon, "freq1_input",
                                  1000000000000000ULL, &value)) {
            gpu->core_clock_available = true;
            gpu->core_clock_hz = value;
        }
        if (!gpu->memory_clock_available
            && read_gpu_hwmon_u64(roots, gpu->card, hwmon, "freq2_input",
                                  1000000000000000ULL, &value)) {
            gpu->memory_clock_available = true;
            gpu->memory_clock_hz = value;
        }
        if (!gpu->power_available
            && (read_gpu_hwmon_u64(roots, gpu->card, hwmon, "power1_average",
                                   UINT64_MAX, &value)
                || read_gpu_hwmon_u64(roots, gpu->card, hwmon, "power1_input",
                                      UINT64_MAX, &value))) {
            gpu->power_available = true;
            gpu->power_microwatts = value;
        }
        if (!gpu->power_cap_available
            && read_gpu_hwmon_u64(roots, gpu->card, hwmon, "power1_cap",
                                  UINT64_MAX, &value)) {
            gpu->power_cap_available = true;
            gpu->power_cap_microwatts = value;
        }
        if (!gpu->fan_available
            && read_gpu_hwmon_u64(roots, gpu->card, hwmon, "fan1_input",
                                  1000000U, &value)) {
            gpu->fan_available = true;
            gpu->fan_rpm = value;
        }
    }
}

static void probe_one_gpu(const mon_roots *roots, mon_gpu *gpu) {
    char path[MON_PATH_MAX];
    if (make_gpu_path(roots, path, sizeof(path), gpu->card, "vendor") == 0)
        (void)read_hex_id(path, gpu->vendor_id);
    if (make_gpu_path(roots, path, sizeof(path), gpu->card, "device") == 0)
        (void)read_hex_id(path, gpu->device_id);
    probe_gpu_driver(roots, gpu);
    const char *busy_names[] = {"gpu_busy_percent", "gt_busy_percent"};
    for (size_t i = 0U; i < sizeof(busy_names) / sizeof(busy_names[0]); i++) {
        uint64_t percent = 0U;
        if (!read_gpu_u64(roots, gpu->card, busy_names[i], 100U, &percent))
            continue;
        gpu->utilization_available = true;
        gpu->utilization_percent_milli = percent * 1000U;
        break;
    }
    uint64_t total = 0U;
    uint64_t used = 0U;
    if (read_gpu_u64(roots, gpu->card, "mem_info_vram_total", UINT64_MAX, &total)
        && read_gpu_u64(roots, gpu->card, "mem_info_vram_used", UINT64_MAX, &used)
        && total > 0U) {
        gpu->memory_available = true;
        gpu->memory_total_bytes = total;
        gpu->memory_used_bytes = used <= total ? used : total;
    }
    uint64_t megahertz = 0U;
    char nested_current[64];
    char nested_maximum[64];
    (void)snprintf(nested_current, sizeof(nested_current),
                   "drm/card%u/gt_cur_freq_mhz", gpu->card);
    (void)snprintf(nested_maximum, sizeof(nested_maximum),
                   "drm/card%u/gt_max_freq_mhz", gpu->card);
    const char *clock_names[] = {
        "gt_cur_freq_mhz", "rps_cur_freq_mhz", "gt/gt0/rps_cur_freq_mhz",
        nested_current
    };
    for (size_t i = 0U; i < sizeof(clock_names) / sizeof(clock_names[0]); i++) {
        if (!read_gpu_u64(roots, gpu->card, clock_names[i], 1000000U,
                          &megahertz)) continue;
        gpu->core_clock_available = true;
        gpu->core_clock_hz = multiply_saturating(megahertz, 1000000U);
        break;
    }
    const char *maximum_names[] = {
        "gt_max_freq_mhz", "gt/gt0/rps_max_freq_mhz", nested_maximum
    };
    for (size_t i = 0U; i < sizeof(maximum_names) / sizeof(maximum_names[0]); i++) {
        if (!read_gpu_u64(roots, gpu->card, maximum_names[i], 1000000U,
                          &megahertz)) continue;
        gpu->core_clock_max_available = true;
        gpu->core_clock_max_hz = multiply_saturating(megahertz, 1000000U);
        break;
    }
    probe_gpu_hwmon(roots, gpu);
}

static bool hex_identifier_line(const char *text) {
    if (!text || strlen(text) < 4U) return false;
    for (size_t i = 0U; i < 4U; i++)
        if (!isxdigit((unsigned char)text[i])) return false;
    return true;
}

static bool same_hex_identifier(const char *line, const char *sysfs_id) {
    if (!line || !sysfs_id || strlen(sysfs_id) != 6U) return false;
    for (size_t i = 0U; i < 4U; i++)
        if (tolower((unsigned char)line[i])
            != tolower((unsigned char)sysfs_id[i + 2U])) return false;
    return true;
}

static bool gpu_models_complete(const mon_host_sample *sample) {
    for (size_t i = 0U; i < sample->gpu_count; i++)
        if (sample->gpus[i].model[0] == '\0') return false;
    return true;
}

static void process_gpu_model_line(char *line, size_t length,
                                   char current_vendor[5],
                                   mon_host_sample *sample) {
    line[length] = '\0';
    if (length >= 6U && line[0] != '\t' && hex_identifier_line(line)
        && line[4] == ' ' && line[5] == ' ') {
        for (size_t i = 0U; i < 4U; i++)
            current_vendor[i] = (char)tolower((unsigned char)line[i]);
        current_vendor[4] = '\0';
        return;
    }
    if (length < 6U || line[0] != '\t' || line[1] == '\t'
        || !hex_identifier_line(line + 1U) || line[5] != ' '
        || current_vendor[0] == '\0') return;
    const char *description = line + 5U;
    while (*description == ' ' || *description == '\t') description++;
    for (size_t i = 0U; i < sample->gpu_count; i++) {
        mon_gpu *gpu = &sample->gpus[i];
        if (gpu->model[0] != '\0'
            || !same_hex_identifier(current_vendor, gpu->vendor_id)
            || !same_hex_identifier(line + 1U, gpu->device_id)) continue;
        (void)copy_safe_text(description, gpu->model, sizeof(gpu->model));
    }
}

static void probe_gpu_models_file(const char *path, mon_host_sample *sample) {
    int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) return;
    char current_vendor[5] = "";
    char line[512];
    char input[4096];
    size_t line_used = 0U;
    size_t observed = 0U;
    bool overlong = false;
    while (observed < MON_PCI_IDS_LIMIT && !gpu_models_complete(sample)) {
        size_t remaining = MON_PCI_IDS_LIMIT - observed;
        size_t requested = remaining < sizeof(input) ? remaining : sizeof(input);
        ssize_t count;
        do count = read(descriptor, input, requested);
        while (count < 0 && errno == EINTR);
        if (count <= 0) break;
        observed += (size_t)count;
        for (ssize_t i = 0; i < count; i++) {
            if (input[i] == '\n') {
                if (!overlong)
                    process_gpu_model_line(line, line_used, current_vendor, sample);
                line_used = 0U;
                overlong = false;
                if (gpu_models_complete(sample)) break;
            } else if (!overlong) {
                if (line_used + 1U < sizeof(line)) line[line_used++] = input[i];
                else overlong = true;
            }
        }
    }
    if (line_used > 0U && !overlong && !gpu_models_complete(sample))
        process_gpu_model_line(line, line_used, current_vendor, sample);
    (void)close(descriptor);
}

typedef struct {
    char vendor_id[8];
    char device_id[8];
    char model[MON_LABEL_MAX + 1U];
} mon_gpu_model_cache_entry;

static char gpu_model_cache_root[MON_PATH_MAX];
static mon_gpu_model_cache_entry gpu_model_cache[MON_MAX_GPUS];
static size_t gpu_model_cache_count;

static const mon_gpu_model_cache_entry *find_gpu_model_cache(const mon_gpu *gpu) {
    for (size_t i = 0U; i < gpu_model_cache_count; i++)
        if (strcmp(gpu_model_cache[i].vendor_id, gpu->vendor_id) == 0
            && strcmp(gpu_model_cache[i].device_id, gpu->device_id) == 0)
            return &gpu_model_cache[i];
    return NULL;
}

static bool apply_gpu_model_cache(mon_host_sample *sample) {
    for (size_t i = 0U; i < sample->gpu_count; i++) {
        const mon_gpu_model_cache_entry *entry =
            find_gpu_model_cache(&sample->gpus[i]);
        if (!entry) return false;
        (void)snprintf(sample->gpus[i].model, sizeof(sample->gpus[i].model),
                       "%s", entry->model);
    }
    return true;
}

static void store_gpu_model_cache(const mon_host_sample *sample) {
    for (size_t i = 0U; i < sample->gpu_count
         && gpu_model_cache_count < MON_MAX_GPUS; i++) {
        if (find_gpu_model_cache(&sample->gpus[i])) continue;
        mon_gpu_model_cache_entry *entry =
            &gpu_model_cache[gpu_model_cache_count++];
        (void)snprintf(entry->vendor_id, sizeof(entry->vendor_id), "%s",
                       sample->gpus[i].vendor_id);
        (void)snprintf(entry->device_id, sizeof(entry->device_id), "%s",
                       sample->gpus[i].device_id);
        (void)snprintf(entry->model, sizeof(entry->model), "%s",
                       sample->gpus[i].model);
    }
}

static void probe_gpu_models(const mon_roots *roots, mon_host_sample *sample) {
    if (strcmp(gpu_model_cache_root, roots->usr_root) != 0) {
        memset(gpu_model_cache, 0, sizeof(gpu_model_cache));
        gpu_model_cache_count = 0U;
        (void)snprintf(gpu_model_cache_root, sizeof(gpu_model_cache_root), "%s",
                       roots->usr_root);
    }
    if (apply_gpu_model_cache(sample)) return;
    static const char *suffixes[] = {
        "/share/hwdata/pci.ids", "/share/misc/pci.ids"
    };
    for (size_t i = 0U; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        char path[MON_PATH_MAX];
        if (join_path(path, sizeof(path), roots->usr_root, suffixes[i]) != 0)
            continue;
        probe_gpu_models_file(path, sample);
        if (gpu_models_complete(sample)) break;
    }
    store_gpu_model_cache(sample);
}

static void probe_gpu(const mon_roots *roots, mon_host_sample *sample) {
    for (unsigned card = 0U; card < 32U; card++) {
        char suffix[128];
        char path[MON_PATH_MAX];
        int written = snprintf(suffix, sizeof(suffix),
                               "/class/drm/card%u/device", card);
        if (written < 0 || (size_t)written >= sizeof(suffix)
            || join_path(path, sizeof(path), roots->sys_root, suffix) != 0)
            continue;
        struct stat status;
        if (stat(path, &status) != 0 || !S_ISDIR(status.st_mode)) continue;
        if (sample->gpu_count >= MON_MAX_GPUS) {
            sample->gpu_truncated = true;
            continue;
        }
        mon_gpu *gpu = &sample->gpus[sample->gpu_count++];
        memset(gpu, 0, sizeof(*gpu));
        gpu->card = card;
        probe_one_gpu(roots, gpu);
    }
    probe_gpu_models(roots, sample);
    char overflow_suffix[128];
    char overflow_path[MON_PATH_MAX];
    int overflow_written = snprintf(overflow_suffix, sizeof(overflow_suffix),
                                    "/class/drm/card32/device");
    struct stat overflow_status;
    if (overflow_written >= 0
        && (size_t)overflow_written < sizeof(overflow_suffix)
        && join_path(overflow_path, sizeof(overflow_path), roots->sys_root,
                     overflow_suffix) == 0
        && stat(overflow_path, &overflow_status) == 0)
        sample->gpu_truncated = true;
    sample->gpu_present = sample->gpu_count > 0U;
    if (!sample->gpu_present) return;
    size_t summary = 0U;
    for (size_t i = 0U; i < sample->gpu_count; i++) {
        if (sample->gpus[i].utilization_available) {
            summary = i;
            break;
        }
    }
    const mon_gpu *gpu = &sample->gpus[summary];
    sample->gpu_card = gpu->card;
    sample->gpu_available = gpu->utilization_available;
    sample->gpu_busy_percent_milli = gpu->utilization_percent_milli;
    sample->gpu_memory_available = gpu->memory_available;
    sample->gpu_memory_used_bytes = gpu->memory_used_bytes;
    sample->gpu_memory_total_bytes = gpu->memory_total_bytes;
}

static mon_thermal_class classify_temperature(const char *source,
                                               const char *label) {
    if (ascii_contains(source, "amdgpu") || ascii_contains(source, "i915")
        || ascii_contains(source, "nouveau") || ascii_contains(source, "nvidia"))
        return MON_THERMAL_GPU;
    if (ascii_contains(source, "nvme") || ascii_contains(source, "drivetemp"))
        return MON_THERMAL_STORAGE;
    if (ascii_contains(source, "bat") || ascii_contains(label, "battery"))
        return MON_THERMAL_BATTERY;
    if (ascii_contains(source, "coretemp") || ascii_contains(source, "k10temp")
        || ascii_contains(source, "zenpower") || ascii_contains(source, "cpu")
        || ascii_contains(source, "x86_pkg_temp")) {
        if (ascii_contains(label, "core")) return MON_THERMAL_CPU_CORE;
        return MON_THERMAL_CPU_PACKAGE;
    }
    if (ascii_contains(label, "package")) return MON_THERMAL_CPU_PACKAGE;
    if (ascii_contains(label, "core")) return MON_THERMAL_CPU_CORE;
    return MON_THERMAL_SYSTEM;
}

static bool same_temperature_observation(const mon_host_sample *sample,
                                         mon_thermal_class sensor_class,
                                         int64_t value) {
    if (sensor_class == MON_THERMAL_OTHER) return false;
    for (size_t i = 0U; i < sample->temperature_count; i++) {
        if (sample->temperatures[i].sensor_class != sensor_class) continue;
        if (sensor_class == MON_THERMAL_CPU_PACKAGE
            || sensor_class == MON_THERMAL_GPU
            || sensor_class == MON_THERMAL_BATTERY
            || sample->temperatures[i].temperature_millidegrees_celsius == value)
            return true;
    }
    return false;
}

static void add_temperature(mon_host_sample *sample, const char *source,
                            const char *label, int64_t value,
                            bool maximum_available, int64_t maximum,
                            bool critical_available, int64_t critical) {
    mon_thermal_class sensor_class = classify_temperature(source, label);
    if (sample->temperature_count >= MON_MAX_TEMPERATURES) {
        sample->temperature_truncated = true;
        return;
    }
    mon_temperature *temperature = &sample->temperatures[sample->temperature_count++];
    memset(temperature, 0, sizeof(*temperature));
    temperature->sensor_class = sensor_class;
    (void)snprintf(temperature->source, sizeof(temperature->source), "%s", source);
    (void)snprintf(temperature->label, sizeof(temperature->label), "%s", label);
    temperature->temperature_millidegrees_celsius = value;
    temperature->maximum_available = maximum_available;
    temperature->maximum_millidegrees_celsius = maximum;
    temperature->critical_available = critical_available;
    temperature->critical_millidegrees_celsius = critical;
}

static void add_fan(mon_host_sample *sample, const char *source,
                    const char *label, uint64_t rpm) {
    if (sample->fan_count >= MON_MAX_FANS) {
        sample->fan_truncated = true;
        return;
    }
    mon_fan *fan = &sample->fans[sample->fan_count++];
    (void)snprintf(fan->source, sizeof(fan->source), "%s", source);
    (void)snprintf(fan->label, sizeof(fan->label), "%s", label);
    fan->rpm = rpm;
}

static void probe_hwmon_temperatures(const mon_roots *roots,
                                     mon_host_sample *sample) {
    for (unsigned hwmon = 0U; hwmon < MON_MAX_HWMON_DEVICES; hwmon++) {
        char path[MON_PATH_MAX];
        if (make_sysfs_path(roots, path, sizeof(path),
                            "/class/hwmon/hwmon%u/name", hwmon, 0U) != 0)
            continue;
        char source[MON_NAME_MAX + 1U];
        if (!read_safe_text(path, source, sizeof(source))) continue;
        sample->hwmon_devices_seen++;
        for (unsigned channel = 1U; channel <= MON_MAX_SENSOR_CHANNELS; channel++) {
            char suffix[256];
            int written = snprintf(suffix, sizeof(suffix),
                                   "/class/hwmon/hwmon%u/temp%u_input",
                                   hwmon, channel);
            if (written < 0 || (size_t)written >= sizeof(suffix)
                || join_path(path, sizeof(path), roots->sys_root, suffix) != 0)
                continue;
            int64_t value = 0;
            errno = 0;
            if (!read_sysfs_i64(path, -100000, 250000, &value)) {
                if (errno == EACCES || errno == EPERM)
                    sample->sensor_permission_denied++;
                else if (errno != ENOENT && errno != ENOTDIR)
                    sample->sensor_malformed++;
                continue;
            }
            char label[MON_SENSOR_LABEL_MAX + 1U];
            written = snprintf(suffix, sizeof(suffix),
                               "/class/hwmon/hwmon%u/temp%u_label",
                               hwmon, channel);
            if (written < 0 || (size_t)written >= sizeof(suffix)
                || join_path(path, sizeof(path), roots->sys_root, suffix) != 0
                || !read_safe_text(path, label, sizeof(label)))
                (void)snprintf(label, sizeof(label), "temperature %u", channel);
            int64_t maximum = 0;
            int64_t critical = 0;
            written = snprintf(suffix, sizeof(suffix),
                               "/class/hwmon/hwmon%u/temp%u_max", hwmon, channel);
            bool maximum_available = written >= 0
                && (size_t)written < sizeof(suffix)
                && join_path(path, sizeof(path), roots->sys_root, suffix) == 0
                && read_sysfs_i64(path, -100000, 250000, &maximum);
            written = snprintf(suffix, sizeof(suffix),
                               "/class/hwmon/hwmon%u/temp%u_crit", hwmon, channel);
            bool critical_available = written >= 0
                && (size_t)written < sizeof(suffix)
                && join_path(path, sizeof(path), roots->sys_root, suffix) == 0
                && read_sysfs_i64(path, -100000, 250000, &critical);
            add_temperature(sample, source, label, value, maximum_available,
                            maximum, critical_available, critical);
        }
        char overflow_suffix[256];
        int overflow_written = snprintf(overflow_suffix, sizeof(overflow_suffix),
                                        "/class/hwmon/hwmon%u/temp33_input",
                                        hwmon);
        struct stat overflow_status;
        if (overflow_written >= 0
            && (size_t)overflow_written < sizeof(overflow_suffix)
            && join_path(path, sizeof(path), roots->sys_root, overflow_suffix) == 0
            && stat(path, &overflow_status) == 0)
            sample->temperature_truncated = true;
        for (unsigned channel = 1U; channel <= MON_MAX_SENSOR_CHANNELS; channel++) {
            char suffix[256];
            int written = snprintf(suffix, sizeof(suffix),
                                   "/class/hwmon/hwmon%u/fan%u_input",
                                   hwmon, channel);
            if (written < 0 || (size_t)written >= sizeof(suffix)
                || join_path(path, sizeof(path), roots->sys_root, suffix) != 0)
                continue;
            uint64_t rpm = 0U;
            if (!read_sysfs_u64(path, 1000000U, &rpm)) continue;
            char label[MON_SENSOR_LABEL_MAX + 1U];
            written = snprintf(suffix, sizeof(suffix),
                               "/class/hwmon/hwmon%u/fan%u_label",
                               hwmon, channel);
            if (written < 0 || (size_t)written >= sizeof(suffix)
                || join_path(path, sizeof(path), roots->sys_root, suffix) != 0
                || !read_safe_text(path, label, sizeof(label)))
                (void)snprintf(label, sizeof(label), "fan %u", channel);
            add_fan(sample, source, label, rpm);
        }
        overflow_written = snprintf(overflow_suffix, sizeof(overflow_suffix),
                                    "/class/hwmon/hwmon%u/fan33_input", hwmon);
        if (overflow_written >= 0
            && (size_t)overflow_written < sizeof(overflow_suffix)
            && join_path(path, sizeof(path), roots->sys_root, overflow_suffix) == 0
            && stat(path, &overflow_status) == 0)
            sample->fan_truncated = true;
    }
    char overflow_path[MON_PATH_MAX];
    if (join_path(overflow_path, sizeof(overflow_path), roots->sys_root,
                  "/class/hwmon/hwmon256/name") == 0) {
        struct stat overflow_status;
        if (stat(overflow_path, &overflow_status) == 0) {
            sample->temperature_truncated = true;
            sample->fan_truncated = true;
        }
    }
}

static void probe_thermal_zones(const mon_roots *roots,
                                mon_host_sample *sample) {
    for (unsigned zone = 0U; zone < MON_MAX_TEMPERATURES; zone++) {
        char path[MON_PATH_MAX];
        if (make_sysfs_path(roots, path, sizeof(path),
                            "/class/thermal/thermal_zone%u/type", zone, 0U) != 0)
            continue;
        char type[MON_SENSOR_LABEL_MAX + 1U];
        if (!read_safe_text(path, type, sizeof(type))) continue;
        if (make_sysfs_path(roots, path, sizeof(path),
                            "/class/thermal/thermal_zone%u/temp", zone, 0U) != 0)
            continue;
        int64_t value = 0;
        if (!read_sysfs_i64(path, -100000, 250000, &value)) continue;
        mon_thermal_class sensor_class = classify_temperature(type, type);
        if (same_temperature_observation(sample, sensor_class, value)) continue;
        add_temperature(sample, "thermal-zone", type, value, false, 0,
                        false, 0);
    }
    char overflow_path[MON_PATH_MAX];
    if (join_path(overflow_path, sizeof(overflow_path), roots->sys_root,
                  "/class/thermal/thermal_zone256/type") == 0) {
        struct stat overflow_status;
        if (stat(overflow_path, &overflow_status) == 0)
            sample->temperature_truncated = true;
    }
}

static int compare_temperature(const void *left_value, const void *right_value) {
    const mon_temperature *left = left_value;
    const mon_temperature *right = right_value;
    if (left->sensor_class < right->sensor_class) return -1;
    if (left->sensor_class > right->sensor_class) return 1;
    int compared = strcmp(left->source, right->source);
    return compared != 0 ? compared : strcmp(left->label, right->label);
}

static int compare_fan(const void *left_value, const void *right_value) {
    const mon_fan *left = left_value;
    const mon_fan *right = right_value;
    int compared = strcmp(left->source, right->source);
    return compared != 0 ? compared : strcmp(left->label, right->label);
}

static void probe_thermals(const mon_roots *roots, mon_host_sample *sample) {
    probe_hwmon_temperatures(roots, sample);
    probe_thermal_zones(roots, sample);
    if (sample->temperature_count > 1U)
        qsort(sample->temperatures, sample->temperature_count,
              sizeof(sample->temperatures[0]), compare_temperature);
    if (sample->fan_count > 1U)
        qsort(sample->fans, sample->fan_count, sizeof(sample->fans[0]), compare_fan);
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
    probe_thermals(roots, sample);
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
