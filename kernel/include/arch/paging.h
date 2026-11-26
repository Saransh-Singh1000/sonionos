#ifndef SONION_PAGING_H
#define SONION_PAGING_H

#include <stdint.h>
#include "bootinfo.h"

#define USER_BASE_ADDR  0x01000000ULL
#define USER_LIMIT_ADDR 0x04000000ULL
#define USER_STACK_TOP  0x03FEFFF0ULL

void paging_init(BootInfo *info);

#endif


