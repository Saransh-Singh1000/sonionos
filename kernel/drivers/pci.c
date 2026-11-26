#include "drivers/pci.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1U << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)slot << 11)
                  | ((uint32_t)func << 8)
                  | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = (1U << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)slot << 11)
                  | ((uint32_t)func << 8)
                  | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, val);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read32(bus, slot, func, offset & 0xFC);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t aligned = offset & 0xFC;
    uint32_t shift = (offset & 2) * 8;
    uint32_t cur = pci_read32(bus, slot, func, aligned);
    cur = (cur & ~(0xFFFFU << shift)) | ((uint32_t)val << shift);
    pci_write32(bus, slot, func, aligned, cur);
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read32(bus, slot, func, offset & 0xFC);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

static uint64_t read_bar_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_offset) {
    uint32_t lo = pci_read32(bus, slot, func, bar_offset);
    if (lo & 1) {
        return (uint64_t)(lo & ~0x3U);
    }
    uint8_t type = (lo >> 1) & 0x3;
    uint64_t addr = (uint64_t)(lo & ~0xFU);
    if (type == 0x2) {
        uint64_t hi = pci_read32(bus, slot, func, bar_offset + 4);
        addr |= (hi << 32);
    }
    return addr;
}

int pci_find_class(uint8_t class_code, uint8_t subclass, int prog_if_filter, PciDevice *out_dev) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read16((uint8_t)bus, slot, 0, 0x00);
            if (vendor == 0xFFFF) continue;

            uint8_t header = pci_read8((uint8_t)bus, slot, 0, 0x0E);
            uint8_t max_func = (header & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < max_func; func++) {
                uint16_t v = pci_read16((uint8_t)bus, slot, func, 0x00);
                if (v == 0xFFFF) continue;

                uint8_t cc  = pci_read8((uint8_t)bus, slot, func, 0x0B);
                uint8_t sub = pci_read8((uint8_t)bus, slot, func, 0x0A);
                uint8_t pif = pci_read8((uint8_t)bus, slot, func, 0x09);

                if (cc == class_code && sub == subclass) {
                    if (prog_if_filter >= 0 && pif != (uint8_t)prog_if_filter) continue;
                    if (out_dev) {
                        out_dev->bus        = (uint8_t)bus;
                        out_dev->slot       = slot;
                        out_dev->func       = func;
                        out_dev->vendor_id  = v;
                        out_dev->device_id  = pci_read16((uint8_t)bus, slot, func, 0x02);
                        out_dev->class_code = cc;
                        out_dev->subclass   = sub;
                        out_dev->prog_if    = pif;
                        out_dev->bar0       = read_bar_addr((uint8_t)bus, slot, func, 0x10);
                        out_dev->bar4       = read_bar_addr((uint8_t)bus, slot, func, 0x20);
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}

void pci_enable_bus_master_mmio(const PciDevice *dev) {
    uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= (1 << 1) | (1 << 2); // Memory Space Enable + Bus Master DMA Enable
    cmd &= ~(uint16_t)(1 << 10); // Clear Interrupt Disable if needed
    pci_write16(dev->bus, dev->slot, dev->func, 0x04, cmd);
}
