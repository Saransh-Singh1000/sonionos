#include "drivers/nvme.h"
#include "drivers/pci.h"
#include "blockdev.h"
#include "string.h"

typedef struct __attribute__((packed)) {
    uint8_t  opc;
    uint8_t  flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsv0;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} NvmeCmd;

typedef struct __attribute__((packed)) {
    uint32_t dw0;
    uint32_t dw1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} NvmeCqe;

#define NVME_Q_DEPTH 16

static NvmeCmd admin_sq[NVME_Q_DEPTH] __attribute__((aligned(4096)));
static NvmeCqe admin_cq[NVME_Q_DEPTH] __attribute__((aligned(4096)));
static NvmeCmd io_sq[NVME_Q_DEPTH]    __attribute__((aligned(4096)));
static NvmeCqe io_cq[NVME_Q_DEPTH]    __attribute__((aligned(4096)));
static uint8_t nvme_dma_page[4096]    __attribute__((aligned(4096)));

static volatile uint8_t *nvme_mmio = 0;
static uint32_t db_stride = 4;

static uint16_t asq_tail = 0, acq_head = 0, acq_phase = 1;
static uint16_t isq_tail = 0, icq_head = 0, icq_phase = 1;
static uint16_t next_cid = 1;

static uint32_t native_block_size = 512;
static uint64_t total_512_sectors = 0;

static inline uint32_t nvme_read32(uint32_t off) {
    return *(volatile uint32_t *)(nvme_mmio + off);
}

static inline void nvme_write32(uint32_t off, uint32_t val) {
    *(volatile uint32_t *)(nvme_mmio + off) = val;
}

static inline uint64_t nvme_read64(uint32_t off) {
    uint32_t lo = nvme_read32(off);
    uint32_t hi = nvme_read32(off + 4);
    return ((uint64_t)hi << 32) | lo;
}

static inline void nvme_write64(uint32_t off, uint64_t val) {
    nvme_write32(off, (uint32_t)(val & 0xFFFFFFFFULL));
    nvme_write32(off + 4, (uint32_t)(val >> 32));
}

static inline void nvme_ring_sq_db(uint16_t qid, uint16_t tail) {
    nvme_write32(0x1000 + (2 * qid) * db_stride, tail);
}

static inline void nvme_ring_cq_db(uint16_t qid, uint16_t head) {
    nvme_write32(0x1000 + (2 * qid + 1) * db_stride, head);
}

static int nvme_submit_sync(uint16_t qid, NvmeCmd *sq, NvmeCqe *cq,
                            uint16_t *sq_tail, uint16_t *cq_head, uint16_t *cq_phase,
                            const NvmeCmd *cmd) {
    uint16_t tail = *sq_tail;
    sq[tail] = *cmd;
    sq[tail].cid = next_cid++;

    tail = (tail + 1) % NVME_Q_DEPTH;
    *sq_tail = tail;
    nvme_ring_sq_db(qid, tail);

    uint16_t head = *cq_head;
    uint16_t phase = *cq_phase;

    for (uint32_t timeout = 0; timeout < 50000000U; timeout++) {
        volatile NvmeCqe *entry = &cq[head];
        uint16_t st = entry->status;
        if ((st & 1) == phase) {
            uint16_t err = (st >> 1) & 0x7FFF;
            head++;
            if (head >= NVME_Q_DEPTH) {
                head = 0;
                phase ^= 1;
            }
            *cq_head = head;
            *cq_phase = phase;
            nvme_ring_cq_db(qid, head);
            return (err == 0) ? 0 : -1;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static int nvme_admin_cmd(const NvmeCmd *cmd) {
    return nvme_submit_sync(0, admin_sq, admin_cq, &asq_tail, &acq_head, &acq_phase, cmd);
}

static int nvme_io_cmd(const NvmeCmd *cmd) {
    return nvme_submit_sync(1, io_sq, io_cq, &isq_tail, &icq_head, &icq_phase, cmd);
}

static int nvme_rw_native_blocks(uint64_t dev_lba, uint16_t nblocks, int write) {
    NvmeCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc   = write ? 0x01 : 0x02; // NVM Write (0x01) / NVM Read (0x02)
    cmd.nsid  = 1;
    cmd.prp1  = (uint64_t)(uintptr_t)nvme_dma_page;
    cmd.cdw10 = (uint32_t)(dev_lba & 0xFFFFFFFFULL);
    cmd.cdw11 = (uint32_t)(dev_lba >> 32);
    cmd.cdw12 = (uint32_t)(nblocks - 1);
    return nvme_io_cmd(&cmd);
}

static int nvme_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    uint8_t *dst = (uint8_t *)buf;
    if (native_block_size == 512) {
        while (count > 0) {
            uint16_t chunk = (count > 8) ? 8 : (uint16_t)count;
            if (nvme_rw_native_blocks(lba, chunk, 0) != 0) return -1;
            memcpy(dst, nvme_dma_page, chunk * 512);
            dst += chunk * 512;
            lba += chunk;
            count -= chunk;
        }
        return 0;
    }

    // 4K (4096-byte) Native NVMe Translation
    while (count > 0) {
        uint64_t dev_lba = lba / 8;
        uint32_t sub_off = (uint32_t)(lba % 8);
        uint32_t avail   = 8 - sub_off;
        uint32_t take    = (count < avail) ? count : avail;

        if (nvme_rw_native_blocks(dev_lba, 1, 0) != 0) return -1;
        memcpy(dst, nvme_dma_page + sub_off * 512, take * 512);
        dst += take * 512;
        lba += take;
        count -= take;
    }
    return 0;
}

static int nvme_write_sectors(uint64_t lba, uint32_t count, const void *buf) {
    const uint8_t *src = (const uint8_t *)buf;
    if (native_block_size == 512) {
        while (count > 0) {
            uint16_t chunk = (count > 8) ? 8 : (uint16_t)count;
            memcpy(nvme_dma_page, src, chunk * 512);
            if (nvme_rw_native_blocks(lba, chunk, 1) != 0) return -1;
            src += chunk * 512;
            lba += chunk;
            count -= chunk;
        }
        return 0;
    }

    // 4K (4096-byte) Native NVMe Read-Modify-Write
    while (count > 0) {
        uint64_t dev_lba = lba / 8;
        uint32_t sub_off = (uint32_t)(lba % 8);
        uint32_t avail   = 8 - sub_off;
        uint32_t take    = (count < avail) ? count : avail;

        if (take < 8) {
            if (nvme_rw_native_blocks(dev_lba, 1, 0) != 0) return -1;
        }
        memcpy(nvme_dma_page + sub_off * 512, src, take * 512);
        if (nvme_rw_native_blocks(dev_lba, 1, 1) != 0) return -1;

        src += take * 512;
        lba += take;
        count -= take;
    }
    return 0;
}

int nvme_init(void) {
    PciDevice dev;
    // PCI Class 0x01 (Mass Storage), Subclass 0x08 (NVM), ProgIF 0x02 (NVMe)
    if (!pci_find_class(0x01, 0x08, 0x02, &dev)) return 0;

    pci_enable_bus_master_mmio(&dev);
    if (!dev.bar0) return 0;

    nvme_mmio = (volatile uint8_t *)(uintptr_t)dev.bar0;
    uint64_t cap = nvme_read64(0x00);
    uint32_t dstrd = (uint32_t)((cap >> 32) & 0xF);
    db_stride = 4U << dstrd;

    // 1. Reset Controller (CC.EN = 0) and wait for CSTS.RDY == 0
    uint32_t cc = nvme_read32(0x14);
    if (cc & 1) {
        nvme_write32(0x14, cc & ~1U);
        for (uint32_t t = 0; t < 50000000U; t++) {
            if (!(nvme_read32(0x1C) & 1)) break;
            __asm__ volatile ("pause");
        }
    }

    memset(admin_sq, 0, sizeof(admin_sq));
    memset(admin_cq, 0, sizeof(admin_cq));
    memset(io_sq,    0, sizeof(io_sq));
    memset(io_cq,    0, sizeof(io_cq));
    asq_tail = acq_head = 0; acq_phase = 1;
    isq_tail = icq_head = 0; icq_phase = 1;

    // 2. Configure Admin Queue Attributes & Base Addresses
    nvme_write32(0x24, ((NVME_Q_DEPTH - 1) << 16) | (NVME_Q_DEPTH - 1));
    nvme_write64(0x28, (uint64_t)(uintptr_t)admin_sq);
    nvme_write64(0x30, (uint64_t)(uintptr_t)admin_cq);

    // 3. Enable Controller (IOSQES=6 [64B], IOCQES=4 [16B], MPS=0 [4KB], CSS=0, EN=1)
    cc = (4U << 20) | (6U << 16) | 1U;
    nvme_write32(0x14, cc);

    int ready = 0;
    for (uint32_t t = 0; t < 50000000U; t++) {
        uint32_t csts = nvme_read32(0x1C);
        if (csts & 2) return 0; // CFS: Controller Fatal Status
        if (csts & 1) { ready = 1; break; }
        __asm__ volatile ("pause");
    }
    if (!ready) return 0;

    // 4. Identify Namespace 1 (CNS = 0) to detect 512B vs 4096B LBA size & total capacity
    memset(nvme_dma_page, 0, sizeof(nvme_dma_page));
    NvmeCmd id_cmd;
    memset(&id_cmd, 0, sizeof(id_cmd));
    id_cmd.opc   = 0x06;
    id_cmd.nsid  = 1;
    id_cmd.prp1  = (uint64_t)(uintptr_t)nvme_dma_page;
    id_cmd.cdw10 = 0;
    if (nvme_admin_cmd(&id_cmd) != 0) return 0;

    uint64_t nsze = *(uint64_t *)(nvme_dma_page + 0);
    uint8_t flbas = nvme_dma_page[26] & 0x0F;
    uint32_t lbaf = *(uint32_t *)(nvme_dma_page + 128 + flbas * 4);
    uint8_t lbads = (uint8_t)((lbaf >> 16) & 0xFF);
    native_block_size = (lbads >= 9) ? (1U << lbads) : 512U;
    total_512_sectors = nsze * (native_block_size / 512U);

    // 5. Create I/O Completion Queue (QID = 1)
    NvmeCmd cq_cmd;
    memset(&cq_cmd, 0, sizeof(cq_cmd));
    cq_cmd.opc   = 0x05;
    cq_cmd.prp1  = (uint64_t)(uintptr_t)io_cq;
    cq_cmd.cdw10 = ((NVME_Q_DEPTH - 1) << 16) | 1U;
    cq_cmd.cdw11 = 1U; // Physically Contiguous
    if (nvme_admin_cmd(&cq_cmd) != 0) return 0;

    // 6. Create I/O Submission Queue (QID = 1 -> CQID = 1)
    NvmeCmd sq_cmd;
    memset(&sq_cmd, 0, sizeof(sq_cmd));
    sq_cmd.opc   = 0x01;
    sq_cmd.prp1  = (uint64_t)(uintptr_t)io_sq;
    sq_cmd.cdw10 = ((NVME_Q_DEPTH - 1) << 16) | 1U;
    sq_cmd.cdw11 = (1U << 16) | 1U; // CQID = 1, Physically Contiguous
    if (nvme_admin_cmd(&sq_cmd) != 0) return 0;

    BlockDriver drv = {
        .name          = "PCIe-NVMe",
        .sector_count  = total_512_sectors,
        .sector_size   = 512,
        .read_sectors  = nvme_read_sectors,
        .write_sectors = nvme_write_sectors
    };
    block_register(&drv);
    return 1;
}
