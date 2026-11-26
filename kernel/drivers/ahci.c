#include "drivers/ahci.h"
#include "drivers/pci.h"
#include "blockdev.h"
#include "string.h"

typedef volatile struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} HbaPort;

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[116];
    uint8_t  vendor[96];
    HbaPort  ports[32];
} HbaMem;

typedef struct {
    uint16_t flags;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} HbaCmdHeader;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc;
} HbaPrdtEntry;

typedef struct {
    uint8_t      cfis[64];
    uint8_t      acmd[16];
    uint8_t      rsv[48];
    HbaPrdtEntry prdt[1];
} HbaCmdTbl;

typedef struct __attribute__((packed)) {
    uint8_t  fis_type;
    uint8_t  pmport_c;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1[4];
} FisRegH2D;

static uint8_t ahci_cmd_list[1024]   __attribute__((aligned(1024)));
static uint8_t ahci_fis_buf[256]     __attribute__((aligned(256)));
static uint8_t ahci_cmd_tbl[256]     __attribute__((aligned(128)));
static uint8_t ahci_dma_bounce[65536] __attribute__((aligned(4096)));

static HbaPort *active_port = 0;

static void ahci_stop_cmd(HbaPort *port) {
    port->cmd &= ~(1U << 0);
    port->cmd &= ~(1U << 4);
    while ((port->cmd & (1U << 15)) || (port->cmd & (1U << 14))) {
        __asm__ volatile ("pause");
    }
}

static void ahci_start_cmd(HbaPort *port) {
    while (port->cmd & (1U << 15)) __asm__ volatile ("pause");
    port->cmd |= (1U << 4);
    port->cmd |= (1U << 0);
}

static int ahci_rw_chunk(uint64_t lba, uint16_t count, void *dma_buf, int write) {
    if (!active_port) return -1;
    active_port->is = (uint32_t)-1;

    HbaCmdHeader *hdr = (HbaCmdHeader *)ahci_cmd_list;
    memset((void *)hdr, 0, sizeof(HbaCmdHeader));
    hdr->flags = (sizeof(FisRegH2D) / sizeof(uint32_t)) | (write ? (1 << 6) : 0);
    hdr->prdtl = 1;
    hdr->ctba  = (uint32_t)(uintptr_t)ahci_cmd_tbl;
    hdr->ctbau = (uint32_t)((uintptr_t)ahci_cmd_tbl >> 32);

    HbaCmdTbl *tbl = (HbaCmdTbl *)ahci_cmd_tbl;
    memset(tbl, 0, sizeof(HbaCmdTbl));
    tbl->prdt[0].dba  = (uint32_t)(uintptr_t)dma_buf;
    tbl->prdt[0].dbau = (uint32_t)((uintptr_t)dma_buf >> 32);
    tbl->prdt[0].dbc  = (count * 512) - 1;

    FisRegH2D *fis = (FisRegH2D *)tbl->cfis;
    fis->fis_type = 0x27;
    fis->pmport_c = 0x80;
    fis->command  = write ? 0x35 : 0x25; // WRITE DMA EXT / READ DMA EXT
    fis->lba0     = (uint8_t)lba;
    fis->lba1     = (uint8_t)(lba >> 8);
    fis->lba2     = (uint8_t)(lba >> 16);
    fis->device   = 1 << 6;
    fis->lba3     = (uint8_t)(lba >> 24);
    fis->lba4     = (uint8_t)(lba >> 32);
    fis->lba5     = (uint8_t)(lba >> 40);
    fis->countl   = (uint8_t)count;
    fis->counth   = (uint8_t)(count >> 8);

    for (int t = 0; t < 1000000; t++) {
        if (!(active_port->tfd & (0x80 | 0x08))) break;
    }

    active_port->ci = 1;
    while (1) {
        if (!(active_port->ci & 1)) break;
        if (active_port->is & (1U << 30)) return -1;
    }
    return (active_port->is & (1U << 30)) ? -1 : 0;
}

static int ahci_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    uint8_t *dst = (uint8_t *)buf;
    while (count > 0) {
        uint16_t chunk = (count > 128) ? 128 : (uint16_t)count;
        if (ahci_rw_chunk(lba, chunk, ahci_dma_bounce, 0) != 0) return -1;
        memcpy(dst, ahci_dma_bounce, chunk * 512);
        dst += chunk * 512;
        lba += chunk;
        count -= chunk;
    }
    return 0;
}

static int ahci_write_sectors(uint64_t lba, uint32_t count, const void *buf) {
    const uint8_t *src = (const uint8_t *)buf;
    while (count > 0) {
        uint16_t chunk = (count > 128) ? 128 : (uint16_t)count;
        memcpy(ahci_dma_bounce, src, chunk * 512);
        if (ahci_rw_chunk(lba, chunk, ahci_dma_bounce, 1) != 0) return -1;
        src += chunk * 512;
        lba += chunk;
        count -= chunk;
    }
    return 0;
}

int ahci_init(void) {
    PciDevice dev;
    if (!pci_find_class(0x01, 0x06, 0x01, &dev)) return 0;

    pci_enable_bus_master_mmio(&dev);
    uint64_t abar = pci_read32(dev.bus, dev.slot, dev.func, 0x24) & ~0x1FFFULL;
    if (!abar) return 0;

    HbaMem *hba = (HbaMem *)(uintptr_t)abar;
    hba->ghc |= (1U << 31); // Enable AHCI Mode

    uint32_t pi = hba->pi;
    for (int i = 0; i < 32; i++) {
        if (!(pi & (1U << i))) continue;
        HbaPort *port = &hba->ports[i];
        uint32_t ssts = port->ssts;
        uint8_t det = ssts & 0x0F;
        uint8_t ipm = (ssts >> 8) & 0x0F;
        if (det == 3 && ipm == 1 && port->sig == 0x00000101) {
            ahci_stop_cmd(port);
            memset(ahci_cmd_list, 0, sizeof(ahci_cmd_list));
            memset(ahci_fis_buf, 0, sizeof(ahci_fis_buf));
            port->clb  = (uint32_t)(uintptr_t)ahci_cmd_list;
            port->clbu = (uint32_t)((uintptr_t)ahci_cmd_list >> 32);
            port->fb   = (uint32_t)(uintptr_t)ahci_fis_buf;
            port->fbu  = (uint32_t)((uintptr_t)ahci_fis_buf >> 32);
            port->serr = (uint32_t)-1;
            port->is   = (uint32_t)-1;
            ahci_start_cmd(port);

            active_port = port;
            BlockDriver drv = {
                .name          = "SATA-AHCI",
                .sector_count  = 0xFFFFFFFFULL,
                .sector_size   = 512,
                .read_sectors  = ahci_read_sectors,
                .write_sectors = ahci_write_sectors
            };
            block_register(&drv);
            return 1;
        }
    }
    return 0;
}
