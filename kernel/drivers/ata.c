#include "drivers/ata.h"
#include "blockdev.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0x00
#define ATA_REG_SECCOUNT0   0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07
#define ATA_REG_STATUS      0x07

#define ATA_CMD_READ_PIO_EXT   0x24
#define ATA_CMD_WRITE_PIO_EXT  0x34
#define ATA_CMD_CACHE_FLUSH    0xE7
#define ATA_CMD_IDENTIFY       0xEC

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static uint16_t active_io    = 0;
static uint16_t active_ctrl  = 0;
static uint8_t  active_slave = 0;
static uint64_t ata_sectors  = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void ata_400ns_delay(uint16_t ctrl) {
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);
}

static int ata_poll(uint16_t io, int check_drq) {
    for (int timeout = 0; timeout < 1000000; timeout++) {
        uint8_t st = inb(io + ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return -1;
        if (!(st & ATA_SR_BSY)) {
            if (!check_drq || (st & ATA_SR_DRQ)) return 0;
        }
    }
    return -1;
}

static int ata_probe_drive(uint16_t io, uint16_t ctrl, uint8_t slave) {
    outb(ctrl, 0x02);
    outb(io + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (slave << 4)));
    ata_400ns_delay(ctrl);
    outb(io + ATA_REG_SECCOUNT0, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_400ns_delay(ctrl);
    uint8_t st = inb(io + ATA_REG_STATUS);
    if (st == 0 || st == 0xFF) return 0;
    if (ata_poll(io, 1) != 0) return 0;
    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(io + ATA_REG_DATA);
    uint64_t secs = ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) |
                    ((uint64_t)id[101] << 16) | (uint64_t)id[100];
    if (secs == 0) secs = ((uint64_t)id[61] << 16) | (uint64_t)id[60];
    if (secs == 0) return 0;
    active_io = io; active_ctrl = ctrl; active_slave = slave; ata_sectors = secs;
    return 1;
}

static int ata_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    if (!active_io || !buf || count == 0) return -1;
    uint32_t *dst = (uint32_t *)buf;
    while (count > 0) {
        if (ata_poll(active_io, 0) != 0) return -1;
        outb(active_io + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | (active_slave << 4) | ((lba >> 24) & 0x0F)));
        ata_400ns_delay(active_ctrl);
        outb(active_io + ATA_REG_SECCOUNT0, 1);
        outb(active_io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
        outb(active_io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
        outb(active_io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
        outb(active_io + ATA_REG_COMMAND, 0x20);
        ata_400ns_delay(active_ctrl);
        if (ata_poll(active_io, 1) != 0) return -1;
        __asm__ volatile ("cld; rep insl" : "+D"(dst) : "c"(128), "d"(active_io + ATA_REG_DATA) : "memory");
        ata_400ns_delay(active_ctrl);
        lba++;
        count--;
    }
    return 0;
}

static int ata_write_sectors(uint64_t lba, uint32_t count, const void *buf) {
    if (!active_io || !buf || count == 0) return -1;
    const uint32_t *src = (const uint32_t *)buf;
    while (count > 0) {
        if (ata_poll(active_io, 0) != 0) return -1;
        outb(active_io + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | (active_slave << 4) | ((lba >> 24) & 0x0F)));
        ata_400ns_delay(active_ctrl);
        outb(active_io + ATA_REG_SECCOUNT0, 1);
        outb(active_io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
        outb(active_io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
        outb(active_io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
        outb(active_io + ATA_REG_COMMAND, 0x30);
        ata_400ns_delay(active_ctrl);
        if (ata_poll(active_io, 1) != 0) return -1;
        __asm__ volatile ("cld; rep outsl" : "+S"(src) : "c"(128), "d"(active_io + ATA_REG_DATA) : "memory");
        ata_400ns_delay(active_ctrl);
        outb(active_io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
        ata_poll(active_io, 0);
        lba++;
        count--;
    }
    return 0;
}
static int drive_has_fs(void) {
    uint8_t sec[512];
    if (ata_read_sectors(0, 1, sec) != 0) return 0;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return 0;
    if (sec[450] != 0 || sec[466] != 0 || sec[482] != 0 || sec[498] != 0) return 1;
    return (sec[11] == 0 && sec[12] == 2 && sec[13] > 0);
}

int ata_init(void) {
    uint16_t ios[4]   = { ATA_PRIMARY_IO, ATA_PRIMARY_IO, ATA_SECONDARY_IO, ATA_SECONDARY_IO };
    uint16_t ctrls[4] = { ATA_PRIMARY_CTRL, ATA_PRIMARY_CTRL, ATA_SECONDARY_CTRL, ATA_SECONDARY_CTRL };
    uint8_t  slvs[4]  = { 0, 1, 0, 1 };
    uint16_t best_io = 0, best_ctrl = 0; uint8_t best_slv = 0; uint64_t best_sec = 0;

    for (int d = 0; d < 4; d++) {
        if (ata_probe_drive(ios[d], ctrls[d], slvs[d])) {
            if (!best_io) { best_io = active_io; best_ctrl = active_ctrl; best_slv = active_slave; best_sec = ata_sectors; }
            if (drive_has_fs()) {
                BlockDriver drv = { "ATA-PIO", ata_sectors, 512, ata_read_sectors, ata_write_sectors };
                block_register(&drv); return 1;
            }
        }
    }
    if (best_io) {
        active_io = best_io; active_ctrl = best_ctrl; active_slave = best_slv; ata_sectors = best_sec;
        BlockDriver drv = { "ATA-PIO", ata_sectors, 512, ata_read_sectors, ata_write_sectors };
        block_register(&drv); return 1;
    }
    return 0;
}





