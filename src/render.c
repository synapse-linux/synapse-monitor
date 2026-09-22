// SPDX-License-Identifier: MIT
#include "monitor.h"

#include <inttypes.h>
#include <limits.h>
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

static void format_iec(uint64_t bytes, char *output, size_t output_size) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    long double value = (long double)bytes;
    size_t unit = 0U;
    while (value >= 1024.0L && unit + 1U < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0L;
        unit++;
    }
    if (unit == 0U) snprintf(output, output_size, "%" PRIu64 " %s", bytes, units[unit]);
    else snprintf(output, output_size, "%.1Lf %s", value, units[unit]);
}

static void format_rate(uint64_t bytes, char *output, size_t output_size) {
    char value[32];
    format_iec(bytes, value, sizeof(value));
    snprintf(output, output_size, "%s/s", value);
}

static void format_percent(uint64_t milli, char *output, size_t output_size) {
    snprintf(output, output_size, "%" PRIu64 ".%03" PRIu64 "%%",
             milli / 1000U, milli % 1000U);
}

static void format_temperature(int64_t milli, char *output, size_t output_size) {
    bool negative = milli < 0;
    uint64_t magnitude = negative ? (uint64_t)(-(milli + 1)) + 1U
                                  : (uint64_t)milli;
    snprintf(output, output_size, "%s%" PRIu64 ".%03" PRIu64 " C",
             negative ? "-" : "", magnitude / 1000U, magnitude % 1000U);
}

static void format_frequency(uint64_t hertz, char *output, size_t output_size) {
    snprintf(output, output_size, "%" PRIu64 ".%03" PRIu64 " MHz",
             hertz / 1000000U, (hertz % 1000000U) / 1000U);
}

static void format_power(uint64_t microwatts, char *output, size_t output_size) {
    snprintf(output, output_size, "%" PRIu64 ".%03" PRIu64 " W",
             microwatts / 1000000U, (microwatts % 1000000U) / 1000U);
}

static size_t rows_returned(const mon_report *report, const mon_options *options) {
    return report->processes.count < options->limit
        ? report->processes.count : options->limit;
}

static void render_summary_text(const mon_report *report) {
    char cpu[32] = "unavailable";
    char memory_used[32] = "unavailable";
    char memory_total[32] = "unavailable";
    char gpu[64] = "unavailable";
    char disk_read[48] = "unavailable";
    char disk_write[48] = "unavailable";
    char net_receive[48] = "unavailable";
    char net_transmit[48] = "unavailable";
    if (report->cpu_available)
        format_percent(report->cpu_busy_percent_milli, cpu, sizeof(cpu));
    if (report->memory_available) {
        format_iec(report->memory_used_bytes, memory_used, sizeof(memory_used));
        format_iec(report->memory_total_bytes, memory_total, sizeof(memory_total));
    }
    if (report->gpu_available)
        format_percent(report->gpu_busy_percent_milli, gpu, sizeof(gpu));
    else if (report->gpu_present)
        (void)snprintf(gpu, sizeof(gpu), "present; activity unavailable");
    if (report->disk_available) {
        format_rate(report->disk_read_bytes_per_second, disk_read, sizeof(disk_read));
        format_rate(report->disk_write_bytes_per_second, disk_write, sizeof(disk_write));
    }
    if (report->network_available) {
        format_rate(report->network_rx_bytes_per_second, net_receive,
                    sizeof(net_receive));
        format_rate(report->network_tx_bytes_per_second, net_transmit,
                    sizeof(net_transmit));
    }
    printf("SYNAPSE MONITOR  read-only  sample=%" PRIu64 " ms\n",
           report->sample_milliseconds);
    printf("CPU %s (%u logical)  |  RAM %s / %s  |  GPU %s\n",
           cpu, report->processor_count, memory_used, memory_total, gpu);
    printf("DISK read %s write %s  |  NET receive %s transmit %s\n",
           disk_read, disk_write, net_receive, net_transmit);
    if (report->gpu_memory_available) {
        char used[32];
        format_iec(report->gpu_memory_used_bytes, used, sizeof(used));
        if (report->gpu_memory_total_available) {
            char total[32];
            format_iec(report->gpu_memory_total_bytes, total, sizeof(total));
            printf("GPU memory %s / %s (card %u)\n", used, total,
                   report->gpu_card);
        } else {
            printf("GPU memory %s shared GEM, included in observed footprint "
                   "(card %u)\n", used, report->gpu_card);
        }
    }
    if (report->observed_memory_available) {
        char process_pss[32];
        char shared_gpu[32];
        char observed[32];
        format_iec(report->process_pss_bytes, process_pss, sizeof(process_pss));
        format_iec(report->observed_shared_gpu_bytes, shared_gpu,
                   sizeof(shared_gpu));
        format_iec(report->observed_memory_bytes, observed, sizeof(observed));
        printf("Observed footprint %s = process PSS %s + shared GPU %s\n",
               observed, process_pss, shared_gpu);
    }
}

static void render_table_header(uint32_t columns) {
    if (columns & MON_COLUMN_NAME) printf("%-24s", "NAME");
    if (columns & MON_COLUMN_PID) printf(" %7s", "PID");
    if (columns & MON_COLUMN_USER) printf(" %7s", "USER");
    if (columns & MON_COLUMN_STATE) printf(" %5s", "STATE");
    if (columns & MON_COLUMN_THREADS) printf(" %7s", "THREADS");
    if (columns & MON_COLUMN_CPU) printf(" %10s", "CPU");
    if (columns & MON_COLUMN_MEMORY) printf(" %11s", "MEMORY");
    if (columns & MON_COLUMN_READ) printf(" %12s", "READ");
    if (columns & MON_COLUMN_WRITE) printf(" %12s", "WRITE");
    fputc('\n', stdout);
}

static void render_process_text(const mon_process *process, uint32_t columns) {
    char user[16] = "-";
    char cpu[32] = "-";
    char memory[32];
    char read_rate[48] = "-";
    char write_rate[48] = "-";
    if (process->uid_available) snprintf(user, sizeof(user), "%u", process->uid);
    if (process->cpu_available)
        format_percent(process->cpu_percent_milli, cpu, sizeof(cpu));
    format_iec(process->resident_bytes, memory, sizeof(memory));
    if (process->io_available && process->existed_for_sample) {
        format_rate(process->read_bytes_per_second, read_rate, sizeof(read_rate));
        format_rate(process->write_bytes_per_second, write_rate, sizeof(write_rate));
    }
    if (columns & MON_COLUMN_NAME) printf("%-24.24s", process->name);
    if (columns & MON_COLUMN_PID) printf(" %7d", process->pid);
    if (columns & MON_COLUMN_USER) printf(" %7s", user);
    if (columns & MON_COLUMN_STATE) printf(" %5c", process->state);
    if (columns & MON_COLUMN_THREADS) printf(" %7" PRIu64, process->threads);
    if (columns & MON_COLUMN_CPU) printf(" %10s", cpu);
    if (columns & MON_COLUMN_MEMORY) printf(" %11s", memory);
    if (columns & MON_COLUMN_READ) printf(" %12s", read_rate);
    if (columns & MON_COLUMN_WRITE) printf(" %12s", write_rate);
    fputc('\n', stdout);
}

static bool same_group(const mon_process *left, const mon_process *right,
                       mon_group group) {
    if (group == MON_GROUP_CLASS) return left->process_class == right->process_class;
    if (group == MON_GROUP_NAME) return strcmp(left->name, right->name) == 0;
    return true;
}

static const char *class_title(mon_process_class process_class) {
    switch (process_class) {
        case MON_CLASS_SYSTEM: return "SYSTEM PROCESSES";
        case MON_CLASS_KERNEL: return "KERNEL TASKS";
        case MON_CLASS_APPLICATION:
        default: return "APPLICATION PROCESSES";
    }
}

static void render_group_header(const mon_report *report, size_t index,
                                size_t returned, mon_group group) {
    const mon_process *process = &report->processes.rows[index];
    size_t count = 1U;
    while (index + count < returned
           && same_group(process, &report->processes.rows[index + count], group))
        count++;
    if (group == MON_GROUP_CLASS)
        printf("\n[%s · %zu]\n", class_title(process->process_class), count);
    else if (group == MON_GROUP_NAME)
        printf("\n[%s · %zu process%s]\n", process->name, count,
               count == 1U ? "" : "es");
}

static int render_text(const mon_report *report, const mon_options *options) {
    size_t returned = rows_returned(report, options);
    render_tabs(options);
    printf("%sPROCESSES%s\n", strong(options), reset(options));
    render_summary_text(report);
    printf("\nFilter: %s  |  Sort: %s  |  Group: %s  |  Rows: %zu/%zu\n",
           options->filter[0] ? options->filter : "(none)",
           mon_sort_id(options->sort), mon_group_id(options->group), returned,
           report->matched_rows);
    if (returned == 0U) {
        fputs("\nNo matching processes.\n", stdout);
    } else {
        for (size_t i = 0U; i < returned; i++) {
            bool new_group = options->group != MON_GROUP_NONE
                && (i == 0U || !same_group(&report->processes.rows[i - 1U],
                                           &report->processes.rows[i],
                                           options->group));
            if (new_group) {
                render_group_header(report, i, returned, options->group);
                render_table_header(options->columns);
            } else if (i == 0U) render_table_header(options->columns);
            render_process_text(&report->processes.rows[i], options->columns);
        }
    }
    printf("\n%sCoverage: proc=%zu/%zu denied=%zu vanished=%zu malformed=%zu "
           "io-unavailable=%zu truncated=%s%s\n", muted(options),
           report->observed_rows, report->processes.numeric_directories,
           report->processes.permission_denied, report->processes.vanished,
           report->processes.malformed, report->processes.io_unavailable,
           report->processes.truncated ? "true" : "false", reset(options));
    fputs("Controls are inspection-only; no process mutation is available.\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

static void json_string(const char *value) {
    fputc('"', stdout);
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++) {
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

static void json_u64(const char *name, uint64_t value, bool comma) {
    printf("%s\"%s\":%" PRIu64, comma ? "," : "", name, value);
}

static void render_available_i64(const char *name, bool available, int64_t value,
                                 bool comma) {
    printf("%s\"%s\":", comma ? "," : "", name);
    if (available) printf("%" PRId64, value);
    else fputs("null", stdout);
}

static void render_available_string(const char *name, const char *value,
                                    bool comma) {
    printf("%s\"%s\":", comma ? "," : "", name);
    if (value && *value) json_string(value);
    else fputs("null", stdout);
}

static void render_columns_json(uint32_t columns) {
    struct column { uint32_t flag; const char *id; } values[] = {
        {MON_COLUMN_NAME, "name"}, {MON_COLUMN_PID, "pid"},
        {MON_COLUMN_USER, "user"}, {MON_COLUMN_STATE, "state"},
        {MON_COLUMN_THREADS, "threads"}, {MON_COLUMN_CPU, "cpu"},
        {MON_COLUMN_MEMORY, "memory"}, {MON_COLUMN_READ, "read"},
        {MON_COLUMN_WRITE, "write"}
    };
    fputc('[', stdout);
    bool first = true;
    for (size_t i = 0U; i < sizeof(values) / sizeof(values[0]); i++) {
        if ((columns & values[i].flag) == 0U) continue;
        if (!first) fputc(',', stdout);
        json_string(values[i].id);
        first = false;
    }
    fputc(']', stdout);
}

static void render_available_u64(const char *name, bool available, uint64_t value,
                                 bool comma) {
    printf("%s\"%s\":", comma ? "," : "", name);
    if (available) printf("%" PRIu64, value);
    else fputs("null", stdout);
}

static const char *gpu_memory_kind(const mon_gpu *gpu) {
    if (!gpu) return NULL;
    if (strcmp(gpu->driver, "i915") == 0 || strcmp(gpu->driver, "xe") == 0)
        return "shared";
    if (gpu->memory_available) return "driver-reported-vram";
    return "unavailable";
}

static const char *gpu_memory_source(const mon_gpu *gpu) {
    if (!gpu || !gpu->memory_available) return "unavailable";
    return gpu->memory_from_collector ? "root-owned-fresh-collector"
                                      : "driver-sysfs";
}

static const mon_gpu *report_summary_gpu(const mon_report *report) {
    if (!report || report->gpu_count == 0U) return NULL;
    for (size_t index = 0U; index < report->gpu_count; index++)
        if (report->gpus[index].card == report->gpu_card)
            return &report->gpus[index];
    return &report->gpus[0];
}

static void render_gpu_memory_json(const mon_gpu *gpu) {
    const bool present = gpu != NULL;
    const bool used_available = present && gpu->memory_available;
    printf(",\"memoryAvailable\":%s", used_available ? "true" : "false");
    fputs(",\"memoryKind\":", stdout);
    if (present) json_string(gpu_memory_kind(gpu));
    else fputs("null", stdout);
    fputs(",\"memorySource\":", stdout);
    if (present) json_string(gpu_memory_source(gpu));
    else fputs("null", stdout);
    render_available_u64("memoryUsedBytes", used_available,
                         present ? gpu->memory_used_bytes : 0U, true);
    render_available_u64("memoryTotalBytes",
                         present && gpu->memory_total_available,
                         present ? gpu->memory_total_bytes : 0U, true);
    fputs(",\"memoryOverlapsSystemRam\":", stdout);
    if (present && strcmp(gpu_memory_kind(gpu), "shared") == 0)
        fputs("true", stdout);
    else fputs("null", stdout);
    render_available_u64("memorySampleAgeMilliseconds",
                         present && gpu->memory_sample_age_available,
                         present ? gpu->memory_sample_age_milliseconds : 0U,
                         true);
}

static void render_observed_memory_json(const mon_report *report) {
    const bool available = report && report->observed_memory_available;
    printf(",\"observedFootprintAvailable\":%s",
           available ? "true" : "false");
    render_available_u64("processPssBytes", available,
                         available ? report->process_pss_bytes : 0U, true);
    render_available_u64("sharedGpuBytes", available,
                         available ? report->observed_shared_gpu_bytes : 0U,
                         true);
    render_available_u64("observedFootprintBytes", available,
                         available ? report->observed_memory_bytes : 0U, true);
    fputs(",\"observedFootprintAccounting\":", stdout);
    if (available) json_string("process-pss-plus-global-i915-gem");
    else fputs("null", stdout);
    fputs(",\"observedFootprintComponentsMayOverlap\":", stdout);
    fputs(available ? "true" : "null", stdout);
}

static int render_json(const mon_report *report, const mon_options *options) {
    size_t returned = rows_returned(report, options);
    fputs("{\"schema\":\"synapse.monitor.snapshot/v1\",\"readOnly\":true,\"view\":\"processes\"", stdout);
    mon_render_stream_metadata(options);
    json_u64("sampledMilliseconds", report->sample_milliseconds, true);
    fputs(",\"selection\":{\"sort\":", stdout);
    json_string(mon_sort_id(options->sort));
    fputs(",\"group\":", stdout);
    json_string(mon_group_id(options->group));
    fputs(",\"filter\":", stdout);
    json_string(options->filter);
    json_u64("limit", options->limit, true);
    fputs(",\"columns\":", stdout);
    render_columns_json(options->columns);
    fputs(",\"theme\":", stdout);
    json_string(mon_theme_id(options->theme));
    fputs(",\"layout\":", stdout);
    json_string(mon_layout_id(options->layout));

    printf("},\"summary\":{\"cpu\":{\"available\":%s",
           report->cpu_available ? "true" : "false");
    render_available_u64("busyPercentMilli", report->cpu_available,
                         report->cpu_busy_percent_milli, true);
    json_u64("logicalProcessors", report->processor_count, true);
    printf("},\"memory\":{\"available\":%s",
           report->memory_available ? "true" : "false");
    render_available_u64("totalBytes", report->memory_available,
                         report->memory_total_bytes, true);
    render_available_u64("availableBytes", report->memory_available,
                         report->memory_available_bytes, true);
    render_available_u64("usedBytes", report->memory_available,
                         report->memory_used_bytes, true);
    render_observed_memory_json(report);
    printf("},\"gpu\":{\"present\":%s,\"available\":%s",
           report->gpu_present ? "true" : "false",
           report->gpu_available ? "true" : "false");
    render_available_u64("card", report->gpu_present, report->gpu_card, true);
    render_available_u64("busyPercentMilli", report->gpu_available,
                         report->gpu_busy_percent_milli, true);
    render_gpu_memory_json(report_summary_gpu(report));
    printf("},\"disk\":{\"available\":%s",
           report->disk_available ? "true" : "false");
    render_available_u64("readBytesPerSecond", report->disk_available,
                         report->disk_read_bytes_per_second, true);
    render_available_u64("writeBytesPerSecond", report->disk_available,
                         report->disk_write_bytes_per_second, true);
    json_u64("devices", report->disk_devices, true);
    json_u64("devicesSkipped", report->disk_devices_skipped, true);
    printf(",\"truncated\":%s", report->disk_truncated ? "true" : "false");
    printf("},\"network\":{\"available\":%s",
           report->network_available ? "true" : "false");
    render_available_u64("receiveBytesPerSecond", report->network_available,
                         report->network_rx_bytes_per_second, true);
    render_available_u64("transmitBytesPerSecond", report->network_available,
                         report->network_tx_bytes_per_second, true);
    json_u64("interfaces", report->network_interfaces, true);
    printf(",\"truncated\":%s}}", report->network_truncated ? "true" : "false");

    fputs(",\"coverage\":{", stdout);
    json_u64("directoriesSeen", report->processes.directories_seen, false);
    json_u64("numericDirectories", report->processes.numeric_directories, true);
    json_u64("rowsObserved", report->observed_rows, true);
    json_u64("rowsMatched", report->matched_rows, true);
    json_u64("rowsReturned", returned, true);
    json_u64("permissionDenied", report->processes.permission_denied, true);
    json_u64("vanished", report->processes.vanished, true);
    json_u64("malformed", report->processes.malformed, true);
    json_u64("ioUnavailable", report->processes.io_unavailable, true);
    printf(",\"truncated\":%s},\"rows\":[",
           report->processes.truncated ? "true" : "false");
    for (size_t i = 0U; i < returned; i++) {
        const mon_process *process = &report->processes.rows[i];
        if (i > 0U) fputc(',', stdout);
        printf("{\"pid\":%d,\"startTicks\":%" PRIu64 ",\"uid\":",
               process->pid, process->start_ticks);
        if (process->uid_available) printf("%u", process->uid);
        else fputs("null", stdout);
        fputs(",\"name\":", stdout);
        json_string(process->name);
        fputs(",\"class\":", stdout);
        json_string(mon_class_id(process->process_class));
        fputs(",\"groupKey\":", stdout);
        if (options->group == MON_GROUP_CLASS)
            json_string(mon_class_id(process->process_class));
        else if (options->group == MON_GROUP_NAME) json_string(process->name);
        else fputs("null", stdout);
        printf(",\"state\":\"%c\"", process->state);
        json_u64("threads", process->threads, true);
        printf(",\"sampled\":%s", process->existed_for_sample ? "true" : "false");
        render_available_u64("cpuPercentMilli", process->cpu_available,
                             process->cpu_percent_milli, true);
        json_u64("residentBytes", process->resident_bytes, true);
        printf(",\"ioAvailable\":%s", process->io_available ? "true" : "false");
        render_available_u64("readBytesPerSecond",
                             process->io_available && process->existed_for_sample,
                             process->read_bytes_per_second, true);
        render_available_u64("writeBytesPerSecond",
                             process->io_available && process->existed_for_sample,
                             process->write_bytes_per_second, true);
        fputc('}', stdout);
    }
    fputs("],\"semantics\":{\"processControl\":false,"
          "\"commandLinesExposed\":false,\"pathsExposed\":false,"
          "\"diskSectorBytes\":512,\"ratesAreSampleDeltas\":true,"
          "\"sharedGpuMemoryNonAdditive\":true,"
          "\"observedFootprintAddsSharedGpu\":true}}\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

static size_t history_index(const mon_history *history, size_t index) {
    size_t start = history->count < MON_HISTORY_SAMPLES ? 0U : history->next;
    return (start + index) % MON_HISTORY_SAMPLES;
}

static uint64_t history_at(const uint64_t values[MON_HISTORY_SAMPLES],
                           const mon_history *history, size_t index) {
    return values[history_index(history, index)];
}

static bool history_available_at(
    const bool values[MON_HISTORY_SAMPLES], const mon_history *history,
    size_t index) {
    return values[history_index(history, index)];
}

static bool history_has_available(
    const bool values[MON_HISTORY_SAMPLES], const mon_history *history) {
    if (!history) return false;
    for (size_t i = 0U; i < history->count; i++)
        if (history_available_at(values, history, i)) return true;
    return false;
}

static void render_history_line(const char *label,
                                const uint64_t values[MON_HISTORY_SAMPLES],
                                const bool available[MON_HISTORY_SAMPLES],
                                const mon_history *history, uint64_t scale) {
    static const char levels[] = " .:-=+*#%@";
    if (!history || history->count == 0U) return;
    if (scale == 0U) {
        for (size_t i = 0U; i < history->count; i++) {
            if (!history_available_at(available, history, i)) continue;
            uint64_t value = history_at(values, history, i);
            if (value > scale) scale = value;
        }
    }
    if (scale == 0U) scale = 1U;
    printf("%-8s ", label);
    for (size_t i = 0U; i < history->count; i++) {
        if (!history_available_at(available, history, i)) {
            fputc(' ', stdout);
            continue;
        }
        uint64_t value = history_at(values, history, i);
        size_t level = value >= scale ? sizeof(levels) - 2U
            : (size_t)(value * (sizeof(levels) - 2U) / scale);
        fputc(levels[level], stdout);
    }
    fputc('\n', stdout);
}

static const char *gpu_vendor(const char *vendor_id) {
    if (strcmp(vendor_id, "0x8086") == 0) return "Intel";
    if (strcmp(vendor_id, "0x1002") == 0) return "AMD";
    if (strcmp(vendor_id, "0x10de") == 0) return "NVIDIA";
    return "unknown-vendor";
}

static void render_gpu_text(const mon_gpu *gpu) {
    char utilization[32] = "unavailable";
    char temperature[32] = "unavailable";
    char memory[80] = "unavailable";
    char core_clock[48] = "unavailable";
    char memory_clock[48] = "unavailable";
    char power[48] = "unavailable";
    char power_cap[48] = "unavailable";
    char fan[32] = "unavailable";
    if (gpu->utilization_available)
        format_percent(gpu->utilization_percent_milli, utilization,
                       sizeof(utilization));
    if (gpu->temperature_available)
        format_temperature(gpu->temperature_millidegrees_celsius, temperature,
                           sizeof(temperature));
    if (gpu->memory_available) {
        char used[32];
        format_iec(gpu->memory_used_bytes, used, sizeof(used));
        if (gpu->memory_total_available) {
            char total[32];
            format_iec(gpu->memory_total_bytes, total, sizeof(total));
            (void)snprintf(memory, sizeof(memory), "%s / %s", used, total);
        } else {
            (void)snprintf(memory, sizeof(memory), "%s shared GEM", used);
        }
    } else if (strcmp(gpu_memory_kind(gpu), "shared") == 0)
        (void)snprintf(memory, sizeof(memory), "shared; driver-managed");
    if (gpu->core_clock_available)
        format_frequency(gpu->core_clock_hz, core_clock, sizeof(core_clock));
    if (gpu->memory_clock_available)
        format_frequency(gpu->memory_clock_hz, memory_clock,
                         sizeof(memory_clock));
    if (gpu->power_available)
        format_power(gpu->power_microwatts, power, sizeof(power));
    if (gpu->power_cap_available)
        format_power(gpu->power_cap_microwatts, power_cap, sizeof(power_cap));
    if (gpu->fan_available)
        (void)snprintf(fan, sizeof(fan), "%" PRIu64 " RPM", gpu->fan_rpm);
    printf("card%u %-6s device=%-7s driver=%-10s load=%-12s temp=%s\n",
           gpu->card, gpu_vendor(gpu->vendor_id),
           gpu->device_id[0] ? gpu->device_id : "unknown",
           gpu->driver[0] ? gpu->driver : "unknown", utilization, temperature);
    printf("       model=%s\n", gpu->model[0] ? gpu->model : "unavailable");
    printf("       memory=%-28s core=%-15s memory-clock=%s\n",
           memory, core_clock, memory_clock);
    printf("       power=%-15s cap=%-15s fan=%s\n", power, power_cap, fan);
}

static void render_temperature_text(const mon_temperature *temperature) {
    char current[32];
    char maximum[32] = "-";
    char critical[32] = "-";
    format_temperature(temperature->temperature_millidegrees_celsius,
                       current, sizeof(current));
    if (temperature->maximum_available)
        format_temperature(temperature->maximum_millidegrees_celsius,
                           maximum, sizeof(maximum));
    if (temperature->critical_available)
        format_temperature(temperature->critical_millidegrees_celsius,
                           critical, sizeof(critical));
    printf("%-11s %-14.14s %-24.24s current=%-12s max=%-12s critical=%s\n",
           mon_thermal_class_id(temperature->sensor_class), temperature->source,
           temperature->label, current, maximum, critical);
}

static void performance_budgets(const mon_options *options, size_t *cpu,
                                size_t *gpu, size_t *temperature, size_t *fan,
                                size_t *disk, size_t *network) {
    size_t total = options->limit;
    *cpu = (total + 2U) / 3U;
    size_t remaining = total - *cpu;
    *gpu = total / 8U;
    if (*gpu > remaining) *gpu = remaining;
    remaining -= *gpu;
    *temperature = total / 4U;
    if (*temperature > remaining) *temperature = remaining;
    remaining -= *temperature;
    *fan = total / 12U;
    if (*fan > remaining) *fan = remaining;
    remaining -= *fan;
    *disk = remaining / 2U;
    *network = remaining - *disk;
}

static int render_performance_text(const mon_report *report,
                                   const mon_options *options) {
    size_t cpu_budget = 0U, gpu_budget = 0U, temperature_budget = 0U;
    size_t fan_budget = 0U, disk_budget = 0U, network_budget = 0U;
    performance_budgets(options, &cpu_budget, &gpu_budget, &temperature_budget,
                        &fan_budget, &disk_budget, &network_budget);
    size_t cpu_returned = report->cpu_count < cpu_budget
        ? report->cpu_count : cpu_budget;
    size_t gpu_returned = report->gpu_count < gpu_budget
        ? report->gpu_count : gpu_budget;
    size_t temperature_returned = report->temperature_count < temperature_budget
        ? report->temperature_count : temperature_budget;
    size_t fan_returned = report->fan_count < fan_budget
        ? report->fan_count : fan_budget;
    size_t disk_returned = report->disk_devices < disk_budget
        ? report->disk_devices : disk_budget;
    size_t network_returned = report->network_interfaces < network_budget
        ? report->network_interfaces : network_budget;
    render_tabs(options);
    printf("%sPERFORMANCE%s  read-only  sample=%" PRIu64 " ms\n",
           strong(options), reset(options), report->sample_milliseconds);
    render_summary_text(report);
    if (options->history && options->history->count > 0U) {
        fputs("\nHISTORY (oldest to newest)\n", stdout);
        render_history_line("CPU", options->history->cpu,
                            options->history->cpu_available, options->history,
                            100000U);
        render_history_line("RAM", options->history->memory,
                            options->history->memory_available, options->history,
                            100000U);
        if (history_has_available(options->history->gpu_available,
                                  options->history))
            render_history_line("GPU", options->history->gpu,
                                options->history->gpu_available,
                                options->history, 100000U);
        render_history_line("DISK", options->history->disk,
                            options->history->disk_available, options->history,
                            0U);
        render_history_line("NETWORK", options->history->network,
                            options->history->network_available, options->history,
                            0U);
    }
    fputs("\nGRAPHICS PROCESSORS\n", stdout);
    if (report->gpu_count == 0U)
        fputs("unavailable\n", stdout);
    for (size_t i = 0U; i < gpu_returned; i++) render_gpu_text(&report->gpus[i]);
    if (report->gpu_count > 0U) {
        bool any_gpu_temperature = false;
        for (size_t i = 0U; i < report->gpu_count; i++)
            if (report->gpus[i].temperature_available) any_gpu_temperature = true;
        if (!any_gpu_temperature)
            fputs("GPU temperature unavailable; CPU/package temperature is not substituted.\n",
                  stdout);
    }

    fputs("\nTEMPERATURES\n", stdout);
    if (report->temperature_count == 0U) fputs("unavailable\n", stdout);
    for (size_t i = 0U; i < temperature_returned; i++)
        render_temperature_text(&report->temperatures[i]);
    fputs("\nFANS\n", stdout);
    if (report->fan_count == 0U) fputs("unavailable\n", stdout);
    for (size_t i = 0U; i < fan_returned; i++)
        printf("%-14.14s %-24.24s %" PRIu64 " RPM\n",
               report->fans[i].source, report->fans[i].label,
               report->fans[i].rpm);

    fputs("\nLOGICAL PROCESSORS\n", stdout);
    if (report->cpu_count == 0U) fputs("unavailable\n", stdout);
    size_t per_line = options->layout == MON_LAYOUT_DENSE ? 8U
        : (options->layout == MON_LAYOUT_WIDE ? 10U : 6U);
    for (size_t i = 0U; i < cpu_returned; i++) {
        char percent[24] = "unavailable";
        if (report->cpu_entry_available[i])
            format_percent(report->cpu_percent_milli[i], percent, sizeof(percent));
        printf("CPU%-4zu %10s", i, percent);
        if ((i + 1U) % per_line == 0U || i + 1U == cpu_returned)
            fputc('\n', stdout);
        else fputs("  ", stdout);
    }
    fputs("\nPHYSICAL DISKS\n", stdout);
    if (!report->disk_available || report->disk_devices == 0U)
        fputs("unavailable\n", stdout);
    for (size_t i = 0U; i < disk_returned; i++) {
        char read_rate[48];
        char write_rate[48];
        format_rate(report->disks[i].read_bytes_per_second, read_rate,
                    sizeof(read_rate));
        format_rate(report->disks[i].write_bytes_per_second, write_rate,
                    sizeof(write_rate));
        printf("%-16.16s read %-14s write %-14s\n", report->disks[i].name,
               read_rate, write_rate);
    }
    fputs("\nNETWORK INTERFACES\n", stdout);
    if (!report->network_available || report->network_interfaces == 0U)
        fputs("unavailable\n", stdout);
    for (size_t i = 0U; i < network_returned; i++) {
        char receive[48];
        char transmit[48];
        format_rate(report->interfaces[i].receive_bytes_per_second, receive,
                    sizeof(receive));
        format_rate(report->interfaces[i].transmit_bytes_per_second, transmit,
                    sizeof(transmit));
        printf("%-16.16s receive %-14s transmit %-14s\n",
               report->interfaces[i].name, receive, transmit);
    }
    printf("\n%sCoverage: logical-cpus=%zu/%zu cpu-truncated=%s gpus=%zu/%zu "
           "gpu-truncated=%s temperatures=%zu/%zu temp-truncated=%s "
           "fans=%zu/%zu fan-truncated=%s disks=%zu/%zu disk-skipped=%zu "
           "disk-truncated=%s interfaces=%zu/%zu net-truncated=%s "
           "sensor-denied=%zu sensor-malformed=%zu%s\n", muted(options),
           cpu_returned, report->cpu_count,
           report->cpu_truncated ? "true" : "false", gpu_returned,
           report->gpu_count, report->gpu_truncated ? "true" : "false",
           temperature_returned, report->temperature_count,
           report->temperature_truncated ? "true" : "false", fan_returned,
           report->fan_count, report->fan_truncated ? "true" : "false",
           disk_returned, report->disk_devices, report->disk_devices_skipped,
           report->disk_truncated ? "true" : "false", network_returned,
           report->network_interfaces,
           report->network_truncated ? "true" : "false",
           report->sensor_permission_denied, report->sensor_malformed,
           reset(options));
    fputs("All graphs and rates are bounded local observations; no telemetry is sent.\n",
          stdout);
    return ferror(stdout) ? 1 : 0;
}

static void render_history_json_values(
    const uint64_t values[MON_HISTORY_SAMPLES],
    const bool available[MON_HISTORY_SAMPLES], const mon_history *history) {
    fputc('[', stdout);
    if (history) {
        for (size_t i = 0U; i < history->count; i++) {
            if (i > 0U) fputc(',', stdout);
            if (history_available_at(available, history, i))
                printf("%" PRIu64, history_at(values, history, i));
            else fputs("null", stdout);
        }
    }
    fputc(']', stdout);
}

static void render_gpu_json(const mon_gpu *gpu) {
    printf("{\"card\":%u", gpu->card);
    render_available_string("vendorId", gpu->vendor_id, true);
    render_available_string("deviceId", gpu->device_id, true);
    render_available_string("vendor", gpu_vendor(gpu->vendor_id), true);
    render_available_string("driver", gpu->driver, true);
    render_available_string("model", gpu->model, true);
    render_gpu_memory_json(gpu);
    render_available_u64("utilizationPercentMilli", gpu->utilization_available,
                         gpu->utilization_percent_milli, true);
    render_available_i64("temperatureMillidegreesCelsius",
                         gpu->temperature_available,
                         gpu->temperature_millidegrees_celsius, true);
    render_available_string("temperatureLabel",
                            gpu->temperature_available
                                ? gpu->temperature_label : NULL, true);
    render_available_u64("coreClockHz", gpu->core_clock_available,
                         gpu->core_clock_hz, true);
    render_available_u64("coreClockMaximumHz", gpu->core_clock_max_available,
                         gpu->core_clock_max_hz, true);
    render_available_u64("memoryClockHz", gpu->memory_clock_available,
                         gpu->memory_clock_hz, true);
    render_available_u64("powerMicrowatts", gpu->power_available,
                         gpu->power_microwatts, true);
    render_available_u64("powerCapMicrowatts", gpu->power_cap_available,
                         gpu->power_cap_microwatts, true);
    render_available_u64("fanRpm", gpu->fan_available, gpu->fan_rpm, true);
    fputc('}', stdout);
}

static void render_temperature_json(const mon_temperature *temperature) {
    fputs("{\"class\":", stdout);
    json_string(mon_thermal_class_id(temperature->sensor_class));
    fputs(",\"source\":", stdout);
    json_string(temperature->source);
    fputs(",\"label\":", stdout);
    json_string(temperature->label);
    printf(",\"temperatureMillidegreesCelsius\":%" PRId64,
           temperature->temperature_millidegrees_celsius);
    render_available_i64("maximumMillidegreesCelsius",
                         temperature->maximum_available,
                         temperature->maximum_millidegrees_celsius, true);
    render_available_i64("criticalMillidegreesCelsius",
                         temperature->critical_available,
                         temperature->critical_millidegrees_celsius, true);
    fputc('}', stdout);
}

static int render_performance_json(const mon_report *report,
                                   const mon_options *options) {
    size_t cpu_budget = 0U, gpu_budget = 0U, temperature_budget = 0U;
    size_t fan_budget = 0U, disk_budget = 0U, network_budget = 0U;
    performance_budgets(options, &cpu_budget, &gpu_budget, &temperature_budget,
                        &fan_budget, &disk_budget, &network_budget);
    size_t cpu_returned = report->cpu_count < cpu_budget
        ? report->cpu_count : cpu_budget;
    size_t gpu_returned = report->gpu_count < gpu_budget
        ? report->gpu_count : gpu_budget;
    size_t temperature_returned = report->temperature_count < temperature_budget
        ? report->temperature_count : temperature_budget;
    size_t fan_returned = report->fan_count < fan_budget
        ? report->fan_count : fan_budget;
    size_t disk_returned = report->disk_devices < disk_budget
        ? report->disk_devices : disk_budget;
    size_t network_returned = report->network_interfaces < network_budget
        ? report->network_interfaces : network_budget;
    fputs("{\"schema\":\"synapse.monitor.performance/v2\","
          "\"readOnly\":true,\"view\":\"performance\"", stdout);
    mon_render_stream_metadata(options);
    json_u64("sampledMilliseconds", report->sample_milliseconds, true);
    printf(",\"cpu\":{\"available\":%s",
           report->cpu_available ? "true" : "false");
    render_available_u64("busyPercentMilli", report->cpu_available,
                         report->cpu_busy_percent_milli, true);
    json_u64("logicalProcessorCount", report->cpu_count, true);
    json_u64("logicalProcessorsReturned", cpu_returned, true);
    printf(",\"truncated\":%s,\"logicalProcessors\":[",
           report->cpu_truncated ? "true" : "false");
    for (size_t i = 0U; i < cpu_returned; i++) {
        if (i > 0U) fputc(',', stdout);
        if (report->cpu_entry_available[i])
            printf("%" PRIu64, report->cpu_percent_milli[i]);
        else fputs("null", stdout);
    }
    printf("]},\"memory\":{\"available\":%s",
           report->memory_available ? "true" : "false");
    render_available_u64("totalBytes", report->memory_available,
                         report->memory_total_bytes, true);
    render_available_u64("availableBytes", report->memory_available,
                         report->memory_available_bytes, true);
    render_available_u64("usedBytes", report->memory_available,
                         report->memory_used_bytes, true);
    render_observed_memory_json(report);
    printf("},\"gpu\":{\"present\":%s,\"available\":%s",
           report->gpu_present ? "true" : "false",
           report->gpu_available ? "true" : "false");
    render_available_u64("card", report->gpu_present, report->gpu_card, true);
    render_available_u64("busyPercentMilli", report->gpu_available,
                         report->gpu_busy_percent_milli, true);
    render_gpu_memory_json(report_summary_gpu(report));
    printf("},\"gpus\":{\"available\":%s,\"rowsReturned\":%zu,"
           "\"rowsObserved\":%zu,\"truncated\":%s,\"rows\":[",
           report->gpu_present ? "true" : "false", gpu_returned,
           report->gpu_count, report->gpu_truncated ? "true" : "false");
    for (size_t i = 0U; i < gpu_returned; i++) {
        if (i > 0U) fputc(',', stdout);
        render_gpu_json(&report->gpus[i]);
    }
    printf("],\"integratedGpuTemperatureInferred\":false},"
           "\"thermals\":{\"available\":%s,\"hwmonDevicesSeen\":%zu,"
           "\"permissionDenied\":%zu,\"malformed\":%zu,"
           "\"temperaturesReturned\":%zu,\"temperaturesObserved\":%zu,"
           "\"temperatureTruncated\":%s,\"temperatures\":[",
           report->temperature_count > 0U || report->fan_count > 0U
               ? "true" : "false",
           report->hwmon_devices_seen, report->sensor_permission_denied,
           report->sensor_malformed, temperature_returned,
           report->temperature_count,
           report->temperature_truncated ? "true" : "false");
    for (size_t i = 0U; i < temperature_returned; i++) {
        if (i > 0U) fputc(',', stdout);
        render_temperature_json(&report->temperatures[i]);
    }
    printf("],\"fansReturned\":%zu,\"fansObserved\":%zu,"
           "\"fanTruncated\":%s,\"fans\":[", fan_returned,
           report->fan_count, report->fan_truncated ? "true" : "false");
    for (size_t i = 0U; i < fan_returned; i++) {
        if (i > 0U) fputc(',', stdout);
        fputs("{\"source\":", stdout);
        json_string(report->fans[i].source);
        fputs(",\"label\":", stdout);
        json_string(report->fans[i].label);
        json_u64("rpm", report->fans[i].rpm, true);
        fputc('}', stdout);
    }
    printf("]},\"disks\":{\"available\":%s,\"rows\":[",
           report->disk_available ? "true" : "false");
    for (size_t i = 0U; i < disk_returned; i++) {
        if (i > 0U) fputc(',', stdout);
        fputs("{\"name\":", stdout); json_string(report->disks[i].name);
        json_u64("readBytesPerSecond",
                 report->disks[i].read_bytes_per_second, true);
        json_u64("writeBytesPerSecond",
                 report->disks[i].write_bytes_per_second, true);
        fputc('}', stdout);
    }
    printf("],\"skipped\":%zu,\"truncated\":%s},"
           "\"network\":{\"available\":%s,\"rows\":[",
           report->disk_devices_skipped,
           report->disk_truncated ? "true" : "false",
           report->network_available ? "true" : "false");
    for (size_t i = 0U; i < network_returned; i++) {
        if (i > 0U) fputc(',', stdout);
        fputs("{\"name\":", stdout); json_string(report->interfaces[i].name);
        json_u64("receiveBytesPerSecond",
                 report->interfaces[i].receive_bytes_per_second, true);
        json_u64("transmitBytesPerSecond",
                 report->interfaces[i].transmit_bytes_per_second, true);
        fputc('}', stdout);
    }
    printf("],\"truncated\":%s},\"history\":{\"cpuPercentMilli\":",
           report->network_truncated ? "true" : "false");
    render_history_json_values(options->history ? options->history->cpu : NULL,
                               options->history
                                   ? options->history->cpu_available : NULL,
                               options->history);
    fputs(",\"memoryPercentMilli\":", stdout);
    render_history_json_values(options->history ? options->history->memory : NULL,
                               options->history
                                   ? options->history->memory_available : NULL,
                               options->history);
    fputs(",\"gpuPercentMilli\":", stdout);
    render_history_json_values(options->history ? options->history->gpu : NULL,
                               options->history
                                   ? options->history->gpu_available : NULL,
                               options->history);
    fputs(",\"diskBytesPerSecond\":", stdout);
    render_history_json_values(options->history ? options->history->disk : NULL,
                               options->history
                                   ? options->history->disk_available : NULL,
                               options->history);
    fputs(",\"networkBytesPerSecond\":", stdout);
    render_history_json_values(options->history ? options->history->network : NULL,
                               options->history
                                   ? options->history->network_available : NULL,
                               options->history);
    fputs("},\"semantics\":{\"ratesAreSampleDeltas\":true,"
          "\"historyMaximumSamples\":60,"
          "\"temperatureUnit\":\"millidegrees-celsius\","
          "\"frequencyUnit\":\"hertz\",\"powerUnit\":\"microwatts\","
          "\"integratedGpuTemperatureInferred\":false,"
          "\"sharedGpuMemoryNonAdditive\":true,"
          "\"observedFootprintAddsSharedGpu\":true,"
          "\"observedFootprintBase\":\"process-pss\","
          "\"telemetry\":false}}\n", stdout);
    return ferror(stdout) ? 1 : 0;
}

int mon_render_report(const mon_report *report, const mon_options *options) {
    if (!report || !options) return 1;
    if (options->view == MON_VIEW_PERFORMANCE)
        return options->format == MON_FORMAT_JSON
            ? render_performance_json(report, options)
            : render_performance_text(report, options);
    if (options->view != MON_VIEW_PROCESSES) return 1;
    return options->format == MON_FORMAT_JSON
        ? render_json(report, options) : render_text(report, options);
}
