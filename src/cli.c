// SPDX-License-Identifier: MIT
#include "monitor.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    uint32_t flag;
} mon_column_definition;

int mon_parse_format(const char *value, mon_format *format) {
    if (!value || !format) return -1;
    if (strcmp(value, "text") == 0) *format = MON_FORMAT_TEXT;
    else if (strcmp(value, "json") == 0) *format = MON_FORMAT_JSON;
    else if (strcmp(value, "ndjson") == 0) *format = MON_FORMAT_NDJSON;
    else return -1;
    return 0;
}

int mon_parse_view(const char *value, mon_view *view) {
    if (!value || !view) return -1;
    if (strcmp(value, "processes") == 0) *view = MON_VIEW_PROCESSES;
    else if (strcmp(value, "performance") == 0) *view = MON_VIEW_PERFORMANCE;
    else if (strcmp(value, "services") == 0) *view = MON_VIEW_SERVICES;
    else if (strcmp(value, "startup") == 0) *view = MON_VIEW_STARTUP;
    else if (strcmp(value, "connections") == 0) *view = MON_VIEW_CONNECTIONS;
    else if (strcmp(value, "information") == 0) *view = MON_VIEW_INFORMATION;
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
    else if (strcmp(value, "status") == 0) *sort = MON_SORT_STATUS;
    else if (strcmp(value, "startup") == 0) *sort = MON_SORT_STARTUP;
    else if (strcmp(value, "protocol") == 0) *sort = MON_SORT_PROTOCOL;
    else if (strcmp(value, "local") == 0) *sort = MON_SORT_LOCAL;
    else if (strcmp(value, "remote") == 0) *sort = MON_SORT_REMOTE;
    else if (strcmp(value, "scope") == 0) *sort = MON_SORT_SCOPE;
    else if (strcmp(value, "type") == 0) *sort = MON_SORT_TYPE;
    else if (strcmp(value, "description") == 0) *sort = MON_SORT_DESCRIPTION;
    else if (strcmp(value, "user") == 0) *sort = MON_SORT_USER;
    else if (strcmp(value, "state") == 0) *sort = MON_SORT_STATE;
    else if (strcmp(value, "threads") == 0) *sort = MON_SORT_THREADS;
    else if (strcmp(value, "executable") == 0) *sort = MON_SORT_EXECUTABLE;
    else if (strcmp(value, "publisher") == 0) *sort = MON_SORT_PUBLISHER;
    else if (strcmp(value, "command") == 0) *sort = MON_SORT_COMMAND;
    else if (strcmp(value, "process") == 0) *sort = MON_SORT_PROCESS;
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

int mon_parse_theme(const char *value, mon_theme *theme) {
    if (!value || !theme) return -1;
    if (strcmp(value, "default") == 0) *theme = MON_THEME_DEFAULT;
    else if (strcmp(value, "contrast") == 0) *theme = MON_THEME_CONTRAST;
    else if (strcmp(value, "mono") == 0) *theme = MON_THEME_MONO;
    else return -1;
    return 0;
}

int mon_parse_layout(const char *value, mon_layout *layout) {
    if (!value || !layout) return -1;
    if (strcmp(value, "dense") == 0) *layout = MON_LAYOUT_DENSE;
    else if (strcmp(value, "balanced") == 0) *layout = MON_LAYOUT_BALANCED;
    else if (strcmp(value, "wide") == 0) *layout = MON_LAYOUT_WIDE;
    else return -1;
    return 0;
}

static int definitions_for_view(mon_view view,
                                const mon_column_definition **definitions,
                                size_t *count, uint32_t *mandatory,
                                uint32_t *allowed) {
    static const mon_column_definition process_columns[] = {
        {"name", MON_COLUMN_NAME}, {"pid", MON_COLUMN_PID},
        {"user", MON_COLUMN_USER}, {"state", MON_COLUMN_STATE},
        {"threads", MON_COLUMN_THREADS}, {"cpu", MON_COLUMN_CPU},
        {"memory", MON_COLUMN_MEMORY}, {"read", MON_COLUMN_READ},
        {"write", MON_COLUMN_WRITE}
    };
    static const mon_column_definition service_columns[] = {
        {"name", MON_SERVICE_COLUMN_NAME},
        {"description", MON_SERVICE_COLUMN_DESCRIPTION},
        {"status", MON_SERVICE_COLUMN_STATUS},
        {"startup", MON_SERVICE_COLUMN_STARTUP},
        {"pid", MON_SERVICE_COLUMN_PID}, {"user", MON_SERVICE_COLUMN_USER},
        {"executable", MON_SERVICE_COLUMN_EXECUTABLE}
    };
    static const mon_column_definition startup_columns[] = {
        {"name", MON_STARTUP_COLUMN_NAME},
        {"publisher", MON_STARTUP_COLUMN_PUBLISHER},
        {"status", MON_STARTUP_COLUMN_STATUS},
        {"type", MON_STARTUP_COLUMN_TYPE},
        {"location", MON_STARTUP_COLUMN_LOCATION},
        {"command", MON_STARTUP_COLUMN_COMMAND},
        {"scope", MON_STARTUP_COLUMN_SCOPE},
        {"launch", MON_STARTUP_COLUMN_LAUNCH}
    };
    static const mon_column_definition connection_columns[] = {
        {"protocol", MON_CONNECTION_COLUMN_PROTOCOL},
        {"local", MON_CONNECTION_COLUMN_LOCAL},
        {"remote", MON_CONNECTION_COLUMN_REMOTE},
        {"state", MON_CONNECTION_COLUMN_STATE},
        {"pid", MON_CONNECTION_COLUMN_PID},
        {"process", MON_CONNECTION_COLUMN_PROCESS}
    };
    if (!definitions || !count || !mandatory || !allowed) return -1;
    switch (view) {
        case MON_VIEW_PROCESSES:
            *definitions = process_columns;
            *count = sizeof(process_columns) / sizeof(process_columns[0]);
            *mandatory = MON_COLUMN_NAME;
            *allowed = MON_PROCESS_COLUMNS_ALL;
            return 0;
        case MON_VIEW_SERVICES:
            *definitions = service_columns;
            *count = sizeof(service_columns) / sizeof(service_columns[0]);
            *mandatory = MON_SERVICE_COLUMN_NAME;
            *allowed = MON_SERVICE_COLUMNS_ALL;
            return 0;
        case MON_VIEW_STARTUP:
            *definitions = startup_columns;
            *count = sizeof(startup_columns) / sizeof(startup_columns[0]);
            *mandatory = MON_STARTUP_COLUMN_NAME;
            *allowed = MON_STARTUP_COLUMNS_ALL;
            return 0;
        case MON_VIEW_CONNECTIONS:
            *definitions = connection_columns;
            *count = sizeof(connection_columns) / sizeof(connection_columns[0]);
            *mandatory = MON_CONNECTION_COLUMN_PROTOCOL | MON_CONNECTION_COLUMN_LOCAL;
            *allowed = MON_CONNECTION_COLUMNS_ALL;
            return 0;
        case MON_VIEW_PERFORMANCE:
        case MON_VIEW_INFORMATION:
        case MON_VIEW_COUNT:
        default: return -1;
    }
}

int mon_parse_columns_for_view(mon_view view, const char *value,
                               uint32_t *columns) {
    if (!value || !*value || !columns || strlen(value) > 255U
        || value[0] == ',' || value[strlen(value) - 1U] == ','
        || strstr(value, ",,") != NULL) return -1;
    const mon_column_definition *definitions = NULL;
    size_t definition_count = 0U;
    uint32_t mandatory = 0U;
    uint32_t allowed = 0U;
    if (definitions_for_view(view, &definitions, &definition_count, &mandatory,
                             &allowed) != 0) return -1;
    char buffer[256];
    (void)snprintf(buffer, sizeof(buffer), "%s", value);
    uint32_t parsed = 0U;
    size_t count = 0U;
    char *save = NULL;
    for (char *token = strtok_r(buffer, ",", &save); token;
         token = strtok_r(NULL, ",", &save)) {
        uint32_t flag = 0U;
        for (size_t i = 0U; i < definition_count; i++) {
            if (strcmp(token, definitions[i].name) == 0) {
                flag = definitions[i].flag;
                break;
            }
        }
        if (flag == 0U || (parsed & flag) != 0U) return -1;
        parsed |= flag;
        count++;
    }
    if (count == 0U || (parsed & mandatory) != mandatory
        || (parsed & ~allowed) != 0U) return -1;
    *columns = parsed;
    return 0;
}

int mon_parse_columns(const char *value, uint32_t *columns) {
    return mon_parse_columns_for_view(MON_VIEW_PROCESSES, value, columns);
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

uint32_t mon_default_columns(mon_view view) {
    switch (view) {
        case MON_VIEW_SERVICES: return MON_SERVICE_COLUMNS_DEFAULT;
        case MON_VIEW_STARTUP: return MON_STARTUP_COLUMNS_DEFAULT;
        case MON_VIEW_CONNECTIONS: return MON_CONNECTION_COLUMNS_DEFAULT;
        case MON_VIEW_PROCESSES: return MON_PROCESS_COLUMNS_DEFAULT;
        case MON_VIEW_PERFORMANCE:
        case MON_VIEW_INFORMATION:
        case MON_VIEW_COUNT:
        default: return 0U;
    }
}

mon_sort mon_default_sort(mon_view view) {
    switch (view) {
        case MON_VIEW_SERVICES:
        case MON_VIEW_STARTUP: return MON_SORT_NAME;
        case MON_VIEW_CONNECTIONS: return MON_SORT_PROTOCOL;
        case MON_VIEW_PROCESSES: return MON_SORT_CPU;
        case MON_VIEW_PERFORMANCE:
        case MON_VIEW_INFORMATION:
        case MON_VIEW_COUNT:
        default: return MON_SORT_NAME;
    }
}

const char *mon_view_id(mon_view view) {
    switch (view) {
        case MON_VIEW_PERFORMANCE: return "performance";
        case MON_VIEW_SERVICES: return "services";
        case MON_VIEW_STARTUP: return "startup";
        case MON_VIEW_CONNECTIONS: return "connections";
        case MON_VIEW_INFORMATION: return "information";
        case MON_VIEW_PROCESSES:
        default: return "processes";
    }
}

const char *mon_sort_id(mon_sort sort) {
    switch (sort) {
        case MON_SORT_MEMORY: return "memory";
        case MON_SORT_READ: return "read";
        case MON_SORT_WRITE: return "write";
        case MON_SORT_NAME: return "name";
        case MON_SORT_PID: return "pid";
        case MON_SORT_STATUS: return "status";
        case MON_SORT_STARTUP: return "startup";
        case MON_SORT_PROTOCOL: return "protocol";
        case MON_SORT_LOCAL: return "local";
        case MON_SORT_REMOTE: return "remote";
        case MON_SORT_SCOPE: return "scope";
        case MON_SORT_TYPE: return "type";
        case MON_SORT_DESCRIPTION: return "description";
        case MON_SORT_USER: return "user";
        case MON_SORT_STATE: return "state";
        case MON_SORT_THREADS: return "threads";
        case MON_SORT_EXECUTABLE: return "executable";
        case MON_SORT_PUBLISHER: return "publisher";
        case MON_SORT_COMMAND: return "command";
        case MON_SORT_PROCESS: return "process";
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

const char *mon_theme_id(mon_theme theme) {
    switch (theme) {
        case MON_THEME_CONTRAST: return "contrast";
        case MON_THEME_MONO: return "mono";
        case MON_THEME_DEFAULT:
        default: return "default";
    }
}

const char *mon_layout_id(mon_layout layout) {
    switch (layout) {
        case MON_LAYOUT_DENSE: return "dense";
        case MON_LAYOUT_WIDE: return "wide";
        case MON_LAYOUT_BALANCED:
        default: return "balanced";
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

const char *mon_thermal_class_id(mon_thermal_class sensor_class) {
    switch (sensor_class) {
        case MON_THERMAL_CPU_PACKAGE: return "cpu-package";
        case MON_THERMAL_CPU_CORE: return "cpu-core";
        case MON_THERMAL_GPU: return "gpu";
        case MON_THERMAL_STORAGE: return "storage";
        case MON_THERMAL_BATTERY: return "battery";
        case MON_THERMAL_SYSTEM: return "system";
        case MON_THERMAL_OTHER:
        default: return "other";
    }
}

void mon_usage(FILE *output) {
    (void)fputs(
        "Usage:\n"
        "  synapse-monitor snapshot [options]\n"
        "  synapse-monitor stream [options]\n"
        "  synapse-monitor watch [options]\n"
        "  synapse-monitor describe [--format json]\n"
        "  synapse-monitor inspect --pid PID [--format text|json]\n\n"
        "Views:\n"
        "  processes | performance | services | startup | connections | information\n\n"
        "Options:\n"
        "  --view VIEW\n"
        "  --format text|json|ndjson     ndjson is stream-only\n"
        "  --sort ID                     reviewed view-appropriate column ID\n"
        "  --group class|name|none       processes only\n"
        "  --columns LIST               reviewed columns for the selected table\n"
        "  --theme default|contrast|mono\n"
        "  --layout dense|balanced|wide\n"
        "  --filter TEXT                 printable ASCII, at most 64 bytes\n"
        "  --limit 1..512\n"
        "  --sample-ms 100..2000\n"
        "  --interval-ms 250..10000     watch or stream\n"
        "  --iterations 1..1000000      watch or stream; stream defaults continuous\n"
        "  --help\n"
        "  --version\n\n"
        "Describe reports GUI-safe capabilities. Stream emits one complete,\n"
        "locale-neutral JSON object per line with bounded 60-sample history.\n"
        "Performance reports bounded GPU parameters, temperatures and fans with\n"
        "explicit unavailable values and no integrated-GPU temperature inference.\n"
        "Process inspection reports bounded module basenames, Linux credentials,\n"
        "capability masks, seccomp state and descriptor counts without paths.\n\n"
        "Interactive: 1-6 views, Tab next view, type or / to filter, S sort,\n"
        "G group, C columns, L layout, T theme, Q quit.\n"
        "The command is read-only; all views are inspection-only.\n"
        "Raw paths and launch commands are not exposed.\n",
        output);
}
