// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int join_root(char *output, size_t output_size, const char *root,
                     const char *suffix) {
    int written = snprintf(output, output_size, "%s%s", root, suffix);
    return written < 0 || (size_t)written >= output_size ? -1 : 0;
}

static char *read_file(const char *path, size_t limit, size_t *size_out) {
    if (size_out) *size_out = 0U;
    if (!path || limit == 0U || limit > MON_MAPS_FILE_LIMIT) {
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
        if (count > 0) used += (size_t)count;
        else if (count == 0) break;
        else if (errno != EINTR) {
            int saved = errno;
            free(text);
            close(descriptor);
            errno = saved;
            return NULL;
        }
    }
    if (used == limit) {
        char extra;
        ssize_t count;
        do count = read(descriptor, &extra, 1U); while (count < 0 && errno == EINTR);
        if (count != 0) {
            int saved = count > 0 ? EFBIG : errno;
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

static bool safe_name(const char *name, size_t maximum) {
    if (!name || !*name || strlen(name) > maximum) return false;
    for (const unsigned char *cursor = (const unsigned char *)name; *cursor; cursor++) {
        if (!isalnum(*cursor) && *cursor != '_' && *cursor != '-'
            && *cursor != '.' && *cursor != '@' && *cursor != ':') return false;
    }
    return true;
}

static bool has_suffix(const char *value, const char *suffix) {
    size_t value_length = strlen(value);
    size_t suffix_length = strlen(suffix);
    return value_length >= suffix_length
        && strcmp(value + value_length - suffix_length, suffix) == 0;
}

static void clean_text(const char *input, char *output, size_t output_size) {
    if (!output || output_size == 0U) return;
    size_t used = 0U;
    if (input) {
        for (size_t i = 0U; input[i] && used + 1U < output_size; i++) {
            unsigned char value = (unsigned char)input[i];
            if (value == '\t') value = ' ';
            if (value < 0x20U || value > 0x7eU) continue;
            if (value == ' ' && (used == 0U || output[used - 1U] == ' ')) continue;
            output[used++] = (char)value;
        }
    }
    while (used > 0U && output[used - 1U] == ' ') used--;
    output[used] = '\0';
}

static bool copy_bounded(char *output, size_t output_size, const char *value) {
    if (!output || output_size == 0U || !value) return false;
    size_t length = strlen(value);
    if (length >= output_size) return false;
    memcpy(output, value, length + 1U);
    return true;
}

static void executable_label(const char *value, char *output,
                             size_t output_size, bool mark_path) {
    if (!output || output_size == 0U) return;
    output[0] = '\0';
    if (!value) return;
    while (*value == ' ' || *value == '\t') value++;
    while (*value == '-' || *value == '+' || *value == '!' || *value == ':'
           || *value == '@') value++;
    char token[MON_PATH_MAX];
    size_t used = 0U;
    bool quoted = false;
    char quote = '\0';
    const char *cursor = value;
    while (*cursor && used + 1U < sizeof(token)) {
        if (!quoted && (*cursor == '\'' || *cursor == '"')) {
            quoted = true;
            quote = *cursor++;
            continue;
        }
        if (quoted && *cursor == quote) {
            quoted = false;
            cursor++;
            continue;
        }
        if (!quoted && (*cursor == ' ' || *cursor == '\t')) break;
        if (*cursor == '\\' && cursor[1]) cursor++;
        token[used++] = *cursor++;
    }
    token[used] = '\0';
    const char *base = strrchr(token, '/');
    base = base ? base + 1U : token;
    char clean[MON_NAME_MAX + 1U];
    clean_text(base, clean, sizeof(clean));
    if (clean[0] == '\0') return;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (mark_path)
        (void)snprintf(output, output_size, "%s (path redacted)", clean);
    else if (*cursor)
        (void)snprintf(output, output_size, "%s (arguments redacted)", clean);
    else (void)snprintf(output, output_size, "%s", clean);
}

static bool parse_u64(const char *text, int base, uint64_t *value) {
    if (!text || !*text || !value || (base != 10 && base != 16)) return false;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; cursor++) {
        if ((base == 10 && !isdigit(*cursor))
            || (base == 16 && !isxdigit(*cursor))) return false;
    }
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, base);
    if (errno || end == text || *end) return false;
    *value = (uint64_t)parsed;
    return true;
}

static bool contains_ascii_casefold(const char *text, const char *needle) {
    if (!needle || !*needle) return true;
    if (!text) return false;
    size_t needle_length = strlen(needle);
    for (size_t offset = 0U; text[offset]; offset++) {
        size_t index = 0U;
        while (index < needle_length && text[offset + index]
               && tolower((unsigned char)text[offset + index])
                  == tolower((unsigned char)needle[index])) index++;
        if (index == needle_length) return true;
    }
    return false;
}

static int reserve_service(mon_service_snapshot *snapshot, size_t *capacity) {
    if (snapshot->count < *capacity) return 0;
    if (*capacity >= MON_MAX_SERVICES) return -1;
    size_t next = *capacity == 0U ? 256U : *capacity * 2U;
    if (next > MON_MAX_SERVICES) next = MON_MAX_SERVICES;
    mon_service *rows = realloc(snapshot->rows, next * sizeof(*rows));
    if (!rows) return -1;
    snapshot->rows = rows;
    *capacity = next;
    return 0;
}

static mon_service *find_service_linear(mon_service_snapshot *snapshot,
                                        const char *name) {
    for (size_t i = 0U; i < snapshot->count; i++)
        if (strcmp(snapshot->rows[i].name, name) == 0) return &snapshot->rows[i];
    return NULL;
}

static void parse_service_file(char *text, mon_service *service) {
    bool installable = false;
    enum { SECTION_NONE, SECTION_UNIT, SECTION_SERVICE, SECTION_INSTALL } section
        = SECTION_NONE;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '#' || *line == ';' || *line == '\0') continue;
        size_t length = strlen(line);
        while (length > 0U && (line[length - 1U] == '\r'
                               || line[length - 1U] == ' '
                               || line[length - 1U] == '\t'))
            line[--length] = '\0';
        if (line[0] == '[' && length > 2U && line[length - 1U] == ']') {
            line[length - 1U] = '\0';
            if (strcmp(line + 1, "Unit") == 0) section = SECTION_UNIT;
            else if (strcmp(line + 1, "Service") == 0) section = SECTION_SERVICE;
            else if (strcmp(line + 1, "Install") == 0) section = SECTION_INSTALL;
            else section = SECTION_NONE;
            continue;
        }
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        const char *value = equals + 1;
        if (section == SECTION_UNIT && strcmp(line, "Description") == 0
            && service->description[0] == '\0')
            clean_text(value, service->description, sizeof(service->description));
        else if (section == SECTION_SERVICE && strcmp(line, "User") == 0
                 && service->user[0] == '\0')
            clean_text(value, service->user, sizeof(service->user));
        else if (section == SECTION_SERVICE && strcmp(line, "ExecStart") == 0
                 && service->executable[0] == '\0')
            executable_label(value, service->executable,
                             sizeof(service->executable), true);
        else if (section == SECTION_INSTALL
                 && (strcmp(line, "WantedBy") == 0
                     || strcmp(line, "RequiredBy") == 0
                     || strcmp(line, "Alias") == 0)
                 && *value) installable = true;
    }
    if (strcmp(service->startup, "masked") != 0)
        (void)snprintf(service->startup, sizeof(service->startup), "%s",
                       installable ? "disabled" : "static");
}

static void check_mask(const char *directory, const char *name,
                       mon_service *service) {
    char path[MON_PATH_MAX];
    int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (written < 0 || (size_t)written >= sizeof(path)) return;
    char target[MON_PATH_MAX];
    ssize_t count = readlink(path, target, sizeof(target) - 1U);
    if (count <= 0 || (size_t)count >= sizeof(target)) return;
    target[count] = '\0';
    if (strcmp(target, "/dev/null") == 0)
        (void)snprintf(service->startup, sizeof(service->startup), "masked");
}

static int collect_service_directory(const char *directory,
                                     mon_service_snapshot *snapshot,
                                     size_t *capacity) {
    DIR *stream = opendir(directory);
    if (!stream) {
        if (errno == ENOENT) return 0;
        if (errno == EACCES || errno == EPERM) {
            snapshot->denied++;
            return 0;
        }
        return -1;
    }
    struct dirent *entry = NULL;
    while ((entry = readdir(stream)) != NULL) {
        if (!has_suffix(entry->d_name, ".service")
            || !safe_name(entry->d_name, MON_LABEL_MAX)) continue;
        snapshot->files_seen++;
        mon_service *service = find_service_linear(snapshot, entry->d_name);
        if (!service) {
            if (snapshot->count >= MON_MAX_SERVICES) {
                snapshot->truncated = true;
                continue;
            }
            if (reserve_service(snapshot, capacity) != 0) {
                closedir(stream);
                return -1;
            }
            service = &snapshot->rows[snapshot->count];
            memset(service, 0, sizeof(*service));
            if (!copy_bounded(service->name, sizeof(service->name),
                              entry->d_name)) {
                snapshot->malformed++;
                continue;
            }
            snapshot->count++;
            (void)snprintf(service->status, sizeof(service->status), "inactive");
            (void)snprintf(service->startup, sizeof(service->startup), "static");
        }
        check_mask(directory, entry->d_name, service);
        char path[MON_PATH_MAX];
        int written = snprintf(path, sizeof(path), "%s/%s", directory,
                               entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            snapshot->malformed++;
            continue;
        }
        errno = 0;
        size_t size = 0U;
        char *text = read_file(path, MON_UNIT_FILE_LIMIT, &size);
        (void)size;
        if (!text) {
            if (errno == EACCES || errno == EPERM) snapshot->denied++;
            else if (errno != ELOOP) snapshot->malformed++;
            continue;
        }
        parse_service_file(text, service);
        free(text);
    }
    closedir(stream);
    return 0;
}

static int compare_service_name(const void *left_value, const void *right_value) {
    const mon_service *left = left_value;
    const mon_service *right = right_value;
    return strcmp(left->name, right->name);
}

static mon_service *find_service_sorted(mon_service_snapshot *snapshot,
                                        const char *name) {
    mon_service key;
    memset(&key, 0, sizeof(key));
    if (!copy_bounded(key.name, sizeof(key.name), name)) return NULL;
    return bsearch(&key, snapshot->rows, snapshot->count, sizeof(*snapshot->rows),
                   compare_service_name);
}

static void mark_enabled_directory(const char *root,
                                   mon_service_snapshot *snapshot) {
    DIR *stream = opendir(root);
    if (!stream) return;
    struct dirent *entry = NULL;
    size_t directories = 0U;
    while ((entry = readdir(stream)) != NULL && directories < 4096U) {
        if ((!has_suffix(entry->d_name, ".wants")
             && !has_suffix(entry->d_name, ".requires"))
            || !safe_name(entry->d_name, MON_LABEL_MAX)) continue;
        directories++;
        char path[MON_PATH_MAX];
        int written = snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) continue;
        DIR *children = opendir(path);
        if (!children) continue;
        struct dirent *child = NULL;
        size_t entries = 0U;
        while ((child = readdir(children)) != NULL && entries < 8192U) {
            entries++;
            if (!has_suffix(child->d_name, ".service")
                || !safe_name(child->d_name, MON_LABEL_MAX)) continue;
            char link_path[MON_PATH_MAX];
            written = snprintf(link_path, sizeof(link_path), "%s/%s", path,
                               child->d_name);
            struct stat link_status;
            if (written < 0 || (size_t)written >= sizeof(link_path)
                || lstat(link_path, &link_status) != 0
                || !S_ISLNK(link_status.st_mode)) continue;
            mon_service *service = find_service_sorted(snapshot, child->d_name);
            if (service && strcmp(service->startup, "masked") != 0)
                (void)snprintf(service->startup, sizeof(service->startup), "enabled");
        }
        closedir(children);
    }
    closedir(stream);
}

static bool cgroup_populated(const char *path) {
    char events[MON_PATH_MAX];
    int written = snprintf(events, sizeof(events), "%s/cgroup.events", path);
    if (written < 0 || (size_t)written >= sizeof(events)) return false;
    size_t size = 0U;
    char *text = read_file(events, 4096U, &size);
    (void)size;
    if (!text) return false;
    bool populated = strstr(text, "populated 1") != NULL;
    free(text);
    return populated;
}

static void service_runtime(const mon_roots *roots, mon_service *service) {
    char path[MON_PATH_MAX];
    int written = snprintf(path, sizeof(path), "%s/fs/cgroup/system.slice/%s",
                           roots->sys_root, service->name);
    if (written < 0 || (size_t)written >= sizeof(path) || !cgroup_populated(path))
        return;
    (void)snprintf(service->status, sizeof(service->status), "active");
    char procs[MON_PATH_MAX];
    written = snprintf(procs, sizeof(procs), "%s/cgroup.procs", path);
    if (written < 0 || (size_t)written >= sizeof(procs)) return;
    size_t size = 0U;
    char *text = read_file(procs, MON_PROCESS_FILE_LIMIT, &size);
    (void)size;
    if (!text) return;
    char *newline = strchr(text, '\n');
    if (newline) *newline = '\0';
    uint64_t pid = 0U;
    if (parse_u64(text, 10, &pid) && pid > 0U && pid <= INT_MAX) {
        service->pid_available = true;
        service->pid = (int)pid;
    }
    free(text);
}

int mon_collect_services(const mon_roots *roots, mon_service_snapshot *snapshot,
                         char *error, size_t error_size) {
    if (!roots || !snapshot) return -1;
    memset(snapshot, 0, sizeof(*snapshot));
    size_t capacity = 0U;
    char directories[3][MON_PATH_MAX];
    if (join_root(directories[0], sizeof(directories[0]), roots->etc_root,
                  "/systemd/system") != 0
        || join_root(directories[1], sizeof(directories[1]), roots->run_root,
                     "/systemd/system") != 0
        || join_root(directories[2], sizeof(directories[2]), roots->usr_root,
                     "/lib/systemd/system") != 0) goto invalid;
    for (size_t i = 0U; i < 3U; i++) {
        if (collect_service_directory(directories[i], snapshot, &capacity) != 0)
            goto invalid;
    }
    if (snapshot->count > 1U)
        qsort(snapshot->rows, snapshot->count, sizeof(*snapshot->rows),
              compare_service_name);
    mark_enabled_directory(directories[0], snapshot);
    mark_enabled_directory(directories[1], snapshot);
    for (size_t i = 0U; i < snapshot->count; i++) {
        mon_service *service = &snapshot->rows[i];
        if (service->description[0] == '\0')
            (void)snprintf(service->description, sizeof(service->description), "%s",
                           service->name);
        if (service->user[0] == '\0')
            (void)snprintf(service->user, sizeof(service->user), "root");
        if (service->executable[0] == '\0')
            (void)snprintf(service->executable, sizeof(service->executable),
                           "unavailable");
        service_runtime(roots, service);
    }
    snapshot->matched = snapshot->count;
    return 0;
invalid:
    mon_service_snapshot_free(snapshot);
    if (error && error_size > 0U)
        snprintf(error, error_size, "bounded service inventory failed");
    return -1;
}

void mon_service_snapshot_free(mon_service_snapshot *snapshot) {
    if (!snapshot) return;
    free(snapshot->rows);
    memset(snapshot, 0, sizeof(*snapshot));
}

static mon_sort service_sort = MON_SORT_NAME;

static int compare_services(const void *left_value, const void *right_value) {
    const mon_service *left = left_value;
    const mon_service *right = right_value;
    int compared = 0;
    if (service_sort == MON_SORT_STATUS)
        compared = strcmp(left->status, right->status);
    else if (service_sort == MON_SORT_STARTUP)
        compared = strcmp(left->startup, right->startup);
    else if (service_sort == MON_SORT_DESCRIPTION)
        compared = strcmp(left->description, right->description);
    else if (service_sort == MON_SORT_USER)
        compared = strcmp(left->user, right->user);
    else if (service_sort == MON_SORT_EXECUTABLE)
        compared = strcmp(left->executable, right->executable);
    else if (service_sort == MON_SORT_PID) {
        if (left->pid_available != right->pid_available)
            return left->pid_available ? -1 : 1;
        if (left->pid < right->pid) return -1;
        if (left->pid > right->pid) return 1;
    } else compared = strcmp(left->name, right->name);
    return compared != 0 ? compared : strcmp(left->name, right->name);
}

void mon_prepare_services(mon_service_snapshot *snapshot,
                          const mon_options *options) {
    if (!snapshot || !options) return;
    size_t kept = 0U;
    for (size_t i = 0U; i < snapshot->count; i++) {
        mon_service row = snapshot->rows[i];
        if (!contains_ascii_casefold(row.name, options->filter)
            && !contains_ascii_casefold(row.description, options->filter)
            && !contains_ascii_casefold(row.status, options->filter)) continue;
        snapshot->rows[kept++] = row;
    }
    snapshot->matched = kept;
    service_sort = options->sort;
    if (kept > 1U)
        qsort(snapshot->rows, kept, sizeof(*snapshot->rows), compare_services);
    snapshot->count = kept < options->limit ? kept : options->limit;
}

static int reserve_startup(mon_startup_snapshot *snapshot, size_t *capacity) {
    if (snapshot->count < *capacity) return 0;
    if (*capacity >= MON_MAX_STARTUP_ITEMS) return -1;
    size_t next = *capacity == 0U ? 128U : *capacity * 2U;
    if (next > MON_MAX_STARTUP_ITEMS) next = MON_MAX_STARTUP_ITEMS;
    mon_startup_item *rows = realloc(snapshot->rows, next * sizeof(*rows));
    if (!rows) return -1;
    snapshot->rows = rows;
    *capacity = next;
    return 0;
}

static mon_startup_item *find_startup(mon_startup_snapshot *snapshot,
                                      const char *id) {
    for (size_t i = 0U; i < snapshot->count; i++)
        if (strcmp(snapshot->rows[i].id, id) == 0) return &snapshot->rows[i];
    return NULL;
}

static bool boolean_false(const char *value) {
    return strcmp(value, "false") == 0 || strcmp(value, "False") == 0
        || strcmp(value, "0") == 0 || strcmp(value, "no") == 0;
}

static bool parse_desktop_file(char *text, mon_startup_item *item,
                               const char *scope) {
    bool desktop_section = false;
    bool application = false;
    bool disabled = false;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t') line++;
        size_t length = strlen(line);
        while (length > 0U && (line[length - 1U] == '\r'
                               || line[length - 1U] == ' '
                               || line[length - 1U] == '\t'))
            line[--length] = '\0';
        if (*line == '#' || *line == ';' || *line == '\0') continue;
        if (line[0] == '[' && length > 2U && line[length - 1U] == ']') {
            line[length - 1U] = '\0';
            desktop_section = strcmp(line + 1, "Desktop Entry") == 0;
            continue;
        }
        if (!desktop_section) continue;
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        const char *value = equals + 1;
        if (strcmp(line, "Name") == 0 && item->name[0] == '\0')
            clean_text(value, item->name, sizeof(item->name));
        else if ((strcmp(line, "X-AppStream-DeveloperName") == 0
                  || strcmp(line, "X-Developer") == 0)
                 && item->publisher[0] == '\0')
            clean_text(value, item->publisher, sizeof(item->publisher));
        else if (strcmp(line, "Type") == 0 && strcmp(value, "Application") == 0)
            application = true;
        else if (strcmp(line, "Exec") == 0 && *value) {
            item->launch_present = true;
            executable_label(value, item->command, sizeof(item->command), false);
        }
        else if ((strcmp(line, "Hidden") == 0 && !boolean_false(value))
                 || (strcmp(line, "X-GNOME-Autostart-enabled") == 0
                     && boolean_false(value))) disabled = true;
    }
    if (!application || item->name[0] == '\0') return false;
    if (item->publisher[0] == '\0')
        (void)snprintf(item->publisher, sizeof(item->publisher), "unavailable");
    (void)snprintf(item->status, sizeof(item->status), "%s",
                   disabled ? "disabled" : "enabled");
    (void)snprintf(item->type, sizeof(item->type), "desktop");
    (void)snprintf(item->scope, sizeof(item->scope), "%s", scope);
    (void)snprintf(item->location, sizeof(item->location), "%s autostart", scope);
    if (item->command[0] == '\0')
        (void)snprintf(item->command, sizeof(item->command), "unavailable");
    return true;
}

static int collect_startup_directory(const char *directory, const char *scope,
                                     mon_startup_snapshot *snapshot,
                                     size_t *capacity) {
    DIR *stream = opendir(directory);
    if (!stream) {
        if (errno == ENOENT) return 0;
        if (errno == EACCES || errno == EPERM) {
            snapshot->denied++;
            return 0;
        }
        return -1;
    }
    struct dirent *entry = NULL;
    while ((entry = readdir(stream)) != NULL) {
        if (!has_suffix(entry->d_name, ".desktop")
            || !safe_name(entry->d_name, MON_LABEL_MAX)) continue;
        snapshot->files_seen++;
        char path[MON_PATH_MAX];
        int written = snprintf(path, sizeof(path), "%s/%s", directory,
                               entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            snapshot->malformed++;
            continue;
        }
        errno = 0;
        size_t size = 0U;
        char *text = read_file(path, MON_DESKTOP_FILE_LIMIT, &size);
        (void)size;
        if (!text) {
            if (errno == EACCES || errno == EPERM) snapshot->denied++;
            else snapshot->malformed++;
            continue;
        }
        mon_startup_item parsed;
        memset(&parsed, 0, sizeof(parsed));
        if (!copy_bounded(parsed.id, sizeof(parsed.id), entry->d_name)) {
            free(text);
            snapshot->malformed++;
            continue;
        }
        bool valid = parse_desktop_file(text, &parsed, scope);
        free(text);
        if (!valid) {
            snapshot->malformed++;
            continue;
        }
        mon_startup_item *existing = find_startup(snapshot, parsed.id);
        if (existing) {
            if (strcmp(scope, "user") == 0) *existing = parsed;
            continue;
        }
        if (snapshot->count >= MON_MAX_STARTUP_ITEMS) {
            snapshot->truncated = true;
            continue;
        }
        if (reserve_startup(snapshot, capacity) != 0) {
            closedir(stream);
            return -1;
        }
        snapshot->rows[snapshot->count++] = parsed;
    }
    closedir(stream);
    return 0;
}

int mon_collect_startup(const mon_roots *roots, mon_startup_snapshot *snapshot,
                        char *error, size_t error_size) {
    if (!roots || !snapshot) return -1;
    memset(snapshot, 0, sizeof(*snapshot));
    size_t capacity = 0U;
    char system[MON_PATH_MAX];
    char user[MON_PATH_MAX];
    if (join_root(system, sizeof(system), roots->etc_root, "/xdg/autostart") != 0
        || join_root(user, sizeof(user), roots->home_root,
                     "/.config/autostart") != 0) goto invalid;
    if (collect_startup_directory(system, "system", snapshot, &capacity) != 0
        || collect_startup_directory(user, "user", snapshot, &capacity) != 0)
        goto invalid;
    snapshot->matched = snapshot->count;
    return 0;
invalid:
    mon_startup_snapshot_free(snapshot);
    if (error && error_size > 0U)
        snprintf(error, error_size, "bounded startup inventory failed");
    return -1;
}

void mon_startup_snapshot_free(mon_startup_snapshot *snapshot) {
    if (!snapshot) return;
    free(snapshot->rows);
    memset(snapshot, 0, sizeof(*snapshot));
}

static mon_sort startup_sort = MON_SORT_NAME;

static int compare_startup(const void *left_value, const void *right_value) {
    const mon_startup_item *left = left_value;
    const mon_startup_item *right = right_value;
    int compared;
    if (startup_sort == MON_SORT_STATUS)
        compared = strcmp(left->status, right->status);
    else if (startup_sort == MON_SORT_SCOPE)
        compared = strcmp(left->scope, right->scope);
    else if (startup_sort == MON_SORT_TYPE)
        compared = strcmp(left->type, right->type);
    else if (startup_sort == MON_SORT_PUBLISHER)
        compared = strcmp(left->publisher, right->publisher);
    else if (startup_sort == MON_SORT_COMMAND)
        compared = strcmp(left->command, right->command);
    else compared = strcmp(left->name, right->name);
    return compared != 0 ? compared : strcmp(left->id, right->id);
}

void mon_prepare_startup(mon_startup_snapshot *snapshot,
                         const mon_options *options) {
    if (!snapshot || !options) return;
    size_t kept = 0U;
    for (size_t i = 0U; i < snapshot->count; i++) {
        mon_startup_item row = snapshot->rows[i];
        if (!contains_ascii_casefold(row.name, options->filter)
            && !contains_ascii_casefold(row.publisher, options->filter)
            && !contains_ascii_casefold(row.status, options->filter)
            && !contains_ascii_casefold(row.scope, options->filter)) continue;
        snapshot->rows[kept++] = row;
    }
    snapshot->matched = kept;
    startup_sort = options->sort;
    if (kept > 1U)
        qsort(snapshot->rows, kept, sizeof(*snapshot->rows), compare_startup);
    snapshot->count = kept < options->limit ? kept : options->limit;
}

static int reserve_connection(mon_connection_snapshot *snapshot, size_t *capacity) {
    if (snapshot->count < *capacity) return 0;
    if (*capacity >= MON_MAX_CONNECTIONS) return -1;
    size_t next = *capacity == 0U ? 256U : *capacity * 2U;
    if (next > MON_MAX_CONNECTIONS) next = MON_MAX_CONNECTIONS;
    mon_connection *rows = realloc(snapshot->rows, next * sizeof(*rows));
    if (!rows) return -1;
    snapshot->rows = rows;
    *capacity = next;
    return 0;
}

static bool parse_hex_byte(const char *text, unsigned char *value) {
    char buffer[3] = {text[0], text[1], '\0'};
    uint64_t parsed = 0U;
    if (!isxdigit((unsigned char)text[0]) || !isxdigit((unsigned char)text[1])
        || !parse_u64(buffer, 16, &parsed) || parsed > 255U) return false;
    *value = (unsigned char)parsed;
    return true;
}

static bool decode_address(const char *encoded, bool ipv6, char *output,
                           size_t output_size) {
    size_t expected = ipv6 ? 32U : 8U;
    if (!encoded || strlen(encoded) != expected || !output) return false;
    unsigned char bytes[16] = {0U};
    if (!ipv6) {
        for (size_t i = 0U; i < 4U; i++)
            if (!parse_hex_byte(encoded + (3U - i) * 2U, &bytes[i])) return false;
    } else {
        for (size_t word = 0U; word < 4U; word++) {
            for (size_t byte = 0U; byte < 4U; byte++) {
                if (!parse_hex_byte(encoded + (word * 8U + (3U - byte) * 2U),
                                    &bytes[word * 4U + byte])) return false;
            }
        }
    }
    return inet_ntop(ipv6 ? AF_INET6 : AF_INET, bytes, output,
                     (socklen_t)output_size) != NULL;
}

static bool endpoint(const char *encoded, bool ipv6, char *output,
                     size_t output_size) {
    const char *colon = strchr(encoded, ':');
    if (!colon || strchr(colon + 1, ':')) return false;
    size_t address_length = (size_t)(colon - encoded);
    if (address_length != (ipv6 ? 32U : 8U) || strlen(colon + 1) != 4U)
        return false;
    char address_hex[33];
    memcpy(address_hex, encoded, address_length);
    address_hex[address_length] = '\0';
    char address[INET6_ADDRSTRLEN];
    if (!decode_address(address_hex, ipv6, address, sizeof(address))) return false;
    uint64_t port = 0U;
    if (!parse_u64(colon + 1, 16, &port) || port > 65535U) return false;
    int written = ipv6
        ? snprintf(output, output_size, "[%s]:%" PRIu64, address, port)
        : snprintf(output, output_size, "%s:%" PRIu64, address, port);
    return written >= 0 && (size_t)written < output_size;
}

static const char *connection_state(const char *encoded, bool tcp) {
    uint64_t state = 0U;
    if (!parse_u64(encoded, 16, &state) || state > 255U) return "unknown";
    if (!tcp) return state == 7U ? "unconnected" : "active";
    switch (state) {
        case 1U: return "established";
        case 2U: return "syn-sent";
        case 3U: return "syn-received";
        case 4U: return "fin-wait-1";
        case 5U: return "fin-wait-2";
        case 6U: return "time-wait";
        case 7U: return "closed";
        case 8U: return "close-wait";
        case 9U: return "last-ack";
        case 10U: return "listening";
        case 11U: return "closing";
        case 12U: return "new-syn-received";
        default: return "unknown";
    }
}

static int collect_connection_file(const char *path, const char *protocol,
                                   bool ipv6, bool tcp,
                                   mon_connection_snapshot *snapshot,
                                   size_t *capacity) {
    errno = 0;
    size_t size = 0U;
    char *text = read_file(path, MON_CONNECTION_FILE_LIMIT, &size);
    (void)size;
    if (!text) {
        if (errno == ENOENT) return 0;
        if (errno == EACCES || errno == EPERM) snapshot->denied++;
        else if (errno == EFBIG) snapshot->truncated = true;
        else snapshot->malformed++;
        return 0;
    }
    size_t line_number = 0U;
    char *line_save = NULL;
    for (char *line = strtok_r(text, "\n", &line_save); line;
         line = strtok_r(NULL, "\n", &line_save)) {
        line_number++;
        if (line_number == 1U) continue;
        snapshot->rows_seen++;
        char *fields[20] = {0};
        size_t count = 0U;
        char *save = NULL;
        for (char *token = strtok_r(line, " \t", &save); token && count < 20U;
             token = strtok_r(NULL, " \t", &save)) fields[count++] = token;
        if (count < 10U) {
            snapshot->malformed++;
            continue;
        }
        if (snapshot->count >= MON_MAX_CONNECTIONS) {
            snapshot->truncated = true;
            continue;
        }
        mon_connection row;
        memset(&row, 0, sizeof(row));
        (void)snprintf(row.protocol, sizeof(row.protocol), "%s", protocol);
        if (!endpoint(fields[1], ipv6, row.local, sizeof(row.local))
            || !endpoint(fields[2], ipv6, row.remote, sizeof(row.remote))
            || !parse_u64(fields[9], 10, &row.inode)) {
            snapshot->malformed++;
            continue;
        }
        if (row.inode == 0U) {
            snapshot->identity_unavailable++;
            continue;
        }
        (void)snprintf(row.state, sizeof(row.state), "%s",
                       connection_state(fields[3], tcp));
        if (reserve_connection(snapshot, capacity) != 0) {
            free(text);
            return -1;
        }
        snapshot->rows[snapshot->count++] = row;
    }
    free(text);
    return 0;
}

typedef struct {
    uint64_t inode;
    size_t row;
} mon_inode_reference;

static int compare_inode_reference(const void *left_value,
                                   const void *right_value) {
    const mon_inode_reference *left = left_value;
    const mon_inode_reference *right = right_value;
    if (left->inode < right->inode) return -1;
    if (left->inode > right->inode) return 1;
    if (left->row < right->row) return -1;
    if (left->row > right->row) return 1;
    return 0;
}

static mon_inode_reference *find_inode(mon_inode_reference *references,
                                       size_t count, uint64_t inode) {
    size_t low = 0U;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        if (references[middle].inode < inode) low = middle + 1U;
        else high = middle;
    }
    return low < count && references[low].inode == inode
        ? &references[low] : NULL;
}

static bool socket_inode(const char *target, uint64_t *inode) {
    static const char prefix[] = "socket:[";
    size_t length = strlen(target);
    if (strncmp(target, prefix, sizeof(prefix) - 1U) != 0
        || length <= sizeof(prefix) || target[length - 1U] != ']') return false;
    char number[32];
    size_t number_length = length - (sizeof(prefix) - 1U) - 1U;
    if (number_length == 0U || number_length >= sizeof(number)) return false;
    memcpy(number, target + sizeof(prefix) - 1U, number_length);
    number[number_length] = '\0';
    return parse_u64(number, 10, inode);
}

static bool process_start_matches(const mon_roots *roots, int pid,
                                  uint64_t expected_start) {
    char path[MON_PATH_MAX];
    int written = snprintf(path, sizeof(path), "%s/%d/stat", roots->proc_root, pid);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;
    size_t size = 0U;
    char *text = read_file(path, 4096U, &size);
    (void)size;
    if (!text) return false;
    bool matches = false;
    char *close = strrchr(text, ')');
    if (close && close[1] == ' ') {
        char *tokens[24] = {0};
        size_t count = 0U;
        char *save = NULL;
        for (char *token = strtok_r(close + 2, " ", &save);
             token && count < 24U; token = strtok_r(NULL, " ", &save))
            tokens[count++] = token;
        uint64_t start = 0U;
        matches = count > 19U && parse_u64(tokens[19], 10, &start)
            && start == expected_start;
    }
    free(text);
    return matches;
}

static void correlate_connection_owners(const mon_roots *roots,
                                        mon_connection_snapshot *snapshot) {
    if (snapshot->count == 0U) return;
    mon_inode_reference *references = calloc(snapshot->count, sizeof(*references));
    if (!references) {
        snapshot->owner_scan_truncated = true;
        return;
    }
    for (size_t i = 0U; i < snapshot->count; i++) {
        references[i].inode = snapshot->rows[i].inode;
        references[i].row = i;
    }
    qsort(references, snapshot->count, sizeof(*references),
          compare_inode_reference);
    mon_process_snapshot processes;
    char ignored[MON_ERROR_MAX] = {0};
    if (mon_probe_processes(roots, &processes, ignored, sizeof(ignored)) != 0) {
        snapshot->owner_scan_truncated = true;
        free(references);
        return;
    }
    bool stop = false;
    for (size_t i = 0U; i < processes.count && !stop; i++) {
        if (!process_start_matches(roots, processes.rows[i].pid,
                                   processes.rows[i].start_ticks)) continue;
        char directory_path[MON_PATH_MAX];
        int written = snprintf(directory_path, sizeof(directory_path), "%s/%d/fd",
                               roots->proc_root, processes.rows[i].pid);
        if (written < 0 || (size_t)written >= sizeof(directory_path)) continue;
        DIR *directory = opendir(directory_path);
        if (!directory) {
            if (errno == EACCES || errno == EPERM) snapshot->denied++;
            continue;
        }
        struct dirent *entry = NULL;
        while ((entry = readdir(directory)) != NULL) {
            if (!isdigit((unsigned char)entry->d_name[0])) continue;
            if (snapshot->owner_fds_seen >= MON_MAX_FD_LINKS) {
                snapshot->owner_scan_truncated = true;
                stop = true;
                break;
            }
            snapshot->owner_fds_seen++;
            char link_path[MON_PATH_MAX];
            written = snprintf(link_path, sizeof(link_path), "%s/%s",
                               directory_path, entry->d_name);
            if (written < 0 || (size_t)written >= sizeof(link_path)) continue;
            char target[128];
            ssize_t count = readlink(link_path, target, sizeof(target) - 1U);
            if (count <= 0 || (size_t)count >= sizeof(target)) continue;
            target[count] = '\0';
            uint64_t inode = 0U;
            if (!socket_inode(target, &inode)) continue;
            mon_inode_reference *reference = find_inode(references, snapshot->count,
                                                        inode);
            while (reference && (size_t)(reference - references) < snapshot->count
                   && reference->inode == inode) {
                mon_connection *row = &snapshot->rows[reference->row];
                if (!row->pid_available) {
                    row->pid_available = true;
                    row->pid = processes.rows[i].pid;
                    (void)snprintf(row->process, sizeof(row->process), "%s",
                                   processes.rows[i].name);
                }
                reference++;
            }
        }
        closedir(directory);
        if (!process_start_matches(roots, processes.rows[i].pid,
                                   processes.rows[i].start_ticks)) {
            for (size_t row = 0U; row < snapshot->count; row++) {
                if (snapshot->rows[row].pid_available
                    && snapshot->rows[row].pid == processes.rows[i].pid) {
                    snapshot->rows[row].pid_available = false;
                    snapshot->rows[row].pid = 0;
                    snapshot->rows[row].process[0] = '\0';
                }
            }
        }
    }
    mon_process_snapshot_free(&processes);
    free(references);
}

int mon_collect_connections(const mon_roots *roots,
                            mon_connection_snapshot *snapshot,
                            char *error, size_t error_size) {
    if (!roots || !snapshot) return -1;
    memset(snapshot, 0, sizeof(*snapshot));
    size_t capacity = 0U;
    struct source { const char *suffix; const char *protocol; bool ipv6; bool tcp; }
        sources[] = {
            {"/net/tcp", "tcp", false, true},
            {"/net/tcp6", "tcp6", true, true},
            {"/net/udp", "udp", false, false},
            {"/net/udp6", "udp6", true, false}
        };
    for (size_t i = 0U; i < sizeof(sources) / sizeof(sources[0]); i++) {
        char path[MON_PATH_MAX];
        if (join_root(path, sizeof(path), roots->proc_root, sources[i].suffix) != 0
            || collect_connection_file(path, sources[i].protocol, sources[i].ipv6,
                                       sources[i].tcp, snapshot, &capacity) != 0)
            goto invalid;
    }
    correlate_connection_owners(roots, snapshot);
    snapshot->matched = snapshot->count;
    return 0;
invalid:
    mon_connection_snapshot_free(snapshot);
    if (error && error_size > 0U)
        snprintf(error, error_size, "bounded connection inventory failed");
    return -1;
}

void mon_connection_snapshot_free(mon_connection_snapshot *snapshot) {
    if (!snapshot) return;
    free(snapshot->rows);
    memset(snapshot, 0, sizeof(*snapshot));
}

static mon_sort connection_sort = MON_SORT_PROTOCOL;

static int compare_connections(const void *left_value, const void *right_value) {
    const mon_connection *left = left_value;
    const mon_connection *right = right_value;
    int compared = 0;
    if (connection_sort == MON_SORT_LOCAL)
        compared = strcmp(left->local, right->local);
    else if (connection_sort == MON_SORT_REMOTE)
        compared = strcmp(left->remote, right->remote);
    else if (connection_sort == MON_SORT_STATUS)
        compared = strcmp(left->state, right->state);
    else if (connection_sort == MON_SORT_PROCESS) {
        if (left->pid_available != right->pid_available)
            return left->pid_available ? -1 : 1;
        compared = strcmp(left->process, right->process);
    } else if (connection_sort == MON_SORT_PID) {
        if (left->pid_available != right->pid_available)
            return left->pid_available ? -1 : 1;
        if (left->pid < right->pid) return -1;
        if (left->pid > right->pid) return 1;
    } else compared = strcmp(left->protocol, right->protocol);
    if (compared != 0) return compared;
    compared = strcmp(left->local, right->local);
    return compared != 0 ? compared : strcmp(left->remote, right->remote);
}

void mon_prepare_connections(mon_connection_snapshot *snapshot,
                             const mon_options *options) {
    if (!snapshot || !options) return;
    size_t kept = 0U;
    for (size_t i = 0U; i < snapshot->count; i++) {
        mon_connection row = snapshot->rows[i];
        if (!contains_ascii_casefold(row.protocol, options->filter)
            && !contains_ascii_casefold(row.local, options->filter)
            && !contains_ascii_casefold(row.remote, options->filter)
            && !contains_ascii_casefold(row.state, options->filter)
            && !contains_ascii_casefold(row.process, options->filter)) continue;
        snapshot->rows[kept++] = row;
    }
    snapshot->matched = kept;
    connection_sort = options->sort;
    if (kept > 1U)
        qsort(snapshot->rows, kept, sizeof(*snapshot->rows), compare_connections);
    snapshot->count = kept < options->limit ? kept : options->limit;
}

static bool parse_four_ids(const char *value, unsigned output[4]) {
    const char *cursor = value;
    for (size_t i = 0U; i < 4U; i++) {
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (!isdigit((unsigned char)*cursor)) return false;
        errno = 0;
        char *end = NULL;
        unsigned long parsed = strtoul(cursor, &end, 10);
        if (errno || end == cursor || parsed > UINT_MAX) return false;
        output[i] = (unsigned)parsed;
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    return *cursor == '\0';
}

static void capability_value(const char *value, char output[65]) {
    size_t length = strlen(value);
    if (length == 0U || length > 64U) return;
    for (size_t i = 0U; i < length; i++)
        if (!isxdigit((unsigned char)value[i])) return;
    (void)snprintf(output, 65U, "%s", value);
}

static void parse_inspection_status(char *text,
                                    mon_process_inspection *inspection) {
    bool uid_seen = false;
    bool gid_seen = false;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *value = colon + 1U;
        while (*value == ' ' || *value == '\t') value++;
        size_t length = strlen(value);
        while (length > 0U && (value[length - 1U] == ' '
                               || value[length - 1U] == '\t'
                               || value[length - 1U] == '\r'))
            value[--length] = '\0';
        if (strcmp(line, "Uid") == 0)
            uid_seen = parse_four_ids(value, inspection->uids);
        else if (strcmp(line, "Gid") == 0)
            gid_seen = parse_four_ids(value, inspection->gids);
        else if (strcmp(line, "CapInh") == 0)
            capability_value(value, inspection->capability_inheritable);
        else if (strcmp(line, "CapPrm") == 0)
            capability_value(value, inspection->capability_permitted);
        else if (strcmp(line, "CapEff") == 0)
            capability_value(value, inspection->capability_effective);
        else if (strcmp(line, "CapBnd") == 0)
            capability_value(value, inspection->capability_bounding);
        else if (strcmp(line, "CapAmb") == 0)
            capability_value(value, inspection->capability_ambient);
        else if (strcmp(line, "NoNewPrivs") == 0) {
            uint64_t parsed = 0U;
            if (parse_u64(value, 10, &parsed) && parsed <= 1U) {
                inspection->no_new_privileges_available = true;
                inspection->no_new_privileges = parsed == 1U;
            }
        } else if (strcmp(line, "Seccomp") == 0) {
            uint64_t parsed = 0U;
            if (parse_u64(value, 10, &parsed) && parsed <= UINT_MAX) {
                inspection->seccomp_available = true;
                inspection->seccomp_mode = (unsigned)parsed;
            }
        }
    }
    inspection->credentials_available = uid_seen && gid_seen;
}

static bool inspection_module_exists(const mon_process_inspection *inspection,
                                     const char *name) {
    for (size_t i = 0U; i < inspection->module_count; i++)
        if (strcmp(inspection->modules[i], name) == 0) return true;
    return false;
}

static int compare_module_names(const void *left_value, const void *right_value) {
    return strcmp((const char *)left_value, (const char *)right_value);
}

static void collect_modules(const char *path,
                            mon_process_inspection *inspection) {
    errno = 0;
    size_t size = 0U;
    char *text = read_file(path, MON_MAPS_FILE_LIMIT, &size);
    (void)size;
    if (!text) {
        if (errno == EACCES || errno == EPERM) inspection->module_denied = true;
        else if (errno == EFBIG) inspection->module_truncated = true;
        return;
    }
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        inspection->module_rows_seen++;
        char *pathname = strchr(line, '/');
        if (!pathname) continue;
        char *deleted = strstr(pathname, " (deleted)");
        if (deleted) *deleted = '\0';
        char *base = strrchr(pathname, '/');
        base = base ? base + 1U : pathname;
        char name[MON_NAME_MAX + 1U];
        clean_text(base, name, sizeof(name));
        if (name[0] == '\0' || inspection_module_exists(inspection, name)) continue;
        if (inspection->module_count >= MON_MAX_MODULES) {
            inspection->module_truncated = true;
            continue;
        }
        (void)snprintf(inspection->modules[inspection->module_count],
                       sizeof(inspection->modules[inspection->module_count]), "%s",
                       name);
        inspection->module_count++;
    }
    free(text);
    if (inspection->module_count > 1U)
        qsort(inspection->modules, inspection->module_count,
              sizeof(inspection->modules[0]), compare_module_names);
}

static void collect_inspection_fds(const char *directory_path,
                                   mon_process_inspection *inspection) {
    DIR *directory = opendir(directory_path);
    if (!directory) {
        if (errno == EACCES || errno == EPERM) inspection->fd_denied = true;
        return;
    }
    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        if (inspection->fd_count >= 8192U) {
            inspection->fd_truncated = true;
            continue;
        }
        inspection->fd_count++;
        char path[MON_PATH_MAX];
        int written = snprintf(path, sizeof(path), "%s/%s", directory_path,
                               entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) continue;
        char target[128];
        ssize_t count = readlink(path, target, sizeof(target) - 1U);
        if (count <= 0 || (size_t)count >= sizeof(target)) continue;
        target[count] = '\0';
        uint64_t inode = 0U;
        if (socket_inode(target, &inode)) inspection->socket_count++;
    }
    closedir(directory);
}

int mon_collect_process_inspection(const mon_roots *roots, int pid,
                                   mon_process_inspection *inspection,
                                   char *error, size_t error_size) {
    if (!roots || !inspection || pid <= 0 || pid > 4194304) return -1;
    memset(inspection, 0, sizeof(*inspection));
    mon_process_snapshot processes;
    if (mon_probe_processes(roots, &processes, error, error_size) != 0) return -1;
    const mon_process *found = NULL;
    for (size_t i = 0U; i < processes.count; i++) {
        if (processes.rows[i].pid == pid) {
            found = &processes.rows[i];
            break;
        }
    }
    if (!found) {
        mon_process_snapshot_free(&processes);
        if (error && error_size > 0U)
            snprintf(error, error_size, "process unavailable or access denied");
        return -1;
    }
    inspection->pid = pid;
    inspection->start_ticks = found->start_ticks;
    (void)snprintf(inspection->name, sizeof(inspection->name), "%s", found->name);
    mon_process_snapshot_free(&processes);
    char path[MON_PATH_MAX];
    size_t size = 0U;
    int written = snprintf(path, sizeof(path), "%s/%d/status", roots->proc_root,
                           pid);
    if (written >= 0 && (size_t)written < sizeof(path)) {
        char *status = read_file(path, MON_PROCESS_FILE_LIMIT, &size);
        if (status) {
            parse_inspection_status(status, inspection);
            free(status);
        }
    }
    written = snprintf(path, sizeof(path), "%s/%d/maps", roots->proc_root, pid);
    if (written >= 0 && (size_t)written < sizeof(path))
        collect_modules(path, inspection);
    written = snprintf(path, sizeof(path), "%s/%d/fd", roots->proc_root, pid);
    if (written >= 0 && (size_t)written < sizeof(path))
        collect_inspection_fds(path, inspection);
    if (!process_start_matches(roots, pid, inspection->start_ticks)) {
        if (error && error_size > 0U)
            snprintf(error, error_size, "process identity changed during inspection");
        return -1;
    }
    return 0;
}

static bool read_value(const char *path, char *output, size_t output_size) {
    size_t size = 0U;
    char *text = read_file(path, 4096U, &size);
    (void)size;
    if (!text) return false;
    char *newline = strpbrk(text, "\r\n");
    if (newline) *newline = '\0';
    clean_text(text, output, output_size);
    free(text);
    return output[0] != '\0';
}

static bool os_release_value(const char *path, const char *key,
                             char *output, size_t output_size) {
    size_t size = 0U;
    char *text = read_file(path, MON_STAT_FILE_LIMIT, &size);
    (void)size;
    if (!text) return false;
    bool found = false;
    size_t key_length = strlen(key);
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') continue;
        char *value = line + key_length + 1U;
        size_t length = strlen(value);
        if (length >= 2U && value[0] == '"' && value[length - 1U] == '"') {
            value[length - 1U] = '\0';
            value++;
        }
        clean_text(value, output, output_size);
        found = output[0] != '\0';
        break;
    }
    free(text);
    return found;
}

static bool memory_total(const char *path, uint64_t *bytes) {
    size_t size = 0U;
    char *text = read_file(path, MON_STAT_FILE_LIMIT, &size);
    (void)size;
    if (!text) return false;
    bool found = false;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "MemTotal:", 9U) != 0) continue;
        char *cursor = line + 9U;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        char *end = cursor;
        while (isdigit((unsigned char)*end)) end++;
        char saved = *end;
        *end = '\0';
        uint64_t kib = 0U;
        found = parse_u64(cursor, 10, &kib) && kib <= UINT64_MAX / 1024U;
        *end = saved;
        if (found) *bytes = kib * 1024U;
        break;
    }
    free(text);
    return found;
}

static bool cpu_model(const char *path, char *output, size_t output_size) {
    size_t size = 0U;
    char *text = read_file(path, MON_STAT_FILE_LIMIT, &size);
    (void)size;
    if (!text) return false;
    bool found = false;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "model name", 10U) != 0
            && strncmp(line, "Hardware", 8U) != 0) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        clean_text(colon + 1, output, output_size);
        found = output[0] != '\0';
        break;
    }
    free(text);
    return found;
}

int mon_collect_information(const mon_roots *roots, mon_information *information,
                            char *error, size_t error_size) {
    if (!roots || !information) return -1;
    memset(information, 0, sizeof(*information));
#if defined(__x86_64__)
    (void)snprintf(information->architecture, sizeof(information->architecture),
                   "x86_64");
#elif defined(__aarch64__)
    (void)snprintf(information->architecture, sizeof(information->architecture),
                   "aarch64");
#else
    (void)snprintf(information->architecture, sizeof(information->architecture),
                   "unknown");
#endif
    char path[MON_PATH_MAX];
    if (join_root(path, sizeof(path), roots->etc_root, "/os-release") != 0
        || !os_release_value(path, "PRETTY_NAME", information->operating_system,
                             sizeof(information->operating_system)))
        information->unavailable_fields++;
    if (join_root(path, sizeof(path), roots->proc_root,
                  "/sys/kernel/osrelease") != 0
        || !read_value(path, information->kernel, sizeof(information->kernel)))
        information->unavailable_fields++;
    if (join_root(path, sizeof(path), roots->proc_root, "/cpuinfo") != 0
        || !cpu_model(path, information->processor, sizeof(information->processor)))
        information->unavailable_fields++;
    struct field { const char *suffix; char *value; size_t size; } fields[] = {
        {"/class/dmi/id/sys_vendor", information->system_vendor,
         sizeof(information->system_vendor)},
        {"/class/dmi/id/product_name", information->system_model,
         sizeof(information->system_model)},
        {"/class/dmi/id/bios_vendor", information->firmware_vendor,
         sizeof(information->firmware_vendor)},
        {"/class/dmi/id/bios_version", information->firmware_version,
         sizeof(information->firmware_version)},
        {"/class/dmi/id/bios_date", information->firmware_date,
         sizeof(information->firmware_date)}
    };
    for (size_t i = 0U; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (join_root(path, sizeof(path), roots->sys_root, fields[i].suffix) != 0
            || !read_value(path, fields[i].value, fields[i].size))
            information->unavailable_fields++;
    }
    if (join_root(path, sizeof(path), roots->proc_root, "/meminfo") == 0
        && memory_total(path, &information->memory_total_bytes))
        information->memory_available = true;
    else information->unavailable_fields++;
    if (join_root(path, sizeof(path), roots->proc_root, "/uptime") == 0) {
        size_t size = 0U;
        char *text = read_file(path, 256U, &size);
        (void)size;
        if (text) {
            errno = 0;
            char *end = NULL;
            long double seconds = strtold(text, &end);
            if (!errno && end != text && seconds >= 0.0L
                && seconds <= (long double)UINT64_MAX) {
                information->uptime_available = true;
                information->uptime_seconds = (uint64_t)seconds;
            }
            free(text);
        }
    }
    if (!information->uptime_available) information->unavailable_fields++;
    if (information->operating_system[0] == '\0'
        && information->kernel[0] == '\0'
        && information->processor[0] == '\0') {
        if (error && error_size > 0U)
            snprintf(error, error_size, "system information unavailable");
        return -1;
    }
    return 0;
}
