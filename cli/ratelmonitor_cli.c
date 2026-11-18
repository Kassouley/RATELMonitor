#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/stat.h>

#include "ratelmonitor.h"
#include "ratelmonitor_cli.h"
#include "ratelmonitor_utils.h"

#define DEFAULT_INTERVAL_MS 1000

static bool user_stop(ratelmonitor_sampling_context_t *ctx)
{
    (void)ctx;

    fd_set input;
    FD_ZERO(&input);
    FD_SET(STDIN_FILENO, &input);

    struct timeval timeout = {0, 0};
    if (select(STDIN_FILENO + 1, &input, NULL, NULL, &timeout) > 0 &&
        FD_ISSET(STDIN_FILENO, &input)) {
        int input_char = getchar();
        if (input_char == '\n' || input_char == EOF) {
            return true;
        }
    }
    return false;
}

static int run_terminal(uint64_t interval_ms)
{
    ratelmonitor_gpu_t *gpus = NULL;
    size_t gpu_count = 0;
    ratelmonitor_status_t status = ratelmonitor_discover_gpus(&gpus, &gpu_count);
    if (status != RATELMONITOR_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to discover GPUs: %s\n", ratelmonitor_status_string(status));
        return EXIT_FAILURE;
    }

    if (gpu_count == 0) {
        printf("No AMD GPUs found.\n");
        free(gpus);
        return EXIT_SUCCESS;
    }

    ratelmonitor_sampling_context_t ctx = {0};

    status = ratelmonitor_sampling_init(&ctx, gpus, gpu_count, interval_ms, callback, &ctx,
                                         (bool[]){true, true, true});
    if (status != RATELMONITOR_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize sampling: %s\n", ratelmonitor_status_string(status));
        ratelmonitor_fini(gpus, gpu_count);
        return EXIT_FAILURE;
    }

    status = ratelmonitor_sampling_loop(&ctx, user_stop);
    if (status != RATELMONITOR_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to start sampling loop: %s\n", ratelmonitor_status_string(status));
        ratelmonitor_fini(gpus, gpu_count);
        return EXIT_FAILURE;
    }

    ratelmonitor_status_t fini_status = ratelmonitor_fini(gpus, gpu_count);
    if (fini_status != RATELMONITOR_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to release GPU resources: %s\n", ratelmonitor_status_string(fini_status));
        status = fini_status;
    }
    return status == RATELMONITOR_STATUS_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

extern const unsigned char _binary_libratelmonitor_preload_so_start[];
extern const unsigned char _binary_libratelmonitor_preload_so_end[];

static int run_preloaded_application(uint64_t interval_ms,
                                     const char *output_path,
                                     char **application)
{
    if (setenv("RATELMONITOR_CSV", output_path, 1) != 0) {
        fprintf(stderr, "Failed to configure output path environment: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    char interval_text[32];
    snprintf(interval_text, sizeof(interval_text), "%" PRIu64, interval_ms);
    if (setenv("RATELMONITOR_INTERVAL_MS", interval_text, 1) != 0)
        return EXIT_FAILURE;

    size_t lib_size = _binary_libratelmonitor_preload_so_end - _binary_libratelmonitor_preload_so_start;
    int fd = syscall(SYS_memfd_create, "embedded-ratelmon-daemon", 0);
    if (fd < 0) {
        perror("memfd_create");
        return EXIT_FAILURE;
    }

    // Write embedded library.
    size_t offset = 0;

    while (offset < lib_size) {
        ssize_t n = write(fd, _binary_libratelmonitor_preload_so_start + offset, lib_size - offset);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            perror("write");
            close(fd);
            return EXIT_FAILURE;
        }

        if (n == 0) {
            fprintf(stderr, "Unexpected EOF while writing library\n");
            close(fd);
            return EXIT_FAILURE;
        }

        offset += n;
    }

    // Make the anonymous file executable/readable.
    if (fchmod(fd, 0700) < 0) {
        perror("fchmod");
        close(fd);
        return EXIT_FAILURE;
    }

    // Construct /proc/self/fd/<fd>.
    char preload_path[64];

    snprintf(preload_path, sizeof(preload_path), "/proc/self/fd/%d", fd);

    // Configure LD_PRELOAD.
    if (setenv("LD_PRELOAD", preload_path, 1) < 0) {
        perror("setenv");
        close(fd);
        return EXIT_FAILURE;
    }

    execvp(application[0], application);

    fprintf(stderr, "Failed to start %s: %s\n", application[0], strerror(errno));
    perror("execvp");
    close(fd);

    return EXIT_FAILURE;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage:\n"
            "  %s [interval_ms]\n"
            "  %s terminal [interval_ms]\n"
            "  %s daemon [--interval-ms N] [--output FILE] -- application [args...]\n",
            program, program, program);
}

int main(int argc, char **argv)
{
    uint64_t interval_ms = DEFAULT_INTERVAL_MS;
    const char *output_path = "ratelmonitor.csv";
    bool daemon = false;
    int application_index = -1;

    if (argc > 1 && strcmp(argv[1], "daemon") == 0) {
        daemon = true;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--") == 0) {
                application_index = i + 1;
                break;
            }
            if (strcmp(argv[i], "--interval-ms") == 0 && i + 1 < argc) {
                char *end = NULL;
                interval_ms = strtoull(argv[++i], &end, 10);
                if (end == argv[i] || *end != '\0' || interval_ms == 0) {
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                continue;
            }
            if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
                output_path = argv[++i];
                continue;
            }
            application_index = i;
            break;
        }
        if (application_index < 0)
            application_index = argc;
        if (application_index >= argc) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        int interval_index = argc > 1 && strcmp(argv[1], "terminal") == 0 ? 2 : 1;
        if (argc > interval_index) {
            char *end = NULL;
            interval_ms = strtoull(argv[interval_index], &end, 10);
            if (end == argv[interval_index] || *end != '\0' || interval_ms == 0 ||
                argc > interval_index + 1) {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
    }

    if (daemon) {
        return run_preloaded_application(interval_ms, output_path,
                                         &argv[application_index]);
    }
    return run_terminal(interval_ms);
}
