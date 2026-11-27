#ifndef SONION_SCHED_H
#define SONION_SCHED_H

#include <stdint.h>

#define MAX_TASKS 8

enum {
    TASK_UNUSED  = 0,
    TASK_RUNNING = 1,
    TASK_READY   = 2,
    TASK_ZOMBIE  = 3
};

typedef struct {
    int      pid;
    char     name[24];
    int      state;
    int      cpl;
    uint64_t ticks;
    uint32_t mem_kb;
    uint64_t rsp;
    uint8_t  kstack[4096] __attribute__((aligned(16)));
} TaskInfo;

void      sched_init(void);
void      sched_yield(void);
int       sched_register_task(const char *name, int cpl, uint32_t mem_kb);
void      sched_set_state(int pid, int state);
void      sched_tick(void);
int       sched_kill(int pid);
int       sched_count(void);
TaskInfo *sched_get(int idx);
int       sched_current_pid(void);

#endif
