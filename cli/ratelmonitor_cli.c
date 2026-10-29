#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "ratelmonitor.h"
#include "ratelmonitor_cli.h"


int main(int argc, char **argv)
{
    ratelmonitor_gpu_t *gpus = NULL;
    size_t gpu_count = 0;

    if (ratelmonitor_discover_gpus(&gpus, &gpu_count) != 0) {
        free(gpus);
        return EXIT_FAILURE;
    }

    if (gpu_count == 0) {
        printf("No AMD GPUs found.\n");
        free(gpus);
        return EXIT_SUCCESS;
    }

    uint64_t interval_ms = 1000;
    if (argc > 1) {
        char *end = NULL;
        interval_ms = strtoull(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || interval_ms == 0) {
            fprintf(stderr, "usage: %s [interval_ms]\n", argv[0]);
            free(gpus);
            return EXIT_FAILURE;
        }
    }
    
    ratelmonitor_sampling_context_t ctx = {0};
    ratelmonitor_sampling_init(&ctx, gpus, gpu_count);
    ctx.user_data = &ctx;
    
    if (ratelmonitor_sampling_start(&ctx, interval_ms, callback, NULL, (bool[]){true, true, true}) != 0) {
        fprintf(stderr, "Failed to start sampling thread.\n");
    }

    ratelmonitor_sampling_wait(&ctx);

    ratelmonitor_fini(gpus, gpu_count);
    return EXIT_SUCCESS;
}
