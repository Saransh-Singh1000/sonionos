#ifndef SONION_SYSCALL_H
#define SONION_SYSCALL_H

#include <stdint.h>
#include "bootinfo.h"

#define SYS_EXIT       0
#define SYS_WRITE      1
#define SYS_GETCHAR    2
#define SYS_CLEAR      3
#define SYS_OPEN       4
#define SYS_READ       5
#define SYS_FWRITE     6
#define SYS_SEEK       7
#define SYS_TELL       8
#define SYS_CLOSE      9
#define SYS_MKDIR      10
#define SYS_REMOVE     11
#define SYS_OPENDIR    12
#define SYS_READDIR    13
#define SYS_CLOSEDIR   14
#define SYS_TIME       15
#define SYS_CLOCK      16
#define SYS_REBOOT     17
#define SYS_POWEROFF   18
#define SYS_DISK_INFO  19
#define SYS_EXEC       20
#define SYS_BATCH      21

void syscall_init(BootInfo *info);
int  ring3_exec(const char *path);
int  ring3_exec_child(const char *path);

void ring3_fault_recover(uint64_t exc, uint64_t rip, uint64_t cr2);
#endif



