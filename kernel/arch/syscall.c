#include "audio.h"
#include "kbd_events.h"
#include <stddef.h>
extern void kheap_stats(size_t *total_b, size_t *used_b, size_t *free_b, size_t *used_cnt, size_t *free_cnt);
#include "sched.h"
#include "arch/syscall.h"
#include "arch/gdt.h"
#include "arch/paging.h"
#include "sysinfo_page.h"
#include <stddef.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include "console.h"
#include "blockdev.h"
#include "fs/fat32.h"

typedef struct __attribute__((packed)) {
    uint64_t r10, r9, r8, rdx, rsi, rdi, rbp, rbx;
    uint64_t r11, rcx, user_rsp;
} SyscallFrame;

static BootInfo  *sc_info = 0;
static Fat32File *fd_tab[32];
static Fat32Dir  *dir_tab[8];
static jmp_buf    r3_ret_jmp[2];
static int        r3_exit_code[2];
static int        r3_depth = 0;

static uint8_t  sc_kstack[2][16384] __attribute__((aligned(16)));
static uint64_t sc_kstack_top = 0;
static uint64_t sc_user_rsp   = 0;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

static int user_ptr_ok(const void *ptr, size_t len) {
    uint64_t p = (uint64_t)(uintptr_t)ptr;
    if (!p || p < USER_BASE_ADDR || p >= USER_LIMIT_ADDR) return 0;
    if (len > (USER_LIMIT_ADDR - p)) return 0;
    return 1;
}
static void sys_hw_reboot(void) {
    __asm__ volatile ("cli");
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x06), "Nd"((uint16_t)0xCF9));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    while (1) __asm__ volatile ("hlt");
}

static char sc_tolower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int sc_starts_with_nocase(const char *s, const char *prefix) {
    while (*prefix) {
        if (sc_tolower(*s) != sc_tolower(*prefix)) return 0;
        s++; prefix++;
    }
    return (*s == 0 || *s == '/' || *s == '\\');
}

static int is_protected_syspath(const char *path) {
    if (!path) return 0;
    while (*path == '/' || *path == '\\') path++;
    if (sc_starts_with_nocase(path, "efi"))  return 1;
    if (sc_starts_with_nocase(path, "boot")) return 1;
    if (sc_starts_with_nocase(path, "user")) return 1;
    return 0;
}

static void deny_syspath_write(const char *path) {
    console_set_color(0x00FACC15, 0x00090D16);
    console_puts("\n[KERNEL] Access Denied (EACCES): Protected system path -> ");
    console_puts(path ? path : "(null)");
    console_puts("\n");
    console_set_color(0x00E2E8F0, 0x00090D16);
    console_flush_now();
}
extern uint32_t __attribute__((weak)) g_mouse_dbg;
extern uint64_t kernel_dev_fb_write(uint64_t offset, const void *buf, uint64_t len);
static uint64_t dev_fb_pos = 0;

static int dev_open_path(const char *p) {
    if (!p) return -1;
    while (*p == '/' || *p == '\\') p++;
    if (p[0]=='d' && p[1]=='e' && p[2]=='v' && (p[3]=='/' || p[3]=='\\')) {
        const char *n = p + 4;
        if (strcmp(n, "null") == 0)  return 100;
        if (strcmp(n, "zero") == 0)  return 101;
        if (strcmp(n, "kbd") == 0)   return 102;
        if (strcmp(n, "mouse") == 0) return 103;
        if (strcmp(n, "fb0") == 0)   { dev_fb_pos = 0; return 104; }
        if (strcmp(n, "dsp") == 0 || strcmp(n, "audio") == 0) return 105;
        if (strcmp(n, "speaker") == 0) return 106;
    }
    return -1;
}
uint64_t syscall_dispatch(SyscallFrame *f, uint64_t num) {
    uint64_t a1 = f->rdi, a2 = f->rsi, a3 = f->rdx;
    sched_tick();
    switch (num) {
        case SYS_EXIT:
            int d = (r3_depth > 0) ? (r3_depth - 1) : 0; r3_exit_code[d] = (int)a1;
            longjmp(r3_ret_jmp[d], 1);
            return 0;
        case SYS_WRITE:
            console_write((const char *)a1, (size_t)a2);
            return a2;
        case SYS_GETCHAR:
            return (uint64_t)(int64_t)console_getchar((int)a1);
        case SYS_CLEAR:
            console_clear();
            return 0;
        case SYS_OPEN: { int dfd = dev_open_path((const char *)a1); if (dfd >= 100) return (uint64_t)dfd;
            int wr = 0; if ((const char *)a2) { for (const char *m = (const char *)a2; *m; m++) if (*m=='w'||*m=='a'||*m=='+') wr=1; } if (wr && is_protected_syspath((const char *)a1)) { deny_syspath_write((const char *)a1); return (uint64_t)-13; }
            Fat32File *fp = fat32_open((const char *)a1, (const char *)a2);
            if (!fp) return (uint64_t)-1LL;
            for (int i = 0; i < 32; i++) {
                if (!fd_tab[i]) { fd_tab[i] = fp; return (uint64_t)i; }
            }
            fat32_close(fp);
            return (uint64_t)-1LL;
        }
        case SYS_READ: {
            if ((int)a1 == 100) return 0;
            if ((int)a1 == 101) { if (a2 && a3) memset((void *)a2, 0, (size_t)a3); return a3; }
            if ((int)a1 == 105) {
                if (!user_ptr_ok((void *)a2, (size_t)a3) || a3 == 0) return 0;
                const char *nm = audio_backend_name();
                char *dst = (char *)a2; size_t i = 0;
                while (nm[i] && i + 1 < (size_t)a3) { dst[i] = nm[i]; i++; }
                dst[i] = 0;
                return (uint64_t)i;
            }
            if ((int)a1 == 102 || (int)a1 == 103) {
                InputState st; memset(&st, 0, sizeof(st));
                if (sc_info && sc_info->poll_input) sc_info->poll_input(&st);
                SonionSysInfo *si = (SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;
                si->mouse_x = st.mouse_x; si->mouse_y = st.mouse_y;
                si->mouse_btns = st.left_click ? 1 : 0;
                if (st.key_char) si->last_key = (uint8_t)st.key_char;
                if ((int)a1 == 102 && a2 && a3 >= 1) {
                    ((char *)a2)[0] = st.key_char;
                    if (a3 >= 2) ((char *)a2)[1] = (char)st.scan_code;
                    return (a3 >= 2) ? 2 : 1;
                }
                if ((int)a1 == 103 && a2 && a3 >= 12) {
                    int32_t *m = (int32_t *)a2;
                    m[0] = st.mouse_x; m[1] = st.mouse_y; m[2] = st.left_click ? 1 : 0;
                    if (a3 >= 16) { m[3] = (int32_t)g_mouse_dbg; return 16; }
                    return 12;
                }
                return 0;
            }
            if (a1 >= 32 || !fd_tab[a1]) return 0;
            return (uint64_t)fat32_read((void *)a2, 1, (size_t)a3, fd_tab[a1]);
        }
        case SYS_FWRITE:
            if ((int)a1 == 100) return a3;
            if ((int)a1 == 104) { uint64_t w = kernel_dev_fb_write(dev_fb_pos, (const void *)a2, a3); dev_fb_pos += w; return w; }
            if ((int)a1 == 105) {
                if (!user_ptr_ok((void *)a2, (size_t)a3)) return 0;
                return (uint64_t)audio_write_pcm((const void *)a2, (size_t)a3);
            }
            if ((int)a1 == 106) {
                if (!user_ptr_ok((void *)a2, 8) || a3 < 8) return 0;
                const uint32_t *tone = (const uint32_t *)a2;
                speaker_beep(tone[0], tone[1]);
                return 8;
            }
            if (a1 >= 32 || !fd_tab[a1]) return 0;
            return (uint64_t)fat32_write((const void *)a2, 1, (size_t)a3, fd_tab[a1]);
        case SYS_SEEK:
            if ((int)a1 == 104) { dev_fb_pos = (uint64_t)a2; return 0; }
            if ((int)a1 >= 100) return 0;
            if (a1 >= 32 || !fd_tab[a1]) return (uint64_t)-1LL;
            return (uint64_t)(int64_t)fat32_seek(fd_tab[a1], (int64_t)a2, (int)a3);
        case SYS_TELL:
            if ((int)a1 == 104) return dev_fb_pos;
            if ((int)a1 >= 100) return 0;
            if (a1 >= 32 || !fd_tab[a1]) return (uint64_t)-1LL;
            return (uint64_t)fat32_tell(fd_tab[a1]);
        case SYS_CLOSE:
            if (a1 < 32 && fd_tab[a1]) { fat32_close(fd_tab[a1]); fd_tab[a1] = 0; }
            return 0;
        case SYS_MKDIR:
            return (uint64_t)(int64_t)fat32_mkdir((const char *)a1);
        case SYS_REMOVE: if (is_protected_syspath((const char *)a1)) { deny_syspath_write((const char *)a1); return (uint64_t)-13; }
            return (uint64_t)(int64_t)fat32_remove((const char *)a1);
        case SYS_OPENDIR: {
            Fat32Dir *d = fat32_opendir((const char *)a1);
            if (!d) return (uint64_t)-1LL;
            for (int i = 0; i < 8; i++) {
                if (!dir_tab[i]) { dir_tab[i] = d; return (uint64_t)i; }
            }
            fat32_closedir(d);
            return (uint64_t)-1LL;
        }
        case SYS_READDIR:
            if (a1 >= 8 || !dir_tab[a1]) return 0;
            return (uint64_t)fat32_readdir(dir_tab[a1], (Fat32DirEntry *)a2);
        case SYS_CLOSEDIR:
            if (a1 < 8 && dir_tab[a1]) { fat32_closedir(dir_tab[a1]); dir_tab[a1] = 0; }
            return 0;
        case SYS_TIME:
            return (uint64_t)time(0);
        case SYS_CLOCK:
            return (uint64_t)clock();
        case SYS_REBOOT:
            sys_hw_reboot();
            return 0;
        case SYS_POWEROFF:
            if (sc_info && sc_info->shutdown) sc_info->shutdown();
            return 0;
        case SYS_DISK_INFO: {
            if (!user_ptr_ok((void *)a1, 7 * sizeof(uint64_t))) return (uint64_t)-14; uint64_t *out = (uint64_t *)a1;
            if (out) {
                out[0] = block_get_sector_count();
                out[1] = (uint64_t)fat32_is_mounted();
                strcpy((char *)(out + 2), block_get_driver_name());
                strcpy((char *)(out + 4), fat32_get_type_name());
            }
            return 0;
        }
        case SYS_EXEC:
            return (uint64_t)(int64_t)ring3_exec_child((const char *)a1);
        case SYS_BATCH: {
            /* io_uring-style batch: a1 = ptr to uint64_t[count][4] {num,a1,a2,a3}, a2 = count */
            uint64_t (*ops)[4] = (uint64_t (*)[4])a1;
            uint64_t cnt = a2, last = 0;
            for (uint64_t i = 0; i < cnt; i++) {
                SyscallFrame sub = *f;
                sub.rdi = ops[i][1];
                sub.rsi = ops[i][2];
                sub.rdx = ops[i][3];
                last = syscall_dispatch(&sub, ops[i][0]);
            }
            return last;
        }
        case 30: { /* SYS_PS: copy compact task list to user buffer */
            typedef struct { int pid, state, cpl; uint32_t mem_kb; uint64_t ticks; char name[24]; } UserTask;
            if (!user_ptr_ok((void *)a1, sizeof(UserTask) * (size_t)a2)) return (uint64_t)-14; UserTask *out = (UserTask *)a1;
            int max = (int)a2, n = 0;
            for (int i = 0; i < MAX_TASKS && n < max; i++) {
                TaskInfo *t = sched_get(i);
                if (t && t->state != TASK_UNUSED) {
                    out[n].pid = t->pid; out[n].state = t->state;
                    out[n].cpl = t->cpl; out[n].mem_kb = t->mem_kb;
                    out[n].ticks = t->ticks;
                    for (int k = 0; k < 24; k++) out[n].name[k] = t->name[k];
                    n++;
                }
            }
            return (uint64_t)n;
        }
        case 31: /* SYS_KILL */
            return (uint64_t)(int64_t)sched_kill((int)a1);
        case 32: /* SYS_SPAWN_BG */
            return (uint64_t)(int64_t)sched_register_task((const char *)a1, 3, 256);
        case 33: { /* SYS_KMEM: Ring 0 Kernel RAM Telemetry */
            if (!user_ptr_ok((void *)a1, 7 * sizeof(uint64_t))) return (uint64_t)-14; uint64_t *out = (uint64_t *)a1;
            size_t tb = 0, ub = 0, fb = 0, uc = 0, fc = 0;
            kheap_stats(&tb, &ub, &fb, &uc, &fc);
            out[0] = tb; /* Kernel heap total bytes */
            out[1] = ub; /* Kernel heap allocated bytes */
            out[2] = fb; /* Kernel heap free bytes */
            out[3] = uc; /* Active kernel heap blocks */
            out[4] = fc; /* Free kernel heap blocks */
            out[5] = (sc_info ? (uint64_t)sc_info->width * sc_info->height * 4ULL : 0); /* Kernel backbuffer */
            out[6] = 56 * 1024ULL; /* Ring 0 kernel + IST1 + scheduler stacks */
            return 0;
        }
        case 34: { /* SYS_KEY_EVENT: pop Key-Down / Key-Up event */
            if (!user_ptr_ok((void *)a1, sizeof(KeyEvent))) return (uint64_t)-14;
            return (uint64_t)kbd_pop_event((KeyEvent *)a1);
        }
        default:
            return (uint64_t)-1LL;
    }
}

__attribute__((naked)) static void syscall_entry(void) {
    __asm__ volatile (
        "movq %%rsp, %0\n\t"
        "movq %1, %%rsp\n\t"
        "pushq %0\n\t"
        "pushq %%rcx\n\t"
        "pushq %%r11\n\t"
        "pushq %%rbx\n\t"
        "pushq %%rbp\n\t"
        "pushq %%rdi\n\t"
        "pushq %%rsi\n\t"
        "pushq %%rdx\n\t"
        "pushq %%r8\n\t"
        "pushq %%r9\n\t"
        "pushq %%r10\n\t"
        "movq %%rsp, %%rcx\n\t"
        "movq %%rax, %%rdx\n\t"
        "movq %%rsp, %%rbx\n\t"
        "andq $-16, %%rsp\n\t"
        "subq $32, %%rsp\n\t"
        "call syscall_dispatch\n\t"
        "movq %%rbx, %%rsp\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        "popq %%rdx\n\t"
        "popq %%rsi\n\t"
        "popq %%rdi\n\t"
        "popq %%rbp\n\t"
        "popq %%rbx\n\t"
        "popq %%r11\n\t"
        "popq %%rcx\n\t"
        "popq %%rsp\n\t"
        "sysretq\n\t"
        : "+m"(sc_user_rsp) : "m"(sc_kstack_top) : "memory"
    );
}

void syscall_init(BootInfo *info) {
    sc_info = info;
    sc_kstack_top = (uint64_t)(uintptr_t)(sc_kstack[0] + 16384);
    memset(fd_tab, 0, sizeof(fd_tab));
    memset(dir_tab, 0, sizeof(dir_tab));

    uint32_t tlo, thi;
    __asm__ volatile ("rdtsc" : "=a"(tlo), "=d"(thi));
    SonionSysInfo *si = (SonionSysInfo *)(uintptr_t)SYSINFO_PAGE_ADDR;
    si->magic      = SYSINFO_MAGIC;
    si->boot_tsc   = ((uint64_t)thi << 32) | tlo;
    si->tsc_per_ms = 2500000ULL;
    si->unix_base  = (uint64_t)time(0);
    si->screen_w   = info ? info->width : 1280;
    si->screen_h   = info ? info->height : 800;
    uint64_t efer = rdmsr(0xC0000080U);
    wrmsr(0xC0000080U, efer | 1ULL);

    uint64_t star = ((uint64_t)GDT_KERNEL_DATA << 48) | ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(0xC0000081U, star);
    wrmsr(0xC0000082U, (uint64_t)(uintptr_t)syscall_entry);
    wrmsr(0xC0000084U, 0x200ULL);
}

static int ring3_launch(const char *path, uint64_t load_addr, uint64_t stack_top, size_t max_bytes) {
    Fat32File *fp = fat32_open(path, "rb");
    if (!fp) return -1;

    uint8_t *dest = (uint8_t *)(uintptr_t)load_addr;
    memset(dest, 0, max_bytes);
    size_t n = fat32_read(dest, 1, max_bytes, fp);
    fat32_close(fp);
    if (n == 0 || r3_depth >= 2) return -2;

    int slot = r3_depth++;
    sc_kstack_top = (uint64_t)(uintptr_t)(sc_kstack[slot] + 16384);
    r3_exit_code[slot] = 0;
    if (setjmp(r3_ret_jmp[slot]) == 0) {
        uint64_t u_ss = GDT_USER_DATA | 3;
        uint64_t u_cs = GDT_USER_CODE | 3;
        __asm__ volatile (
            "movw %w0, %%ds\n\t"
            "movw %w0, %%es\n\t"
            "movw %w0, %%fs\n\t"
            "movw %w0, %%gs\n\t"
            "pushq %0\n\t"
            "pushq %1\n\t"
            "pushq $0x002\n\t"
            "pushq %2\n\t"
            "pushq %3\n\t"
            "iretq\n\t"
            : : "r"(u_ss), "r"(stack_top), "r"(u_cs), "r"(load_addr) : "memory"
        );
    }
    r3_depth--;
    sc_kstack_top = (uint64_t)(uintptr_t)(sc_kstack[r3_depth > 0 ? r3_depth - 1 : 0] + 16384);
    return r3_exit_code[slot];
}

int ring3_exec(const char *path) {
    return ring3_launch(path, USER_BASE_ADDR, USER_STACK_TOP, 2 * 1024 * 1024);
}

int ring3_exec_child(const char *path) {
    return ring3_launch(path, 0x02800000ULL, 0x037FFFF0ULL, 4 * 1024 * 1024);
}



void ring3_fault_recover(uint64_t exc, uint64_t rip, uint64_t cr2) {
    if (r3_depth <= 0) return;
    int d = r3_depth - 1;
    console_set_color(0x00FACC15, 0x00090D16);
    console_puts("\n[KERNEL] Ring 3 Fault Caught (EXC=");
    char hex[20];
    for (int i = 0; i < 16; i++) {
        uint8_t nib = (rip >> ((15 - i) * 4)) & 0xF;
        hex[i] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
    }
    hex[16] = 0;
    console_puts(exc == 14 ? "14 #PF" : (exc == 13 ? "13 #GP" : "FAULT"));
    console_puts(", RIP=0x");
    console_puts(hex);
    console_puts(") -> Terminating faulty app!\n");
    console_set_color(0x00E2E8F0, 0x00090D16);
    console_flush_now();
    char c2hex[20];
    for (int i = 0; i < 16; i++) {
        uint8_t nib = (cr2 >> ((15 - i) * 4)) & 0xF;
        c2hex[i] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
    }
    c2hex[16] = 0;
    console_puts(" CR2=0x"); console_puts(c2hex); console_puts("\n"); console_flush_now();
    r3_depth = d;
    sc_kstack_top = (uint64_t)(uintptr_t)(sc_kstack[d > 0 ? d - 1 : 0] + 16384);
    r3_exit_code[d] = -(int)(128 + exc);
    longjmp(r3_ret_jmp[d], 1);
}
































