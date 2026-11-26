#include "usyscall.h"

__attribute__((section(".text._start")))
void _start(void) {
    uint16_t cpl = get_current_cpl();
    if (cpl == 3) {
        const char msg[] = "Hello from User Mode (Ring 3, CPL = 3) via SYSCALL!\n";
        sys_write(msg, sizeof(msg) - 1);
        sys_exit(0);
    } else {
        const char err[] = "ERROR: Not running in Ring 3!\n";
        sys_write(err, sizeof(err) - 1);
        sys_exit(1);
    }
}
