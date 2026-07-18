#include <stdio.h>

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/runtime/task.h>
#include <blue-bird/runtime/event.h>

#if defined(_WIN32)
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

typedef struct {
    bb_runtime_t *runtime;
    int counter;
} app_state_t;

#if defined(_WIN32)

/* select() on Windows only works on SOCKETs, not on console/file handles,
 * so bb_runtime_watch_fd() (which is built on the select()-based poller)
 * cannot observe stdin here the way it can on POSIX. Raw mode is also a
 * different API (Console mode flags, not termios). Instead of forcing
 * stdin through the poller, we poll the keyboard from a regular timer
 * task on the runtime, which needs no changes to the runtime/poller. */

static HANDLE stdin_handle;
static DWORD original_console_mode;

static void enable_raw_mode(void)
{
    stdin_handle = GetStdHandle(STD_INPUT_HANDLE);

    GetConsoleMode(stdin_handle, &original_console_mode);

    DWORD raw_mode = original_console_mode;
    raw_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    SetConsoleMode(stdin_handle, raw_mode);
}

static void disable_raw_mode(void)
{
    SetConsoleMode(stdin_handle, original_console_mode);
}

static void stdin_poll_task(bb_task_t *task, void *userdata)
{
    (void) task;

    bb_runtime_t *runtime = userdata;

    if (_kbhit())
    {
        char c = (char) _getch();

        if (c == 'q')
        {
            printf("Stopping runtime...\n");

            disable_raw_mode();

            bb_runtime_stop(runtime);
        }
    }
}

#else

static struct termios original_termios;

static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &original_termios);

    struct termios raw = original_termios;

    raw.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

static void stdin_task(bb_task_t *task, void *userdata)
{
    (void) task;

    bb_runtime_t *runtime = userdata;

    char c;

    if (read(STDIN_FILENO, &c, 1) == 1)
    {
        if (c == 'q')
        {
            printf("Stopping runtime...\n");

            disable_raw_mode();

            bb_runtime_stop(runtime);
        }
    }
}

#endif

static void counter_task(bb_task_t *task, void *userdata)
{
    (void) task;

    app_state_t *app = userdata;

    printf("counter: %d\n", app->counter++);
}

int main(void)
{
    enable_raw_mode();

    bb_runtime_t *runtime = bb_runtime_create();

    app_state_t app = {
        .runtime = runtime,
        .counter = 0
    };

    bb_task_t *counter = bb_runtime_set_interval(runtime, 1000, counter_task, &app);
    (void) counter;

#if defined(_WIN32)
    /* Poll the keyboard every 50ms instead of watching stdin as an fd. */
    bb_task_t *stdin_watcher = bb_runtime_set_interval(runtime, 50, stdin_poll_task, runtime);
    (void) stdin_watcher;
#else
    bb_task_t *stdin_watcher = bb_runtime_watch_fd(
        runtime,
        STDIN_FILENO,
        BB_EVENT_READ,
        BB_WATCH_PERSISTENT,
        stdin_task, runtime
    );
    (void) stdin_watcher;
#endif

    printf("Press 'q' to quit.\n");

    bb_runtime_run(runtime);

    bb_runtime_destroy(runtime);

    disable_raw_mode();

    return 0;
}
