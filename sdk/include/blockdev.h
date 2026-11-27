#ifndef SONION_BLOCKDEV_H
#define SONION_BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"

typedef struct {
    const char *name;
    uint64_t    sector_count;
    uint32_t    sector_size;
    int       (*read_sectors)(uint64_t lba, uint32_t count, void *buf);
    int       (*write_sectors)(uint64_t lba, uint32_t count, const void *buf);
} BlockDriver;

void        block_init(BootInfo *info);
void        block_register(const BlockDriver *drv);
int         block_read(uint64_t lba, uint32_t count, void *buf);
int         block_write(uint64_t lba, uint32_t count, const void *buf);
const char *block_get_driver_name(void);
uint64_t    block_get_sector_count(void);

#endif
