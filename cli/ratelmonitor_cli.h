#ifndef RATELMONITOR_CLI_H
#define RATELMONITOR_CLI_H

#include <stdio.h>
#include <stdint.h>
#include "ratelmonitor.h"

#define BAR_WIDTH 24

static const char *COLOR_RESET  = "\033[0m";
static const char *COLOR_CYAN   = "\033[1;36m";
static const char *COLOR_BLUE   = "\033[1;34m";
static const char *COLOR_GREEN  = "\033[1;32m";
static const char *COLOR_YELLOW = "\033[1;33m";
static const char *COLOR_RED    = "\033[1;31m";

/* Clamp a percentage to [0, 100]. */
static int clamp_percent(double percent)
{
    if (percent < 0.0)   return 0;
    if (percent > 100.0) return 100;
    return (int)percent;
}

/* Select a color based on the utilization percentage. */
static const char *get_usage_color(int percent)
{
    return percent >= 90 ? COLOR_RED
         : percent >= 70 ? COLOR_YELLOW
                         : COLOR_GREEN;
}

/* Render a progress bar with a consistent width. */
static void print_progress_bar(int percent, const char *color)
{
    printf("%s[", color);

    int filled = percent * BAR_WIDTH / 100;

    for (int i = 0; i < BAR_WIDTH; ++i) {
        putchar(i < filled ? '#' : '-');
    }

    printf("]%s", COLOR_RESET);
}

/* Render the dashboard header. */
static void print_dashboard_header(void)
{
    printf("\033[2J\033[H\n\n");
    printf("%s+-----------------------------------------------------------------------+%s\n",
           COLOR_CYAN, COLOR_RESET);
    printf("%s|                              RATELMonitor                             |%s\n",
           COLOR_CYAN, COLOR_RESET);
    printf("%s|                       GPU MONITORING DASHBOARD                        |%s\n",
           COLOR_CYAN, COLOR_RESET);
    printf("%s+-----------------------------------------------------------------------+%s\n\n",
           COLOR_CYAN, COLOR_RESET);
}

/* Render the GPU information header. */
static void print_gpu_header(const ratelmonitor_gpu_t *gpu)
{
    printf("%s+--[ GPU %2u ]-----------------------------------------------------------+%s\n",
           COLOR_BLUE, gpu->node_id, COLOR_RESET);

    printf("%s|%s  PCI %04x:%02x:%02x.%x                                                     %s|%s\n",
           COLOR_BLUE, COLOR_RESET,
           gpu->dbdf.domain,
           gpu->dbdf.bus,
           gpu->dbdf.device,
           gpu->dbdf.function,
           COLOR_BLUE, COLOR_RESET);
}

/* Render a generic utilization row. */
static void print_usage_row(
    const char *label,
    double percentage,
    int bar_percentage)
{
    const char *color = get_usage_color(bar_percentage);

    printf("%s|%s  %-12s", COLOR_BLUE, COLOR_RESET, label);
    print_progress_bar(bar_percentage, color);

    printf(" %5.1f%%%s %s|%s\n",
           percentage,
           COLOR_BLUE,
           "", /* Reserved for row-specific spacing. */
           COLOR_RESET);
}

/* Render a GPU usage row. */
static void print_gpu_usage_row(int usage)
{
    int bar_percentage = clamp_percent(usage);

    printf("%s|%s  GPU Usage   ",
           COLOR_BLUE, COLOR_RESET);

    print_progress_bar(bar_percentage, get_usage_color(bar_percentage));

    printf(" %3d%%%s                          |%s\n",
           bar_percentage, COLOR_BLUE, COLOR_RESET);
}

/* Render a VRAM usage row. */
static void print_vram_row(
    double percentage,
    int bar_percentage,
    double used_gib,
    double total_gib)
{
    printf("%s|%s  VRAM        ",
           COLOR_BLUE, COLOR_RESET);

    print_progress_bar(bar_percentage, get_usage_color(bar_percentage));

    printf(" %5.1f%%  %6.2f / %6.2f GiB%s   |%s\n",
           percentage, used_gib, total_gib,
           COLOR_BLUE, COLOR_RESET);
}

/* Render a power usage row. */
static void print_power_row(
    double percentage,
    int bar_percentage,
    uint64_t used_watts,
    uint64_t total_watts)
{
    printf("%s|%s  Power       ",
           COLOR_BLUE, COLOR_RESET);

    print_progress_bar(bar_percentage, get_usage_color(bar_percentage));

    printf(" %5.1f%%  %6lu / %6lu W%s     |%s\n",
           percentage, used_watts, total_watts,
           COLOR_BLUE, COLOR_RESET);
}

/* Main sampling callback. */
static void callback(ratelmonitor_sample_t *sample, void *user_data)
{
    ratelmonitor_sampling_context_t *ctx = user_data;

    if (sample == NULL || ctx == NULL ||
        sample->gpu_index >= ctx->gpu_count ||
        sample->gpu == NULL) {
        return;
    }

    if (sample->gpu_index == 0) {
        print_dashboard_header();
    }

    const ratelmonitor_gpu_t *gpu = sample->gpu;

    uint64_t vram_used = sample->gpu_vram_used > 0
                       ? (uint64_t)sample->gpu_vram_used
                       : 0;

    double vram_used_gib =
        (double)vram_used / (1024.0 * 1024.0 * 1024.0);

    double vram_total_gib =
        (double)gpu->vram_total / (1024.0 * 1024.0 * 1024.0);

    double vram_usage_percent = gpu->vram_total != 0
        ? (double)vram_used * 100.0 / (double)gpu->vram_total
        : 0.0;

    double power_usage_percent = gpu->power_cap != 0
        ? (double)sample->gpu_power_watts * 100.0 / (double)gpu->power_cap
        : 0.0;

    int usage_bar = clamp_percent(sample->gpu_usage_percent);
    int vram_bar = clamp_percent(vram_usage_percent);
    int power_bar = clamp_percent(power_usage_percent);

    print_gpu_header(gpu);

    print_gpu_usage_row(usage_bar);

    print_vram_row(vram_usage_percent, vram_bar, vram_used_gib, vram_total_gib);

    print_power_row(power_usage_percent, power_bar, (uint64_t)sample->gpu_power_watts, (uint64_t)gpu->power_cap);

    printf("%s+-----------------------------------------------------------------------+%s\n\n",
           COLOR_BLUE, COLOR_RESET);

    fflush(stdout);
}

#endif // RATELMONITOR_CLI_H