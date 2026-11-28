#pragma once

#include <stdint.h>

typedef struct {
    uint8_t revision;
    uint64_t rsdp_address;
    uint64_t root_table_address;
    uint32_t table_count;
    uint8_t xsdt;
    uint8_t valid;
} ACPIInfo;

int acpi_init(uint64_t rsdp_address);
const ACPIInfo *acpi_get_info(void);
uint64_t acpi_find_table(const char signature[4]);
