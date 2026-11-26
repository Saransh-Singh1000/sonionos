#include "blockdev.h"
#include "drivers/nvme.h"
#include "drivers/ahci.h"
#include "drivers/ata.h"
#include "drivers/usb_msc.h"

static BlockDriver active_driver = { "NONE", 0, 512, 0, 0 };

void block_register(const BlockDriver *drv) {
    if (drv && drv->read_sectors) active_driver = *drv;
}

void block_init(BootInfo *info) {
    (void)info;
    if (nvme_init())    return;
    if (ahci_init())    return;
    if (ata_init())     return;
    if (usb_msc_init()) return;
}

int block_read(uint64_t lba, uint32_t count, void *buf) {
    if (!active_driver.read_sectors || count == 0) return -1;
    return active_driver.read_sectors(lba, count, buf);
}

int block_write(uint64_t lba, uint32_t count, const void *buf) {
    if (!active_driver.write_sectors || count == 0) return -1;
    return active_driver.write_sectors(lba, count, buf);
}

const char *block_get_driver_name(void) { return active_driver.name; }
uint64_t block_get_sector_count(void)   { return active_driver.sector_count; }
