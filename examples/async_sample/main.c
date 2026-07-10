#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/runtime/task.h>
#include <blue-bird/runtime/event.h>

typedef struct {
    bb_runtime_t *runtime;
    int counter;
} app_state_t;

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

static void counter_task(bb_task_t *task, void *userdata)
{
    (void)task;

    app_state_t *app = userdata;

    printf("counter: %d\n", app->counter++);
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

    bb_task_t *stdin_watcher = bb_runtime_watch_fd(
        runtime,
        STDIN_FILENO,
        BB_EVENT_READ,
        BB_WATCH_PERSISTENT,
        stdin_task, runtime
    );
    (void)stdin_watcher;

    printf("Press 'q' to quit.\n");

    bb_runtime_run(runtime);

    bb_runtime_destroy(runtime);

    disable_raw_mode();

    return 0;
}
