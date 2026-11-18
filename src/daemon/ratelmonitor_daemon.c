#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#include "ratelmonitor_daemon.h"

#define DEFAULT_STATE_PATH "/tmp/ratelmonitor_shared"
#define FILE_ENV "RATELMONITOR_DAEMON_FILE"

static char clients_lock_path[PATH_MAX];
static char worker_lock_path[PATH_MAX];

static int client_lock_fd = -1;
static int worker_lock_fd = -1;
static bool daemon_joined;

/* --------------------------------------------------------- */
/* Paths                                                     */
/* --------------------------------------------------------- */

static ratelmonitor_status_t initialize_paths(void)
{
    const char *base = getenv(FILE_ENV);

    if (base == NULL || base[0] == '\0')
        base = DEFAULT_STATE_PATH;

    unsigned int uid = (unsigned int)getuid();
    int n;

    n = snprintf(clients_lock_path, sizeof(clients_lock_path),
                 "%s_%u.clients.lock", base, uid);

    if (n < 0 || n >= (int)sizeof(clients_lock_path))
        return RATELMONITOR_STATUS_WRITE_FAILED;

    n = snprintf(worker_lock_path, sizeof(worker_lock_path),
                 "%s_%u.worker.lock", base, uid);

    if (n < 0 || n >= (int)sizeof(worker_lock_path))
        return RATELMONITOR_STATUS_WRITE_FAILED;


    return RATELMONITOR_STATUS_SUCCESS;
}

/* --------------------------------------------------------- */
/* Lock-file helpers                                         */
/* --------------------------------------------------------- */

static int open_lock_file(const char *path)
{
    return open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
}

static void close_lock_file(int fd)
{
    if (fd >= 0)
        close(fd);
}

ratelmonitor_status_t open_lock_client(int *fd)
{
    if (fd == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    *fd = open_lock_file(clients_lock_path);

    if (*fd < 0)
        return RATELMONITOR_STATUS_OPEN_FAILED;

    if (flock(*fd, LOCK_SH) != 0) {
        close(*fd);
        *fd = -1;
        return RATELMONITOR_STATUS_OPEN_FAILED;
    }

    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t open_lock_worker(int *fd, bool *exists)
{
    if (fd == NULL || exists == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    *exists = false;

    *fd = open_lock_file(worker_lock_path);

    if (*fd < 0)
        return RATELMONITOR_STATUS_OPEN_FAILED;

    if (flock(*fd, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            *exists = true;
            close(*fd);
            *fd = -1;
            return RATELMONITOR_STATUS_SUCCESS;
        }
        close(*fd);
        *fd = -1;
        return RATELMONITOR_STATUS_OPEN_FAILED;
    }

    return RATELMONITOR_STATUS_SUCCESS;
}


/* --------------------------------------------------------- */
/* Worker: monitor application processes                     */
/* --------------------------------------------------------- */

static void *monitor_clients(void *uarg)
{
    ratelmonitor_worker_on_exit_t *on_exit_data = (ratelmonitor_worker_on_exit_t *)uarg;

    int fd = open_lock_file(clients_lock_path);

    if (fd < 0)
        _exit(EXIT_FAILURE);


    if (flock(fd, LOCK_EX) == 0) {
        close(fd);

        // No clients remain.
        if (on_exit_data && on_exit_data->on_exit) {
            on_exit_data->on_exit(on_exit_data->user_data);
            free(on_exit_data);
        }
    }

    return NULL;
}

/* --------------------------------------------------------- */
/* Worker process                                            */
/* --------------------------------------------------------- */

static void run_worker_process(
    ratelmonitor_worker_t worker, void *user_data, ratelmonitor_worker_on_exit_t *on_exit_data)
{
    pthread_t monitor_thread;

    if (pthread_create(&monitor_thread, NULL, monitor_clients, on_exit_data) != 0) {
        _exit(EXIT_FAILURE);
    }

    // Run the original worker callback.
    ratelmonitor_status_t status = worker(user_data);

    if (status != RATELMONITOR_STATUS_SUCCESS) {
        fprintf(stderr,
                "ratelmonitor daemon worker failed: %s\n",
                ratelmonitor_status_string(status));
    }

    // The worker callback returned, so the worker process exits.
    _exit(status == RATELMONITOR_STATUS_SUCCESS
              ? EXIT_SUCCESS
              : EXIT_FAILURE);
}


/* --------------------------------------------------------- */
/* Public API                                                */
/* --------------------------------------------------------- */

static void daemon_cleanup(void)
{
    ratelmonitor_daemon_stop();
}

ratelmonitor_status_t ratelmonitor_daemon_start(
    ratelmonitor_worker_t worker,
    void *user_data,
    ratelmonitor_worker_on_exit_t *on_exit_data)
{
    if (worker == NULL)
        return RATELMONITOR_STATUS_INVALID_PTR;

    if (daemon_joined)
        return RATELMONITOR_STATUS_SUCCESS;

    ratelmonitor_status_t status = initialize_paths();

    if (status != RATELMONITOR_STATUS_SUCCESS)
        return status;

    /*
     * Every application independently opens this file and
     * holds a shared lock for its lifetime.
     */
    status = open_lock_client(&client_lock_fd);
    if (status != RATELMONITOR_STATUS_SUCCESS) {
        return status;
    }

    /*
     * Try to acquire the exclusive worker lock.
     *
     * Success: no worker exists, so this process must start one.
     * EWOULDBLOCK: a worker already exists, do nothing.
     */
    bool worker_exists = false;

    status = open_lock_worker(&worker_lock_fd, &worker_exists);
    if (status != RATELMONITOR_STATUS_SUCCESS) {
        close(client_lock_fd);
        client_lock_fd = -1;
        return status;
    }

    if (!worker_exists) {
        pid_t pid = fork();

        if (pid < 0) {
            close(worker_lock_fd);
            worker_lock_fd = -1;
            close(client_lock_fd);
            client_lock_fd = -1;

            return RATELMONITOR_STATUS_THREAD_FAILED;
        }

        if (pid == 0) {
            close_lock_file(client_lock_fd);
            client_lock_fd = -1;
            /*
            * worker_lock_fd is intentionally kept open in child process.
            * It holds LOCK_EX and prevents another worker from starting.
            */
            run_worker_process(worker, user_data, on_exit_data);
        }
    }

    close(worker_lock_fd);
    worker_lock_fd = -1;

    daemon_joined = true;

    atexit(daemon_cleanup);

    return RATELMONITOR_STATUS_SUCCESS;
}

ratelmonitor_status_t ratelmonitor_daemon_stop(void)
{
    if (!daemon_joined)
        return RATELMONITOR_STATUS_SUCCESS;

    // Closing this descriptor releases this application's shared lock.
    close_lock_file(client_lock_fd);
    client_lock_fd = -1;

    daemon_joined = false;

    return RATELMONITOR_STATUS_SUCCESS;
}