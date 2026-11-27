#ifndef SONION_SYSINFO_PAGE_H
#define SONION_SYSINFO_PAGE_H

#include <stdint.h>

#define SYSINFO_PAGE_ADDR 0x03FFF000ULL
#define SYSINFO_MAGIC     0x534F4E494F4E5349ULL

typedef struct {
    uint64_t magic;
    uint64_t boot_tsc;
    uint64_t tsc_per_ms;
    uint64_t unix_base;
    uint32_t screen_w;
    uint32_t screen_h;
    int32_t  mouse_x;
    int32_t  mouse_y;
    uint32_t mouse_btns;
    uint32_t last_key;
} SonionSysInfo;

#endif

