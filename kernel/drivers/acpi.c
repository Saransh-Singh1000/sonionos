#include <stdint.h>
#include <stddef.h>
#include "drivers/acpi.h"

#define RSDP_SIGNATURE "RSD PTR "
#define MAX_ACPI_TABLES 256

typedef struct __attribute__((packed)) {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} RSDP1;

typedef struct __attribute__((packed)) {
    RSDP1 first;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} RSDP2;

typedef struct __attribute__((packed)) {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} ACPISDTHeader;

static ACPIInfo info;

static uint8_t checksum(const void *data, uint32_t length) {
    const uint8_t *p = (const uint8_t *)data;
    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; i++)
        sum = (uint8_t)(sum + p[i]);

    return sum;
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p) {
    return (uint64_t)read_u32(p) |
           ((uint64_t)read_u32(p + 4) << 32);
}

static int signature_is(const char actual[4], const char expected[4]) {
    for (unsigned i = 0; i < 4; i++)
        if (actual[i] != expected[i])
            return 0;
    return 1;
}

static int valid_header(const ACPISDTHeader *h) {
    if (!h || h->length < sizeof(ACPISDTHeader))
        return 0;

    if (h->length > 1024 * 1024)
        return 0;

    return checksum(h, h->length) == 0;
}

int acpi_init(uint64_t address) {
    info = (ACPIInfo){0};

    if (!address)
        return -1;

    const RSDP1 *r1 = (const RSDP1 *)(uintptr_t)address;

    for (unsigned i = 0; i < 8; i++)
        if (r1->signature[i] != RSDP_SIGNATURE[i])
            return -1;

    if (checksum(r1, 20) != 0)
        return -1;

    info.revision = r1->revision;

    uint64_t root = r1->rsdt_address;
    uint8_t use_xsdt = 0;

    if (r1->revision >= 2) {
        const RSDP2 *r2 = (const RSDP2 *)r1;

        if (r2->length < sizeof(RSDP2) ||
            r2->length > 4096 ||
            checksum(r2, r2->length) != 0)
            return -1;

        if (r2->xsdt_address) {
            root = r2->xsdt_address;
            use_xsdt = 1;
        }
    }

    if (!root)
        return -1;

    const ACPISDTHeader *table =
        (const ACPISDTHeader *)(uintptr_t)root;

    if (!valid_header(table))
        return -1;

    if (use_xsdt) {
        if (!signature_is(table->signature, "XSDT"))
            return -1;

        if ((table->length - sizeof(*table)) % 8)
            return -1;

        info.table_count =
            (table->length - sizeof(*table)) / 8;
    } else {
        if (!signature_is(table->signature, "RSDT"))
            return -1;

        if ((table->length - sizeof(*table)) % 4)
            return -1;

        info.table_count =
            (table->length - sizeof(*table)) / 4;
    }

    if (info.table_count > MAX_ACPI_TABLES)
        return -1;

    info.root_table_address = root;
    info.xsdt = use_xsdt;
    info.valid = 1;

    return 0;
}

const ACPIInfo *acpi_get_info(void) {
    return info.valid ? &info : 0;
}

uint64_t acpi_find_table(const char signature[4]) {
    if (!info.valid || !signature)
        return 0;

    const ACPISDTHeader *root =
        (const ACPISDTHeader *)(uintptr_t)info.root_table_address;

    const uint8_t *entries =
        (const uint8_t *)root + sizeof(*root);

    for (uint32_t i = 0; i < info.table_count; i++) {
        uint64_t address;

        if (info.xsdt)
            address = read_u64(entries + (size_t)i * 8);
        else
            address = read_u32(entries + (size_t)i * 4);

        if (!address)
            continue;

        const ACPISDTHeader *table =
            (const ACPISDTHeader *)(uintptr_t)address;

        if (!valid_header(table))
            continue;

        if (table->signature[0] == signature[0] &&
            table->signature[1] == signature[1] &&
            table->signature[2] == signature[2] &&
            table->signature[3] == signature[3])
            return address;
    }

    return 0;
}
