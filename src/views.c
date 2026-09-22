// SPDX-License-Identifier: MIT
#include "monitor.h"

#include <inttypes.h>
#include <string.h>

static bool colored(const mon_options *options) {
    return options && options->interactive_output && options->theme != MON_THEME_MONO;
}

static const char *accent(const mon_options *options) {
    if (!colored(options)) return "";
    return options->theme == MON_THEME_CONTRAST ? "\033[1;93m" : "\033[1;36m";
}

static const char *strong(const mon_options *options) {
    if (!colored(options)) return "";
    return options->theme == MON_THEME_CONTRAST ? "\033[1;97m" : "\033[1m";
}

static const char *muted(const mon_options *options) {
    return colored(options) ? "\033[2m" : "";
}

static const char *reset(const mon_options *options) {
    return colored(options) ? "\033[0m" : "";
}

static void render_tabs(const mon_options *options) {
    static const char *titles[] = {
        "1 Processes", "2 Performance", "3 Services", "4 Startup",
        "5 Connections", "6 Information"
    };
    for (size_t i = 0U; i < sizeof(titles) / sizeof(titles[0]); i++) {
        if ((mon_view)i == options->view)
            printf("%s[%s]%s", accent(options), titles[i], reset(options));
        else printf(" %s ", titles[i]);
        if (i + 1U < sizeof(titles) / sizeof(titles[0])) fputs("  ", stdout);
    }
    fputc('\n', stdout);
}

static void json_string(const char *value) {
    fputc('"', stdout);
    if (!value) {
        fputc('"', stdout);
        return;
    }
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor != '\0'; cursor++) {
        switch (*cursor) {
            case '"': fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\b': fputs("\\b", stdout); break;
            case '\f': fputs("\\f", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:
                if (*cursor < 0x20U) printf("\\u%04x", (unsigned)*cursor);
                else fputc((int)*cursor, stdout);
                break;
        }
    }
    fputc('"', stdout);
}

static void json_selection(const mon_options *options) {
    fputs("\"selection\":{\"sort\":", stdout);
    json_string(mon_sort_id(options->sort));
    fputs(",\"filter\":", stdout);
    json_string(options->filter);
    printf(",\"limit\":%zu,\"theme\":", options->limit);
    json_string(mon_theme_id(options->theme));
    fputs(",\"layout\":", stdout);
    json_string(mon_layout_id(options->layout));
    fputc('}', stdout);
}

static void json_columns(mon_view view, uint32_t columns) {
    struct definition { uint32_t flag; const char *name; } process[] = {
        {MON_COLUMN_NAME, "name"}, {MON_COLUMN_PID, "pid"},
        {MON_COLUMN_USER, "user"}, {MON_COLUMN_STATE, "state"},
        {MON_COLUMN_THREADS, "threads"}, {MON_COLUMN_CPU, "cpu"},
        {MON_COLUMN_MEMORY, "memory"}, {MON_COLUMN_READ, "read"},
        {MON_COLUMN_WRITE, "write"}
    }, services[] = {
        {MON_SERVICE_COLUMN_NAME, "name"},
        {MON_SERVICE_COLUMN_DESCRIPTION, "description"},
        {MON_SERVICE_COLUMN_STATUS, "status"},
        {MON_SERVICE_COLUMN_STARTUP, "startup"},
        {MON_SERVICE_COLUMN_PID, "pid"}, {MON_SERVICE_COLUMN_USER, "user"},
        {MON_SERVICE_COLUMN_EXECUTABLE, "executable"}
    }, startup[] = {
        {MON_STARTUP_COLUMN_NAME, "name"},
        {MON_STARTUP_COLUMN_PUBLISHER, "publisher"},
        {MON_STARTUP_COLUMN_STATUS, "status"},
        {MON_STARTUP_COLUMN_TYPE, "type"},
        {MON_STARTUP_COLUMN_LOCATION, "location"},
        {MON_STARTUP_COLUMN_COMMAND, "command"}
    }, connections[] = {
        {MON_CONNECTION_COLUMN_PROTOCOL, "protocol"},
        {MON_CONNECTION_COLUMN_LOCAL, "local"},
        {MON_CONNECTION_COLUMN_REMOTE, "remote"},
        {MON_CONNECTION_COLUMN_STATE, "state"},
        {MON_CONNECTION_COLUMN_PID, "pid"},
        {MON_CONNECTION_COLUMN_PROCESS, "process"}
    };
    const struct definition *values = process;
    size_t count = sizeof(process) / sizeof(process[0]);
    if (view == MON_VIEW_SERVICES) {
        values = services;
        count = sizeof(services) / sizeof(services[0]);
    } else if (view == MON_VIEW_STARTUP) {
        values = startup;
        count = sizeof(startup) / sizeof(startup[0]);
    } else if (view == MON_VIEW_CONNECTIONS) {
        values = connections;
        count = sizeof(connections) / sizeof(connections[0]);
    }
    fputc('[', stdout);
    bool first = true;
    for (size_t i = 0U; i < count; i++) {
        if ((columns & values[i].flag) == 0U) continue;
        if (!first) fputc(',', stdout);
        json_string(values[i].name);
        first = false;
    }
    fputc(']', stdout);
}

static int service_name_width(const mon_options *options) {
    return options->layout == MON_LAYOUT_DENSE ? 22
        : (options->layout == MON_LAYOUT_WIDE ? 36 : 28);
}

static int service_description_width(const mon_options *options) {
    return options->layout == MON_LAYOUT_DENSE ? 28
        : (options->layout == MON_LAYOUT_WIDE ? 52 : 38);
}

static int render_services_text(const mon_service_snapshot *snapshot,
                                const mon_options *options) {
    render_tabs(options);
    printf("%sSERVICES%s  read-only  Filter: %s  |  Sort: %s  |  Rows: %zu/%zu\n\n",
           strong(options), reset(options),
           options->filter[0] ? options->filter : "(none)",
           mon_sort_id(options->sort), snapshot->count, snapshot->matched);
    int name_width = service_name_width(options);
    int description_width = service_description_width(options);
    if (options->columns & MON_SERVICE_COLUMN_NAME)
        printf("%-*s", name_width, "NAME");
    if (options->columns & MON_SERVICE_COLUMN_DESCRIPTION)
        printf(" %-*s", description_width, "DESCRIPTION");
    if (options->columns & MON_SERVICE_COLUMN_STATUS) printf(" %-9s", "STATUS");
    if (options->columns & MON_SERVICE_COLUMN_STARTUP) printf(" %-9s", "STARTUP");
    if (options->columns & MON_SERVICE_COLUMN_PID) printf(" %7s", "PID");
    if (options->columns & MON_SERVICE_COLUMN_USER) printf(" %-14s", "USER");
    if (options->columns & MON_SERVICE_COLUMN_EXECUTABLE)
        printf(" %-24s", "EXECUTABLE");
    fputc('\n', stdout);
    for (size_t i = 0U; i < snapshot->count; i++) {
        const mon_service *row = &snapshot->rows[i];
        if (options->columns & MON_SERVICE_COLUMN_NAME)
            printf("%-*.*s", name_width, name_width, row->name);
        if (options->columns & MON_SERVICE_COLUMN_DESCRIPTION)
            printf(" %-*.*s", description_width, description_width,
                   row->description);
        if (options->columns & MON_SERVICE_COLUMN_STATUS)
            printf(" %-9.9s", row->status);
        if (options->columns & MON_SERVICE_COLUMN_STARTUP)
            printf(" %-9.9s", row->startup);
        if (options->columns & MON_SERVICE_COLUMN_PID) {
            if (row->pid_available) printf(" %7d", row->pid);
            else printf(" %7s", "-");
        }
        if (options->columns & MON_SERVICE_COLUMN_USER)
            printf(" %-14.14s", row->user);
        if (options->columns & MON_SERVICE_COLUMN_EXECUTABLE)
            printf(" %-24.24s", row->executable);
        fputc('\n', stdout);
    }
    printf("\n%sCoverage: files=%zu denied=%zu malformed=%zu truncated=%s%s\n",
           muted(options), snapshot->files_seen, snapshot->denied,
           snapshot->malformed, snapshot->truncated ? "true" : "false",
           reset(options));
    fputs("Executable paths and service mutation are not exposed.\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

static int render_services_json(const mon_service_snapshot *snapshot,
                                const mon_options *options) {
    fputs("{\"schema\":\"synapse.monitor.services/v1\",\"readOnly\":true,\"view\":\"services\"", stdout);
    mon_render_stream_metadata(options);
    fputc(',', stdout);
    json_selection(options);
    fputs(",\"columns\":", stdout);
    json_columns(MON_VIEW_SERVICES, options->columns);
    printf(",\"coverage\":{\"filesSeen\":%zu,\"rowsMatched\":%zu,"
           "\"rowsReturned\":%zu,\"permissionDenied\":%zu,"
           "\"malformed\":%zu,\"truncated\":%s},\"rows\":[",
           snapshot->files_seen, snapshot->matched, snapshot->count,
           snapshot->denied, snapshot->malformed,
           snapshot->truncated ? "true" : "false");
    for (size_t i = 0U; i < snapshot->count; i++) {
        const mon_service *row = &snapshot->rows[i];
        if (i > 0U) fputc(',', stdout);
        fputs("{\"name\":", stdout); json_string(row->name);
        fputs(",\"description\":", stdout); json_string(row->description);
        fputs(",\"status\":", stdout); json_string(row->status);
        fputs(",\"startup\":", stdout); json_string(row->startup);
        fputs(",\"pid\":", stdout);
        if (row->pid_available) printf("%d", row->pid); else fputs("null", stdout);
        fputs(",\"user\":", stdout); json_string(row->user);
        fputs(",\"executable\":", stdout); json_string(row->executable);
        fputc('}', stdout);
    }
    fputs("],\"semantics\":{\"serviceMutation\":false,"
          "\"pathsExposed\":false,\"unitFileContentExposed\":false}}\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

int mon_render_services(const mon_service_snapshot *snapshot,
                        const mon_options *options) {
    if (!snapshot || !options) return 1;
    return options->format == MON_FORMAT_JSON
        ? render_services_json(snapshot, options)
        : render_services_text(snapshot, options);
}

static int render_startup_text(const mon_startup_snapshot *snapshot,
                               const mon_options *options) {
    render_tabs(options);
    printf("%sSTARTUP APPS%s  read-only  Filter: %s  |  Sort: %s  |  Rows: %zu/%zu\n\n",
           strong(options), reset(options),
           options->filter[0] ? options->filter : "(none)",
           mon_sort_id(options->sort), snapshot->count, snapshot->matched);
    int name_width = options->layout == MON_LAYOUT_DENSE ? 24
        : (options->layout == MON_LAYOUT_WIDE ? 42 : 32);
    int publisher_width = options->layout == MON_LAYOUT_DENSE ? 18
        : (options->layout == MON_LAYOUT_WIDE ? 30 : 22);
    if (options->columns & MON_STARTUP_COLUMN_NAME)
        printf("%-*s", name_width, "NAME");
    if (options->columns & MON_STARTUP_COLUMN_PUBLISHER)
        printf(" %-*s", publisher_width, "PUBLISHER");
    if (options->columns & MON_STARTUP_COLUMN_STATUS) printf(" %-9s", "STATUS");
    if (options->columns & MON_STARTUP_COLUMN_TYPE) printf(" %-9s", "TYPE");
    if (options->columns & MON_STARTUP_COLUMN_LOCATION)
        printf(" %-16s", "LOCATION");
    if (options->columns & MON_STARTUP_COLUMN_COMMAND)
        printf(" %-30s", "COMMAND");
    fputc('\n', stdout);
    for (size_t i = 0U; i < snapshot->count; i++) {
        const mon_startup_item *row = &snapshot->rows[i];
        if (options->columns & MON_STARTUP_COLUMN_NAME)
            printf("%-*.*s", name_width, name_width, row->name);
        if (options->columns & MON_STARTUP_COLUMN_PUBLISHER)
            printf(" %-*.*s", publisher_width, publisher_width, row->publisher);
        if (options->columns & MON_STARTUP_COLUMN_STATUS)
            printf(" %-9.9s", row->status);
        if (options->columns & MON_STARTUP_COLUMN_TYPE)
            printf(" %-9.9s", row->type);
        if (options->columns & MON_STARTUP_COLUMN_LOCATION)
            printf(" %-16.16s", row->location);
        if (options->columns & MON_STARTUP_COLUMN_COMMAND)
            printf(" %-30.30s", row->command);
        fputc('\n', stdout);
    }
    printf("\n%sCoverage: files=%zu denied=%zu malformed=%zu truncated=%s%s\n",
           muted(options), snapshot->files_seen, snapshot->denied,
           snapshot->malformed, snapshot->truncated ? "true" : "false",
           reset(options));
    fputs("Command arguments and full locations are redacted; startup mutation is absent.\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

static int render_startup_json(const mon_startup_snapshot *snapshot,
                               const mon_options *options) {
    fputs("{\"schema\":\"synapse.monitor.startup/v1\",\"readOnly\":true,\"view\":\"startup\"", stdout);
    mon_render_stream_metadata(options);
    fputc(',', stdout);
    json_selection(options);
    fputs(",\"columns\":", stdout);
    json_columns(MON_VIEW_STARTUP, options->columns);
    printf(",\"coverage\":{\"filesSeen\":%zu,\"rowsMatched\":%zu,"
           "\"rowsReturned\":%zu,\"permissionDenied\":%zu,"
           "\"malformed\":%zu,\"truncated\":%s},\"rows\":[",
           snapshot->files_seen, snapshot->matched, snapshot->count,
           snapshot->denied, snapshot->malformed,
           snapshot->truncated ? "true" : "false");
    for (size_t i = 0U; i < snapshot->count; i++) {
        const mon_startup_item *row = &snapshot->rows[i];
        if (i > 0U) fputc(',', stdout);
        fputs("{\"id\":", stdout); json_string(row->id);
        fputs(",\"name\":", stdout); json_string(row->name);
        fputs(",\"publisher\":", stdout); json_string(row->publisher);
        fputs(",\"status\":", stdout); json_string(row->status);
        fputs(",\"type\":", stdout); json_string(row->type);
        fputs(",\"scope\":", stdout); json_string(row->scope);
        fputs(",\"location\":", stdout); json_string(row->location);
        fputs(",\"command\":", stdout); json_string(row->command);
        printf(",\"launchPresent\":%s}", row->launch_present ? "true" : "false");
    }
    fputs("],\"semantics\":{\"startupMutation\":false,"
          "\"rawLaunchCommandsExposed\":false,\"fullLocationsExposed\":false}}\n",
          stdout);
    return ferror(stdout) ? 1 : 0;
}

int mon_render_startup(const mon_startup_snapshot *snapshot,
                       const mon_options *options) {
    if (!snapshot || !options) return 1;
    return options->format == MON_FORMAT_JSON
        ? render_startup_json(snapshot, options)
        : render_startup_text(snapshot, options);
}

static int render_connections_text(const mon_connection_snapshot *snapshot,
                                   const mon_options *options) {
    render_tabs(options);
    printf("%sCONNECTIONS%s  read-only  Filter: %s  |  Sort: %s  |  Rows: %zu/%zu\n\n",
           strong(options), reset(options),
           options->filter[0] ? options->filter : "(none)",
           mon_sort_id(options->sort), snapshot->count, snapshot->matched);
    int endpoint_width = options->layout == MON_LAYOUT_DENSE ? 27
        : (options->layout == MON_LAYOUT_WIDE ? 45 : 37);
    int process_width = options->layout == MON_LAYOUT_DENSE ? 16
        : (options->layout == MON_LAYOUT_WIDE ? 28 : 22);
    if (options->columns & MON_CONNECTION_COLUMN_PROTOCOL) printf("%-5s", "PROTO");
    if (options->columns & MON_CONNECTION_COLUMN_LOCAL)
        printf(" %-*s", endpoint_width, "LOCAL");
    if (options->columns & MON_CONNECTION_COLUMN_REMOTE)
        printf(" %-*s", endpoint_width, "REMOTE");
    if (options->columns & MON_CONNECTION_COLUMN_STATE) printf(" %-16s", "STATE");
    if (options->columns & MON_CONNECTION_COLUMN_PID) printf(" %7s", "PID");
    if (options->columns & MON_CONNECTION_COLUMN_PROCESS)
        printf(" %-*s", process_width, "PROCESS");
    fputc('\n', stdout);
    for (size_t i = 0U; i < snapshot->count; i++) {
        const mon_connection *row = &snapshot->rows[i];
        if (options->columns & MON_CONNECTION_COLUMN_PROTOCOL)
            printf("%-5.5s", row->protocol);
        if (options->columns & MON_CONNECTION_COLUMN_LOCAL)
            printf(" %-*.*s", endpoint_width, endpoint_width, row->local);
        if (options->columns & MON_CONNECTION_COLUMN_REMOTE)
            printf(" %-*.*s", endpoint_width, endpoint_width, row->remote);
        if (options->columns & MON_CONNECTION_COLUMN_STATE)
            printf(" %-16.16s", row->state);
        if (options->columns & MON_CONNECTION_COLUMN_PID) {
            if (row->pid_available) printf(" %7d", row->pid);
            else printf(" %7s", "-");
        }
        if (options->columns & MON_CONNECTION_COLUMN_PROCESS)
            printf(" %-*.*s", process_width, process_width,
                   row->pid_available ? row->process : "unavailable");
        fputc('\n', stdout);
    }
    printf("\n%sCoverage: rows=%zu denied=%zu malformed=%zu identity-unavailable=%zu "
           "owner-fds=%zu truncated=%s owner-truncated=%s%s\n",
           muted(options), snapshot->rows_seen, snapshot->denied,
           snapshot->malformed, snapshot->identity_unavailable,
           snapshot->owner_fds_seen,
           snapshot->truncated ? "true" : "false",
           snapshot->owner_scan_truncated ? "true" : "false", reset(options));
    fputs("Endpoints are observed locally; no socket is opened and no connection control exists.\n",
          stdout);
    return ferror(stdout) ? 1 : 0;
}

static int render_connections_json(const mon_connection_snapshot *snapshot,
                                   const mon_options *options) {
    fputs("{\"schema\":\"synapse.monitor.connections/v1\",\"readOnly\":true,\"view\":\"connections\"", stdout);
    mon_render_stream_metadata(options);
    fputc(',', stdout);
    json_selection(options);
    fputs(",\"columns\":", stdout);
    json_columns(MON_VIEW_CONNECTIONS, options->columns);
    printf(",\"coverage\":{\"rowsSeen\":%zu,\"rowsMatched\":%zu,"
           "\"rowsReturned\":%zu,\"permissionDenied\":%zu,"
           "\"malformed\":%zu,\"identityUnavailable\":%zu,"
           "\"ownerFdsSeen\":%zu,\"truncated\":%s,"
           "\"ownerScanTruncated\":%s},\"rows\":[",
           snapshot->rows_seen, snapshot->matched, snapshot->count,
           snapshot->denied, snapshot->malformed,
           snapshot->identity_unavailable, snapshot->owner_fds_seen,
           snapshot->truncated ? "true" : "false",
           snapshot->owner_scan_truncated ? "true" : "false");
    for (size_t i = 0U; i < snapshot->count; i++) {
        const mon_connection *row = &snapshot->rows[i];
        if (i > 0U) fputc(',', stdout);
        fputs("{\"protocol\":", stdout); json_string(row->protocol);
        printf(",\"socketInode\":%" PRIu64, row->inode);
        fputs(",\"local\":", stdout); json_string(row->local);
        fputs(",\"remote\":", stdout); json_string(row->remote);
        fputs(",\"state\":", stdout); json_string(row->state);
        fputs(",\"pid\":", stdout);
        if (row->pid_available) printf("%d", row->pid); else fputs("null", stdout);
        fputs(",\"process\":", stdout);
        if (row->pid_available) json_string(row->process); else fputs("null", stdout);
        fputc('}', stdout);
    }
    fputs("],\"semantics\":{\"socketOpened\":false,"
          "\"connectionControl\":false,\"endpointScope\":\"local-session\"}}\n",
          stdout);
    return ferror(stdout) ? 1 : 0;
}

int mon_render_connections(const mon_connection_snapshot *snapshot,
                           const mon_options *options) {
    if (!snapshot || !options) return 1;
    return options->format == MON_FORMAT_JSON
        ? render_connections_json(snapshot, options)
        : render_connections_text(snapshot, options);
}

static const char *capability_or_unavailable(const char *value) {
    return value && value[0] ? value : "unavailable";
}

static int render_process_inspection_text(
    const mon_process_inspection *inspection) {
    printf("SYNAPSE MONITOR  PROCESS INSPECTION  read-only\n\n");
    printf("%-22s %s\n", "Name", inspection->name);
    printf("%-22s %d\n", "PID", inspection->pid);
    printf("%-22s %" PRIu64 "\n", "Start ticks", inspection->start_ticks);
    if (inspection->credentials_available) {
        printf("%-22s %u %u %u %u\n", "UID real/effective/saved/fs",
               inspection->uids[0], inspection->uids[1], inspection->uids[2],
               inspection->uids[3]);
        printf("%-22s %u %u %u %u\n", "GID real/effective/saved/fs",
               inspection->gids[0], inspection->gids[1], inspection->gids[2],
               inspection->gids[3]);
    } else fputs("Credentials            unavailable\n", stdout);
    printf("%-22s %s\n", "Capabilities inherited",
           capability_or_unavailable(inspection->capability_inheritable));
    printf("%-22s %s\n", "Capabilities permitted",
           capability_or_unavailable(inspection->capability_permitted));
    printf("%-22s %s\n", "Capabilities effective",
           capability_or_unavailable(inspection->capability_effective));
    printf("%-22s %s\n", "Capabilities bounding",
           capability_or_unavailable(inspection->capability_bounding));
    printf("%-22s %s\n", "Capabilities ambient",
           capability_or_unavailable(inspection->capability_ambient));
    if (inspection->no_new_privileges_available)
        printf("%-22s %s\n", "No new privileges",
               inspection->no_new_privileges ? "true" : "false");
    else printf("%-22s unavailable\n", "No new privileges");
    if (inspection->seccomp_available)
        printf("%-22s %u\n", "Seccomp mode", inspection->seccomp_mode);
    else printf("%-22s unavailable\n", "Seccomp mode");
    printf("%-22s %zu%s\n", "Open descriptors", inspection->fd_count,
           inspection->fd_truncated ? "+" : "");
    printf("%-22s %zu\n", "Socket descriptors", inspection->socket_count);
    printf("\nMODULE BASENAMES (%zu unique, %zu mappings)\n",
           inspection->module_count, inspection->module_rows_seen);
    if (inspection->module_denied) fputs("unavailable: permission denied\n", stdout);
    else if (inspection->module_count == 0U) fputs("none observed\n", stdout);
    else {
        for (size_t i = 0U; i < inspection->module_count; i++)
            printf("%-32.32s%s", inspection->modules[i],
                   (i + 1U) % 3U == 0U || i + 1U == inspection->module_count
                   ? "\n" : "  ");
    }
    printf("\nCoverage: modules-truncated=%s fd-truncated=%s "
           "module-denied=%s fd-denied=%s\n",
           inspection->module_truncated ? "true" : "false",
           inspection->fd_truncated ? "true" : "false",
           inspection->module_denied ? "true" : "false",
           inspection->fd_denied ? "true" : "false");
    fputs("Module paths, descriptor targets, commands, authentication tokens and process dumps are not exposed.\n",
          stdout);
    return ferror(stdout) ? 1 : 0;
}

static void json_capability(const char *name, const char *value) {
    printf(",\"%s\":", name);
    if (value && value[0]) json_string(value);
    else fputs("null", stdout);
}

static int render_process_inspection_json(
    const mon_process_inspection *inspection) {
    fputs("{\"schema\":\"synapse.monitor.process-inspection/v1\","
          "\"readOnly\":true,\"identity\":{\"pid\":", stdout);
    printf("%d,\"startTicks\":%" PRIu64 ",\"name\":",
           inspection->pid, inspection->start_ticks);
    json_string(inspection->name);
    printf("},\"credentials\":{\"available\":%s,\"uids\":[",
           inspection->credentials_available ? "true" : "false");
    for (size_t i = 0U; i < 4U; i++) {
        if (i > 0U) fputc(',', stdout);
        if (inspection->credentials_available) printf("%u", inspection->uids[i]);
        else fputs("null", stdout);
    }
    fputs("],\"gids\":[", stdout);
    for (size_t i = 0U; i < 4U; i++) {
        if (i > 0U) fputc(',', stdout);
        if (inspection->credentials_available) printf("%u", inspection->gids[i]);
        else fputs("null", stdout);
    }
    fputs("],\"capabilities\":{", stdout);
    fputs("\"inheritable\":", stdout);
    if (inspection->capability_inheritable[0])
        json_string(inspection->capability_inheritable); else fputs("null", stdout);
    json_capability("permitted", inspection->capability_permitted);
    json_capability("effective", inspection->capability_effective);
    json_capability("bounding", inspection->capability_bounding);
    json_capability("ambient", inspection->capability_ambient);
    fputs("},\"noNewPrivileges\":", stdout);
    if (inspection->no_new_privileges_available)
        fputs(inspection->no_new_privileges ? "true" : "false", stdout);
    else fputs("null", stdout);
    fputs(",\"seccompMode\":", stdout);
    if (inspection->seccomp_available) printf("%u", inspection->seccomp_mode);
    else fputs("null", stdout);
    printf("},\"modules\":{\"mappingsSeen\":%zu,\"truncated\":%s,"
           "\"permissionDenied\":%s,\"names\":[",
           inspection->module_rows_seen,
           inspection->module_truncated ? "true" : "false",
           inspection->module_denied ? "true" : "false");
    for (size_t i = 0U; i < inspection->module_count; i++) {
        if (i > 0U) fputc(',', stdout);
        json_string(inspection->modules[i]);
    }
    printf("],\"pathsExposed\":false},\"descriptors\":{\"count\":%zu,"
           "\"socketCount\":%zu,\"truncated\":%s,"
           "\"permissionDenied\":%s,\"targetsExposed\":false},"
           "\"semantics\":{\"processControl\":false,\"processDump\":false,"
           "\"commandLineExposed\":false,\"environmentExposed\":false}}\n",
           inspection->fd_count, inspection->socket_count,
           inspection->fd_truncated ? "true" : "false",
           inspection->fd_denied ? "true" : "false");
    return ferror(stdout) ? 1 : 0;
}

int mon_render_process_inspection(const mon_process_inspection *inspection,
                                  mon_format format) {
    if (!inspection) return 1;
    return format == MON_FORMAT_JSON
        ? render_process_inspection_json(inspection)
        : render_process_inspection_text(inspection);
}

static void format_iec(uint64_t bytes, char *output, size_t output_size) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    long double value = (long double)bytes;
    size_t unit = 0U;
    while (value >= 1024.0L && unit + 1U < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0L;
        unit++;
    }
    if (unit == 0U)
        snprintf(output, output_size, "%" PRIu64 " %s", bytes, units[unit]);
    else snprintf(output, output_size, "%.1Lf %s", value, units[unit]);
}

static const char *available_text(const char *value) {
    return value && value[0] ? value : "unavailable";
}

static int render_information_text(const mon_information *information,
                                   const mon_options *options) {
    render_tabs(options);
    printf("%sSYSTEM INFORMATION%s  read-only\n\n",
           strong(options), reset(options));
    char memory[32] = "unavailable";
    if (information->memory_available)
        format_iec(information->memory_total_bytes, memory, sizeof(memory));
    char uptime[64] = "unavailable";
    if (information->uptime_available) {
        uint64_t days = information->uptime_seconds / 86400U;
        uint64_t hours = (information->uptime_seconds % 86400U) / 3600U;
        uint64_t minutes = (information->uptime_seconds % 3600U) / 60U;
        (void)snprintf(uptime, sizeof(uptime), "%" PRIu64 "d %" PRIu64
                       "h %" PRIu64 "m", days, hours, minutes);
    }
    printf("%-20s %s\n", "Operating system", available_text(information->operating_system));
    printf("%-20s %s\n", "Kernel", available_text(information->kernel));
    printf("%-20s %s\n", "Architecture", available_text(information->architecture));
    printf("%-20s %s\n", "Processor", available_text(information->processor));
    printf("%-20s %s\n", "System vendor", available_text(information->system_vendor));
    printf("%-20s %s\n", "System model", available_text(information->system_model));
    printf("%-20s %s\n", "Physical memory", memory);
    printf("%-20s %s\n", "Firmware vendor", available_text(information->firmware_vendor));
    printf("%-20s %s\n", "Firmware version", available_text(information->firmware_version));
    printf("%-20s %s\n", "Firmware date", available_text(information->firmware_date));
    printf("%-20s %s\n", "Uptime", uptime);
    printf("\n%sCoverage: unavailable-fields=%zu%s\n",
           muted(options), information->unavailable_fields, reset(options));
    fputs("Host names, serial numbers, paths and credentials are not exposed.\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

static void json_nullable_string(const char *name, const char *value,
                                 bool comma) {
    printf("%s\"%s\":", comma ? "," : "", name);
    if (value && value[0]) json_string(value);
    else fputs("null", stdout);
}

static int render_information_json(const mon_information *information,
                                   const mon_options *options) {
    fputs("{\"schema\":\"synapse.monitor.information/v1\","
          "\"readOnly\":true,\"view\":\"information\"", stdout);
    mon_render_stream_metadata(options);
    fputc(',', stdout);
    json_selection(options);
    fputs(",\"information\":{", stdout);
    json_nullable_string("operatingSystem", information->operating_system, false);
    json_nullable_string("kernel", information->kernel, true);
    json_nullable_string("architecture", information->architecture, true);
    json_nullable_string("processor", information->processor, true);
    json_nullable_string("systemVendor", information->system_vendor, true);
    json_nullable_string("systemModel", information->system_model, true);
    printf(",\"memoryTotalBytes\":");
    if (information->memory_available)
        printf("%" PRIu64, information->memory_total_bytes);
    else fputs("null", stdout);
    json_nullable_string("firmwareVendor", information->firmware_vendor, true);
    json_nullable_string("firmwareVersion", information->firmware_version, true);
    json_nullable_string("firmwareDate", information->firmware_date, true);
    printf(",\"uptimeSeconds\":");
    if (information->uptime_available)
        printf("%" PRIu64, information->uptime_seconds);
    else fputs("null", stdout);
    printf("},\"coverage\":{\"unavailableFields\":%zu},"
           "\"semantics\":{\"hostNameExposed\":false,"
           "\"serialNumbersExposed\":false,\"pathsExposed\":false}}\n",
           information->unavailable_fields);
    return ferror(stdout) ? 1 : 0;
}

int mon_render_information(const mon_information *information,
                           const mon_options *options) {
    if (!information || !options) return 1;
    return options->format == MON_FORMAT_JSON
        ? render_information_json(information, options)
        : render_information_text(information, options);
}
