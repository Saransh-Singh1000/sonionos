#ifndef SONION_USYSCALL_H
#define SONION_USYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "sysinfo_page.h"
#include "sysinfo_page.h"

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

typedef struct {
    int      in_use;
    int      writable;
    uint32_t first_cluster;
    uint32_t cur_cluster;
    uint32_t cur_cluster_idx;
    uint32_t size;
    uint32_t pos;
    uint32_t dir_sector;
    uint32_t dir_offset;
} Fat32File;

typedef struct {
    char     name[256];
    uint32_t size;
    int      is_dir;
} UserDirEntry;

static inline int64_t syscall3(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void sys_exit(int code) { syscall3(SYS_EXIT, (uint64_t)code, 0, 0); while (1); }
static inline void sys_write(const char *s, size_t len) { syscall3(SYS_WRITE, (uint64_t)(uintptr_t)s, len, 0); }
static inline int  sys_getchar(int blocking) { return (int)syscall3(SYS_GETCHAR, (uint64_t)blocking, 0, 0); }
static inline void sys_clear(void) { syscall3(SYS_CLEAR, 0, 0, 0); }
static inline int  sys_open(const char *p, const char *m) { return (int)syscall3(SYS_OPEN, (uint64_t)(uintptr_t)p, (uint64_t)(uintptr_t)m, 0); }
static inline int64_t sys_read(int fd, void *b, size_t n) { return syscall3(SYS_READ, (uint64_t)fd, (uint64_t)(uintptr_t)b, n); }
static inline int64_t sys_fwrite(int fd, const void *b, size_t n) { return syscall3(SYS_FWRITE, (uint64_t)fd, (uint64_t)(uintptr_t)b, n); }
static inline int  sys_seek(int fd, int64_t off, int w) { return (int)syscall3(SYS_SEEK, (uint64_t)fd, (uint64_t)off, (uint64_t)w); }
static inline int64_t sys_tell(int fd) { return syscall3(SYS_TELL, (uint64_t)fd, 0, 0); }
static inline void sys_close(int fd) { syscall3(SYS_CLOSE, (uint64_t)fd, 0, 0); }
static inline int  sys_mkdir(const char *p) { return (int)syscall3(SYS_MKDIR, (uint64_t)(uintptr_t)p, 0, 0); }
static inline int  sys_remove(const char *p) { return (int)syscall3(SYS_REMOVE, (uint64_t)(uintptr_t)p, 0, 0); }
static inline int  sys_opendir(const char *p) { return (int)syscall3(SYS_OPENDIR, (uint64_t)(uintptr_t)p, 0, 0); }
static inline int  sys_readdir(int dd, UserDirEntry *e) { return (int)syscall3(SYS_READDIR, (uint64_t)dd, (uint64_t)(uintptr_t)e, 0); }
static inline void sys_closedir(int dd) { syscall3(SYS_CLOSEDIR, (uint64_t)dd, 0, 0); }
static inline uint64_t u_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline int64_t sys_clock(void) {
    const volatile SonionSysInfo *si = (const volatile SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;
    if (si->magic == SYSINFO_MAGIC && si->tsc_per_ms > 0) {
        return (int64_t)((u_rdtsc() - si->boot_tsc) / si->tsc_per_ms);
    }
    return syscall3(SYS_CLOCK, 0, 0, 0);
}

static inline int64_t sys_time(void) {
    const volatile SonionSysInfo *si = (const volatile SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;
    if (si->magic == SYSINFO_MAGIC && si->tsc_per_ms > 0) {
        uint64_t ms = (u_rdtsc() - si->boot_tsc) / si->tsc_per_ms;
        return (int64_t)(si->unix_base + (ms / 1000ULL));
    }
    return syscall3(SYS_TIME, 0, 0, 0);
}
static inline void sys_reboot(void) { syscall3(SYS_REBOOT, 0, 0, 0); }
static inline void sys_poweroff(void) { syscall3(SYS_POWEROFF, 0, 0, 0); }
static inline void sys_disk_info(uint64_t *buf) { syscall3(SYS_DISK_INFO, (uint64_t)(uintptr_t)buf, 0, 0); }
static inline int64_t sys_batch(void *ops, uint64_t count) { return syscall3(SYS_BATCH, (uint64_t)(uintptr_t)ops, count, 0); }
static inline int  sys_exec(const char *p) { return (int)syscall3(SYS_EXEC, (uint64_t)(uintptr_t)p, 0, 0); }

static inline uint16_t get_current_cpl(void) {
    uint16_t cs;
    __asm__ volatile ("movw %%cs, %0" : "=r"(cs));
    return cs & 3;
}

static inline int sys_kmem(uint64_t out7[7]) { return (int)syscall3(33, (uint64_t)out7, 0, 0); }
typedef struct {
    uint8_t scancode;
    uint8_t pressed; /* 1 = Key-Down, 0 = Key-Up */
    char    ascii;
    uint8_t reserved;
} SonionKeyEvent;

static inline int sys_poll_key(SonionKeyEvent *ev) {
    return (int)syscall3(34, (uint64_t)ev, 0, 0);
}
#endif









