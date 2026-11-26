#ifndef SONION_SHELL_H
#define SONION_SHELL_H

#include "bootinfo.h"

void shell_init(BootInfo *info);
void shell_exec(const char *cmdline);

const char *shell_get_cwd(void);
#endif

