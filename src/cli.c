// SPDX-License-Identifier: MIT
#include "monitor.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int mon_parse_format(const char *value, mon_format *format) {
    if (!value || !format) return -1;
    if (strcmp(value, "text") == 0) *format = MON_FORMAT_TEXT;
    else if (strcmp(value, "json") == 0) *format = MON_FORMAT_JSON;
    else return -1;
    return 0;
}

int mon_parse_sort(const char *value, mon_sort *sort) {
    if (!value || !sort) return -1;
    if (strcmp(value, "cpu") == 0) *sort = MON_SORT_CPU;
    else if (strcmp(value, "memory") == 0) *sort = MON_SORT_MEMORY;
    else if (strcmp(value, "read") == 0) *sort = MON_SORT_READ;
    else if (strcmp(value, "write") == 0) *sort = MON_SORT_WRITE;
    else if (strcmp(value, "name") == 0) *sort = MON_SORT_NAME;
    else if (strcmp(value, "pid") == 0) *sort = MON_SORT_PID;
    else return -1;
    return 0;
}

int mon_parse_group(const char *value, mon_group *group) {
    if (!value || !group) return -1;
    if (strcmp(value, "class") == 0) *group = MON_GROUP_CLASS;
    else if (strcmp(value, "name") == 0) *group = MON_GROUP_NAME;
    else if (strcmp(value, "none") == 0) *group = MON_GROUP_NONE;
    else return -1;
    return 0;
}

static int column_flag(const char *name, uint32_t *flag) {
    if (strcmp(name, "name") == 0) *flag = MON_COLUMN_NAME;
    else if (strcmp(name, "pid") == 0) *flag = MON_COLUMN_PID;
    else if (strcmp(name, "user") == 0) *flag = MON_COLUMN_USER;
    else if (strcmp(name, "state") == 0) *flag = MON_COLUMN_STATE;
    else if (strcmp(name, "threads") == 0) *flag = MON_COLUMN_THREADS;
    else if (strcmp(name, "cpu") == 0) *flag = MON_COLUMN_CPU;
    else if (strcmp(name, "memory") == 0) *flag = MON_COLUMN_MEMORY;
    else if (strcmp(name, "read") == 0) *flag = MON_COLUMN_READ;
    else if (strcmp(name, "write") == 0) *flag = MON_COLUMN_WRITE;
    else return -1;
    return 0;
}

int mon_parse_columns(const char *value, uint32_t *columns) {
    if (!value || !*value || !columns || strlen(value) > 128U
        || value[0] == ',' || value[strlen(value) - 1U] == ','
        || strstr(value, ",,") != NULL) return -1;
    char buffer[129];
    (void)snprintf(buffer, sizeof(buffer), "%s", value);
    uint32_t parsed = 0U;
    size_t count = 0U;
    char *save = NULL;
    for (char *token = strtok_r(buffer, ",", &save); token;
         token = strtok_r(NULL, ",", &save)) {
        uint32_t flag = 0U;
        if (!*token || column_flag(token, &flag) != 0 || (parsed & flag) != 0U)
            return -1;
        parsed |= flag;
        count++;
    }
    if (count == 0U || (parsed & MON_COLUMN_NAME) == 0U
        || (parsed & ~MON_COLUMNS_ALL) != 0U) return -1;
    *columns = parsed;
    return 0;
}

int mon_parse_u64(const char *value, uint64_t minimum, uint64_t maximum,
                  uint64_t *result) {
    if (!value || !*value || !result || minimum > maximum) return -1;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++)
        if (!isdigit(*cursor)) return -1;
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(value, &end, 10);
    if (errno || end == value || *end || parsed < minimum || parsed > maximum)
        return -1;
    *result = (uint64_t)parsed;
    return 0;
}

int mon_parse_filter(const char *value, char output[MON_FILTER_MAX + 1U]) {
    if (!value || !output || strlen(value) > MON_FILTER_MAX) return -1;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++) {
        if (*cursor < 0x20U || *cursor > 0x7eU) return -1;
    }
    (void)snprintf(output, MON_FILTER_MAX + 1U, "%s", value);
    return 0;
}

const char *mon_sort_id(mon_sort sort) {
    switch (sort) {
        case MON_SORT_MEMORY: return "memory";
        case MON_SORT_READ: return "read";
        case MON_SORT_WRITE: return "write";
        case MON_SORT_NAME: return "name";
        case MON_SORT_PID: return "pid";
        case MON_SORT_CPU:
        default: return "cpu";
    }
}

const char *mon_group_id(mon_group group) {
    switch (group) {
        case MON_GROUP_NAME: return "name";
        case MON_GROUP_NONE: return "none";
        case MON_GROUP_CLASS:
        default: return "class";
    }
}

const char *mon_class_id(mon_process_class process_class) {
    switch (process_class) {
        case MON_CLASS_SYSTEM: return "system";
        case MON_CLASS_KERNEL: return "kernel";
        case MON_CLASS_APPLICATION:
        default: return "application";
    }
}

void mon_usage(FILE *output) {
    (void)fputs("Usage:\n"
          "  synapse-monitor snapshot [options]\n"
          "  synapse-monitor watch [options]\n\n"
          "Options:\n"
          "  --format text|json\n"
          "  --sort cpu|memory|read|write|name|pid\n"
          "  --group class|name|none\n"
          "  --columns name,pid,user,state,threads,cpu,memory,read,write\n"
          "  --filter TEXT                 printable ASCII, at most 64 bytes\n"
          "  --limit 1..512\n"
          "  --sample-ms 100..2000\n"
          "  --interval-ms 250..10000     watch only\n"
          "  --iterations 1..1000000      watch only; default is continuous on a TTY\n"
          "  --help\n"
          "  --version\n\n"
          "Interactive watch keys: q quit, / filter, s sort, g group, c columns.\n"
          "The command is read-only and exposes no process-control operation.\n",
          output);
}
