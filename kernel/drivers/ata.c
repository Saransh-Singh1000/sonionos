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
    if (inb(io + ATA_REG_STATUS) == 0xFF) return 0;

    outb(io + ATA_REG_HDDEVSEL, 0xA0 | (slave << 4));
    ata_400ns_delay(ctrl);

    outb(io + ATA_REG_SECCOUNT0, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_400ns_delay(ctrl);

    uint8_t st = inb(io + ATA_REG_STATUS);
    if (st == 0) return 0;

    if (inb(io + ATA_REG_LBA1) != 0 || inb(io + ATA_REG_LBA2) != 0) return 0;
    if (ata_poll(io, 1) != 0) return 0;

    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(io + ATA_REG_DATA);

    uint64_t lba48 = ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) |
                     ((uint64_t)id[101] << 16) | (uint64_t)id[100];
    uint32_t lba28 = ((uint32_t)id[61] << 16) | (uint32_t)id[60];

    ata_sectors  = lba48 ? lba48 : (uint64_t)lba28;
    active_io    = io;
    active_ctrl  = ctrl;
    active_slave = slave;
    return ata_sectors > 0;
}

static int ata_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    uint16_t *dst = (uint16_t *)buf;
    while (count > 0) {
        uint16_t chunk = (count > 256) ? 256 : (uint16_t)count;
        uint8_t sc = (chunk == 256) ? 0 : (uint8_t)chunk;

        if (ata_poll(active_io, 0) != 0) return -1;
        outb(active_io + ATA_REG_HDDEVSEL, 0x40 | (active_slave << 4));
        ata_400ns_delay(active_ctrl);

        outb(active_io + ATA_REG_SECCOUNT0, (uint8_t)(sc >> 8));
        outb(active_io + ATA_REG_LBA0, (uint8_t)(lba >> 24));
        outb(active_io + ATA_REG_LBA1, (uint8_t)(lba >> 32));
        outb(active_io + ATA_REG_LBA2, (uint8_t)(lba >> 40));

        outb(active_io + ATA_REG_SECCOUNT0, sc);
        outb(active_io + ATA_REG_LBA0, (uint8_t)(lba));
        outb(active_io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(active_io + ATA_REG_LBA2, (uint8_t)(lba >> 16));

        outb(active_io + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
        ata_400ns_delay(active_ctrl);

        for (uint16_t s = 0; s < chunk; s++) {
            if (ata_poll(active_io, 1) != 0) return -1;
            for (int w = 0; w < 256; w++) *dst++ = inw(active_io + ATA_REG_DATA);
        }
        lba += chunk;
        count -= chunk;
    }
    return 0;
}

static int ata_write_sectors(uint64_t lba, uint32_t count, const void *buf) {
    const uint16_t *src = (const uint16_t *)buf;
    while (count > 0) {
        uint16_t chunk = (count > 256) ? 256 : (uint16_t)count;
        uint8_t sc = (chunk == 256) ? 0 : (uint8_t)chunk;

        if (ata_poll(active_io, 0) != 0) return -1;
        outb(active_io + ATA_REG_HDDEVSEL, 0x40 | (active_slave << 4));
        ata_400ns_delay(active_ctrl);

        outb(active_io + ATA_REG_SECCOUNT0, (uint8_t)(sc >> 8));
        outb(active_io + ATA_REG_LBA0, (uint8_t)(lba >> 24));
        outb(active_io + ATA_REG_LBA1, (uint8_t)(lba >> 32));
        outb(active_io + ATA_REG_LBA2, (uint8_t)(lba >> 40));

        outb(active_io + ATA_REG_SECCOUNT0, sc);
        outb(active_io + ATA_REG_LBA0, (uint8_t)(lba));
        outb(active_io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(active_io + ATA_REG_LBA2, (uint8_t)(lba >> 16));

        outb(active_io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
        ata_400ns_delay(active_ctrl);

        for (uint16_t s = 0; s < chunk; s++) {
            if (ata_poll(active_io, 1) != 0) return -1;
            for (int w = 0; w < 256; w++) outw(active_io + ATA_REG_DATA, *src++);
        }
        outb(active_io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
        ata_poll(active_io, 0);

        lba += chunk;
        count -= chunk;
    }
    return 0;
}

int ata_init(void) {
    if (ata_probe_drive(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0) ||
        ata_probe_drive(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 1) ||
        ata_probe_drive(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 0) ||
        ata_probe_drive(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 1)) {
        BlockDriver drv = {
            .name          = "ATA-PIO",
            .sector_count  = ata_sectors,
            .sector_size   = 512,
            .read_sectors  = ata_read_sectors,
            .write_sectors = ata_write_sectors
        };
        block_register(&drv);
        return 1;
    }
    return 0;
}
