#include "drivers/usb_msc.h"
#include "drivers/pci.h"
#include "blockdev.h"
#include "string.h"

typedef struct __attribute__((packed)) {
    uint64_t param;
    uint32_t status, control;
} XhciTrb;

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint32_t size, rsv;
} XhciErstEntry;

typedef struct __attribute__((packed)) {
    uint32_t sig, tag, data_len;
    uint8_t  flags, lun, cb_len, cb[16];
} UsbMscCbw;

typedef struct __attribute__((packed)) {
    uint32_t sig, tag, residue;
    uint8_t  status;
} UsbMscCsw;

static uint64_t      dcbaa[256]      __attribute__((aligned(4096)));
static XhciTrb       cmd_ring[64]    __attribute__((aligned(4096)));
static XhciTrb       evt_ring[64]    __attribute__((aligned(4096)));
static XhciErstEntry erst[1]         __attribute__((aligned(64)));
static uint32_t      dev_ctx[1024]   __attribute__((aligned(4096)));
static uint32_t      in_ctx[1056]    __attribute__((aligned(4096)));
static XhciTrb       ep0_ring[64]    __attribute__((aligned(4096)));
static XhciTrb       bulk_in_ring[64]  __attribute__((aligned(4096)));
static XhciTrb       bulk_out_ring[64] __attribute__((aligned(4096)));
static uint8_t       usb_dma_buf[4096] __attribute__((aligned(4096)));

static volatile uint8_t *xhci_cap = 0, *xhci_op = 0, *xhci_rt = 0;
static volatile uint32_t *xhci_db = 0;

static uint32_t cmd_idx = 0, cmd_pcs = 1, evt_idx = 0, evt_ccs = 1;
static uint32_t ep0_idx = 0, ep0_pcs = 1, bin_idx = 0, bin_pcs = 1, bout_idx = 0, bout_pcs = 1;
static uint8_t  active_slot = 0, bulk_in_dci = 0, bulk_out_dci = 0;
static uint32_t csw_tag = 1;
static uint64_t usb_sectors = 0;

static inline uint32_t mmio_r32(volatile uint8_t *b, uint32_t o) { return *(volatile uint32_t *)(b + o); }
static inline void mmio_w32(volatile uint8_t *b, uint32_t o, uint32_t v) { *(volatile uint32_t *)(b + o) = v; }
static inline void mmio_w64(volatile uint8_t *b, uint32_t o, uint64_t v) {
    mmio_w32(b, o, (uint32_t)v); mmio_w32(b, o + 4, (uint32_t)(v >> 32));
}

static int xhci_wait_event(uint8_t expected_type, XhciTrb *out_evt) {
    for (uint32_t t = 0; t < 50000000U; t++) {
        volatile XhciTrb *e = &evt_ring[evt_idx];
        uint32_t ctrl = e->control;
        if ((ctrl & 1U) == evt_ccs) {
            if (out_evt) *out_evt = *e;
            evt_idx++;
            if (evt_idx >= 63) { evt_idx = 0; evt_ccs ^= 1; }
            uint64_t erdp = (uint64_t)(uintptr_t)&evt_ring[evt_idx];
            mmio_w64(xhci_rt, 0x38, erdp | (1ULL << 3));
            uint8_t type = (uint8_t)((ctrl >> 10) & 0x3F);
            if (expected_type == 0 || type == expected_type) return 0;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static int xhci_send_cmd(uint64_t param, uint32_t status, uint32_t ctrl, XhciTrb *out_evt) {
    cmd_ring[cmd_idx].param   = param;
    cmd_ring[cmd_idx].status  = status;
    cmd_ring[cmd_idx].control = (ctrl & ~1U) | cmd_pcs;
    cmd_idx++;
    if (cmd_idx >= 63) {
        cmd_ring[63].param   = (uint64_t)(uintptr_t)cmd_ring;
        cmd_ring[63].status  = 0;
        cmd_ring[63].control = (6U << 10) | 2U | cmd_pcs;
        cmd_idx = 0; cmd_pcs ^= 1;
    }
    xhci_db[0] = 0;
    return xhci_wait_event(33, out_evt);
}

static int xhci_ctrl_xfer(uint8_t rt, uint8_t rq, uint16_t val, uint16_t idx, void *data, uint16_t len) {
    uint64_t setup = (uint64_t)rt | ((uint64_t)rq << 8) | ((uint64_t)val << 16) |
                     ((uint64_t)idx << 32) | ((uint64_t)len << 48);
    uint32_t trt = (len == 0) ? 0 : ((rt & 0x80) ? 3 : 2);
    ep0_ring[ep0_idx].param = setup; ep0_ring[ep0_idx].status = 8;
    ep0_ring[ep0_idx].control = (2U << 10) | (1U << 6) | (trt << 16) | ep0_pcs; ep0_idx++;
    if (len > 0) {
        ep0_ring[ep0_idx].param = (uint64_t)(uintptr_t)data; ep0_ring[ep0_idx].status = len;
        ep0_ring[ep0_idx].control = (3U << 10) | ((rt & 0x80) ? (1U << 16) : 0) | ep0_pcs; ep0_idx++;
    }
    ep0_ring[ep0_idx].param = 0; ep0_ring[ep0_idx].status = 0;
    ep0_ring[ep0_idx].control = (4U << 10) | (1U << 5) | ((len > 0 && (rt & 0x80)) ? 0 : (1U << 16)) | ep0_pcs; ep0_idx++;
    if (ep0_idx >= 60) {
        ep0_ring[ep0_idx].param = (uint64_t)(uintptr_t)ep0_ring; ep0_ring[ep0_idx].status = 0;
        ep0_ring[ep0_idx].control = (6U << 10) | 2U | ep0_pcs; ep0_idx = 0; ep0_pcs ^= 1;
    }
    xhci_db[active_slot] = 1;
    XhciTrb evt;
    return (xhci_wait_event(32, &evt) == 0 && ((evt.status >> 24) & 0xFF) == 1) ? 0 : -1;
}
static int xhci_bulk_xfer(int is_in, void *buf, uint32_t len) {
    XhciTrb *ring = is_in ? bulk_in_ring : bulk_out_ring;
    uint32_t *idx = is_in ? &bin_idx : &bout_idx;
    uint32_t *pcs = is_in ? &bin_pcs : &bout_pcs;
    uint8_t dci   = is_in ? bulk_in_dci : bulk_out_dci;

    ring[*idx].param   = (uint64_t)(uintptr_t)buf;
    ring[*idx].status  = len;
    ring[*idx].control = (1U << 10) | (1U << 5) | (*pcs);
    (*idx)++;
    if (*idx >= 63) {
        ring[63].param   = (uint64_t)(uintptr_t)ring;
        ring[63].status  = 0;
        ring[63].control = (6U << 10) | 2U | (*pcs);
        *idx = 0; (*pcs) ^= 1;
    }
    xhci_db[active_slot] = dci;
    XhciTrb evt;
    if (xhci_wait_event(32, &evt) != 0) return -1;
    uint8_t cc = (uint8_t)((evt.status >> 24) & 0xFF);
    return (cc == 1 || cc == 13) ? 0 : -1;
}

static int usb_msc_scsi_cmd(const uint8_t *cdb, uint8_t cdb_len, int is_in, void *data, uint32_t data_len) {
    UsbMscCbw *cbw = (UsbMscCbw *)usb_dma_buf;
    memset(cbw, 0, sizeof(UsbMscCbw));
    cbw->sig = 0x43425355U; cbw->tag = csw_tag++; cbw->data_len = data_len;
    cbw->flags = is_in ? 0x80 : 0x00; cbw->cb_len = cdb_len;
    memcpy(cbw->cb, cdb, cdb_len);

    if (xhci_bulk_xfer(0, cbw, 31) != 0) return -1;
    if (data_len > 0) {
        if (!is_in) memcpy(usb_dma_buf + 64, data, data_len);
        if (xhci_bulk_xfer(is_in, usb_dma_buf + 64, data_len) != 0) return -1;
        if (is_in) memcpy(data, usb_dma_buf + 64, data_len);
    }
    if (xhci_bulk_xfer(1, usb_dma_buf, 13) != 0) return -1;
    UsbMscCsw *csw = (UsbMscCsw *)usb_dma_buf;
    return (csw->sig == 0x53425355U && csw->status == 0) ? 0 : -1;
}

static int usb_msc_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    uint8_t *dst = (uint8_t *)buf;
    while (count > 0) {
        uint16_t chunk = (count > 7) ? 7 : (uint16_t)count;
        uint8_t cdb[10] = { 0x28, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, (uint8_t)(chunk >> 8), (uint8_t)chunk, 0 };
        if (usb_msc_scsi_cmd(cdb, 10, 1, dst, chunk * 512) != 0) return -1;
        dst += chunk * 512; lba += chunk; count -= chunk;
    }
    return 0;
}

static int usb_msc_write_sectors(uint64_t lba, uint32_t count, const void *buf) {
    const uint8_t *src = (const uint8_t *)buf;
    while (count > 0) {
        uint16_t chunk = (count > 7) ? 7 : (uint16_t)count;
        uint8_t cdb[10] = { 0x2A, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba, 0, (uint8_t)(chunk >> 8), (uint8_t)chunk, 0 };
        if (usb_msc_scsi_cmd(cdb, 10, 0, (void *)src, chunk * 512) != 0) return -1;
        src += chunk * 512; lba += chunk; count -= chunk;
    }
    return 0;
}

int usb_msc_init(void) {
    PciDevice dev;
    if (!pci_find_class(0x0C, 0x03, 0x30, &dev)) return 0;
    pci_enable_bus_master_mmio(&dev);
    if (!dev.bar0) return 0;

    xhci_cap = (volatile uint8_t *)(uintptr_t)dev.bar0;
    uint32_t hcs1 = mmio_r32(xhci_cap, 0x04);
    xhci_op = xhci_cap + *(volatile uint8_t *)xhci_cap;
    xhci_db = (volatile uint32_t *)(xhci_cap + (mmio_r32(xhci_cap, 0x14) & ~3U));
    xhci_rt = xhci_cap + (mmio_r32(xhci_cap, 0x18) & ~0x1FU);

    mmio_w32(xhci_op, 0, mmio_r32(xhci_op, 0) & ~1U);
    for (int t = 0; t < 1000000 && !(mmio_r32(xhci_op, 4) & 1); t++);
    mmio_w32(xhci_op, 0, 2U);
    for (int t = 0; t < 1000000 && ((mmio_r32(xhci_op, 0) & 2U) || (mmio_r32(xhci_op, 4) & (1U << 11))); t++);

    memset(dcbaa, 0, sizeof(dcbaa)); memset(cmd_ring, 0, sizeof(cmd_ring)); memset(evt_ring, 0, sizeof(evt_ring));
    cmd_idx = evt_idx = 0; cmd_pcs = evt_ccs = 1;
    mmio_w32(xhci_op, 0x38, hcs1 & 0xFF);
    mmio_w64(xhci_op, 0x30, (uint64_t)(uintptr_t)dcbaa);
    mmio_w64(xhci_op, 0x18, (uint64_t)(uintptr_t)cmd_ring | 1ULL);
    erst[0].base = (uint64_t)(uintptr_t)evt_ring; erst[0].size = 64;
    mmio_w32(xhci_rt, 0x28, 1); mmio_w64(xhci_rt, 0x38, (uint64_t)(uintptr_t)evt_ring); mmio_w64(xhci_rt, 0x30, (uint64_t)(uintptr_t)erst);
    mmio_w32(xhci_op, 0, 1U);
    for (int t = 0; t < 1000000 && (mmio_r32(xhci_op, 4) & 1); t++);

    uint8_t ports = (uint8_t)((hcs1 >> 24) & 0xFF), port = 0; uint32_t spd = 0;
    for (uint8_t p = 1; p <= ports; p++) {
        uint32_t off = 0x400 + (p - 1) * 0x10, sc = mmio_r32(xhci_op, off);
        if (sc & 1U) {
            if (!(sc & 2U)) { mmio_w32(xhci_op, off, (sc & 0x0E00C3E0U) | 16U); for (int t = 0; t < 2000000 && !(mmio_r32(xhci_op, off) & 2U); t++); }
            sc = mmio_r32(xhci_op, off);
            if (sc & 2U) { port = p; spd = (sc >> 10) & 0xF; break; }
        }
    }
    if (!port) return 0;

    XhciTrb evt;
    if (xhci_send_cmd(0, 0, (9U << 10), &evt) != 0) return 0;
    active_slot = (uint8_t)((evt.control >> 24) & 0xFF);
    if (!active_slot) return 0;

    memset(dev_ctx, 0, sizeof(dev_ctx)); memset(in_ctx, 0, sizeof(in_ctx)); memset(ep0_ring, 0, sizeof(ep0_ring));
    ep0_idx = 0; ep0_pcs = 1; dcbaa[active_slot] = (uint64_t)(uintptr_t)dev_ctx;
    uint16_t mps0 = (spd == 4) ? 512 : ((spd == 3) ? 64 : 8);
    in_ctx[1] = 3U; in_ctx[8] = (1U << 27) | (spd << 20); in_ctx[9] = ((uint32_t)port << 16);
    in_ctx[17] = (4U << 3) | ((uint32_t)mps0 << 16); in_ctx[18] = (uint32_t)((uintptr_t)ep0_ring | 1U); in_ctx[19] = (uint32_t)((uintptr_t)ep0_ring >> 32);
    if (xhci_send_cmd((uint64_t)(uintptr_t)in_ctx, 0, (11U << 10) | ((uint32_t)active_slot << 24), &evt) != 0) return 0;

    memset(usb_dma_buf, 0, 256);
    if (xhci_ctrl_xfer(0x80, 0x06, 0x0200, 0, usb_dma_buf, 64) != 0) return 0;
    int msc = 0; uint8_t ein = 0, eout = 0; uint16_t emps = 512;
    uint8_t *p = usb_dma_buf, *end = usb_dma_buf + 64;
    while (p + 2 <= end && p[0] >= 2) {
        if (p[1] == 4 && p[0] >= 9 && p[5] == 0x08 && p[6] == 0x06 && p[7] == 0x50) msc = 1;
        else if (msc && p[1] == 5 && p[0] >= 7) {
            emps = (uint16_t)p[4] | ((uint16_t)p[5] << 8);
            if (p[2] & 0x80) ein = p[2] & 0x0F; else eout = p[2] & 0x0F;
        }
        p += p[0];
    }
    if (!msc || !ein || !eout) return 0;

    bulk_in_dci = ein * 2 + 1; bulk_out_dci = eout * 2;
    memset(in_ctx, 0, sizeof(in_ctx)); memset(bulk_in_ring, 0, sizeof(bulk_in_ring)); memset(bulk_out_ring, 0, sizeof(bulk_out_ring));
    bin_idx = bout_idx = 0; bin_pcs = bout_pcs = 1;
    uint32_t mdci = (bulk_in_dci > bulk_out_dci) ? bulk_in_dci : bulk_out_dci;
    in_ctx[1] = 1U | (1U << bulk_in_dci) | (1U << bulk_out_dci); in_ctx[8] = (mdci << 27) | (spd << 20); in_ctx[9] = ((uint32_t)port << 16);
    uint32_t oo = (bulk_out_dci + 1) * 8, io = (bulk_in_dci + 1) * 8;
    in_ctx[oo + 1] = (2U << 3) | ((uint32_t)emps << 16); in_ctx[oo + 2] = (uint32_t)((uintptr_t)bulk_out_ring | 1U); in_ctx[oo + 3] = (uint32_t)((uintptr_t)bulk_out_ring >> 32);
    in_ctx[io + 1] = (6U << 3) | ((uint32_t)emps << 16); in_ctx[io + 2] = (uint32_t)((uintptr_t)bulk_in_ring | 1U); in_ctx[io + 3] = (uint32_t)((uintptr_t)bulk_in_ring >> 32);

    if (xhci_send_cmd((uint64_t)(uintptr_t)in_ctx, 0, (12U << 10) | ((uint32_t)active_slot << 24), &evt) != 0) return 0;
    if (xhci_ctrl_xfer(0x00, 0x09, usb_dma_buf[5], 0, 0, 0) != 0) return 0;

    uint8_t cdb[10] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, resp[8];
    if (usb_msc_scsi_cmd(cdb, 10, 1, resp, 8) != 0) return 0;
    usb_sectors = (uint64_t)(((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16) | ((uint32_t)resp[2] << 8) | resp[3]) + 1;

    BlockDriver drv = { "USB-MSC(xHCI)", usb_sectors, 512, usb_msc_read_sectors, usb_msc_write_sectors };
    block_register(&drv);
    return 1;
}


