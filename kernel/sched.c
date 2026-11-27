#include "sched.h"
#include <stddef.h>

static TaskInfo tasks[MAX_TASKS];
static int current_idx = 0;
static int next_pid = 1;

static void scpy(char *d, const char *s, size_t m) {
    size_t i = 0;
    if (!s) s = "task";
    while (s[i] && i + 1 < m) { d[i] = s[i]; i++; }
    d[i] = 0;
}

void sched_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].pid = 0;
        tasks[i].state = TASK_UNUSED;
        tasks[i].ticks = 0;
    }
    tasks[0].pid = next_pid++;
    scpy(tasks[0].name, "INIT.BIN (Shell)", 24);
    tasks[0].state = TASK_RUNNING;
    tasks[0].cpl = 3;
    tasks[0].mem_kb = 16384;
    tasks[0].ticks = 1;
    current_idx = 0;
}

void sched_yield(void) { sched_tick(); }

int sched_register_task(const char *name, int cpl, uint32_t mem_kb) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_ZOMBIE) {
            tasks[i].pid = next_pid++;
            scpy(tasks[i].name, name, 24);
            tasks[i].state = TASK_RUNNING;
            tasks[i].cpl = cpl;
            tasks[i].mem_kb = mem_kb;
            tasks[i].ticks = 1;
            return tasks[i].pid;
        }
    }
    return -1;
}

void sched_set_state(int pid, int state) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid) { tasks[i].state = state; return; }
    }
}

void sched_tick(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_RUNNING || tasks[i].state == TASK_READY) {
            tasks[i].ticks++;
        }
    }
}

int sched_kill(int pid) {
    if (pid <= 1) return -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid && tasks[i].state != TASK_UNUSED) {
            tasks[i].state = TASK_UNUSED;
            tasks[i].pid = 0;
            return 0;
        }
    }
    return -2;
}

int sched_count(void) {
    int c = 0;
    for (int i = 0; i < MAX_TASKS; i++) if (tasks[i].state != TASK_UNUSED) c++;
    return c;
}

TaskInfo *sched_get(int idx) {
    return (idx < 0 || idx >= MAX_TASKS) ? 0 : &tasks[idx];
}

int sched_current_pid(void) { return tasks[current_idx].pid; }
