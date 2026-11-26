#include "fs/fat32.h"
#include "blockdev.h"
#include "string.h"
#include "ctype.h"

typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} Fat32Bpb;

typedef struct __attribute__((packed)) {
    char     name[11];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} Fat32ShortEnt;

typedef struct __attribute__((packed)) {
    uint8_t  seq;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t fst_clus_lo;
    uint16_t name3[2];
} Fat32LfnEnt;

#define FAT32_MAX_FILES 32
#define FAT32_MAX_DIRS  8
static Fat32File file_pool[FAT32_MAX_FILES];
static Fat32Dir  dir_pool[FAT32_MAX_DIRS];

static int      mounted = 0;
static int      is_fat16 = 0;
static uint32_t spc = 0;
static uint8_t  num_fats = 2;
static uint32_t fat_start_lba = 0;
static uint32_t fat_sectors = 0;
static uint32_t root_dir_lba = 0;
static uint32_t root_dir_sectors = 0;
static uint32_t data_start_lba = 0;
static uint32_t root_cluster = 2;
static uint32_t total_clusters = 0;
static uint8_t  sec_buf[512];

static uint64_t cluster_to_lba(uint32_t clus) {
    return (uint64_t)data_start_lba + (uint64_t)(clus - 2) * spc;
}

static uint32_t fat_get_next(uint32_t clus) {
    uint32_t off = is_fat16 ? (clus * 2) : (clus * 4);
    if (block_read(fat_start_lba + (off / 512), 1, sec_buf) != 0) return 0x0FFFFFFF;
    if (is_fat16) {
        uint16_t v = *(uint16_t *)(sec_buf + (off % 512));
        return (v >= 0xFFF8U) ? 0x0FFFFFFF : (uint32_t)v;
    }
    return (*(uint32_t *)(sec_buf + (off % 512))) & 0x0FFFFFFF;
}

static int fat_set_next(uint32_t clus, uint32_t val) {
    uint32_t off = is_fat16 ? (clus * 2) : (clus * 4);
    uint32_t rel_sec = off / 512;
    uint32_t ent_off = off % 512;
    for (uint8_t f = 0; f < num_fats; f++) {
        uint64_t lba = (uint64_t)fat_start_lba + (uint64_t)f * fat_sectors + rel_sec;
        if (block_read(lba, 1, sec_buf) != 0) return -1;
        if (is_fat16) {
            uint16_t *p = (uint16_t *)(sec_buf + ent_off);
            *p = (val >= 0x0FFFFFF8U) ? 0xFFFFU : (uint16_t)val;
        } else {
            uint32_t *p = (uint32_t *)(sec_buf + ent_off);
            *p = (*p & 0xF0000000U) | (val & 0x0FFFFFFFU);
        }
        if (block_write(lba, 1, sec_buf) != 0) return -1;
    }
    return 0;
}

static void fat_free_chain(uint32_t start_clus) {
    uint32_t c = start_clus;
    while (c >= 2 && c < 0x0FFFFFF8U) {
        uint32_t nxt = fat_get_next(c);
        fat_set_next(c, 0);
        c = nxt;
    }
}

static uint32_t fat_alloc_cluster(void) {
    for (uint32_t c = 2; c < total_clusters + 2; c++) {
        if (fat_get_next(c) == 0) {
            fat_set_next(c, 0x0FFFFFFF);
            memset(sec_buf, 0, 512);
            uint64_t lba = cluster_to_lba(c);
            for (uint32_t s = 0; s < spc; s++) block_write(lba + s, 1, sec_buf);
            return c;
        }
    }
    return 0;
}

static int try_mount_vbr(uint64_t lba) {
    if (block_read(lba, 1, sec_buf) != 0) return 0;
    Fat32Bpb *bpb = (Fat32Bpb *)sec_buf;
    if (bpb->bytes_per_sector != 512 || bpb->sectors_per_cluster == 0) return 0;

    spc           = bpb->sectors_per_cluster;
    num_fats      = bpb->num_fats ? bpb->num_fats : 1;
    fat_start_lba = (uint32_t)lba + bpb->reserved_sectors;
    uint32_t tot  = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    if (tot == 0) return 0;

    if (bpb->fat_size_16 != 0) {
        is_fat16         = 1;
        fat_sectors      = bpb->fat_size_16;
        root_dir_lba     = fat_start_lba + (num_fats * fat_sectors);
        root_dir_sectors = ((uint32_t)bpb->root_entry_count * 32 + 511) / 512;
        data_start_lba   = root_dir_lba + root_dir_sectors;
        root_cluster     = 1;
        total_clusters   = (tot - (data_start_lba - (uint32_t)lba)) / spc;
    } else if (bpb->fat_size_32 != 0) {
        is_fat16         = 0;
        fat_sectors      = bpb->fat_size_32;
        root_dir_lba     = 0;
        root_dir_sectors = 0;
        data_start_lba   = fat_start_lba + (num_fats * fat_sectors);
        root_cluster     = bpb->root_cluster ? bpb->root_cluster : 2;
        total_clusters   = (tot - (data_start_lba - (uint32_t)lba)) / spc;
    } else {
        return 0;
    }
    mounted = 1;
    return 1;
}

int fat32_init(void) {
    mounted = 0;
    memset(file_pool, 0, sizeof(file_pool));
    memset(dir_pool, 0, sizeof(dir_pool));

    uint8_t hdr[512];
    if (block_read(1, 1, hdr) == 0 && memcmp(hdr, "EFI PART", 8) == 0) {
        uint64_t plba = *(uint64_t *)(hdr + 72);
        uint32_t npart = *(uint32_t *)(hdr + 80);
        uint32_t esz = *(uint32_t *)(hdr + 84);
        if (npart > 32) npart = 32;
        for (uint32_t i = 0; i < npart; i++) {
            if (block_read(plba + (i * esz) / 512, 1, sec_buf) == 0) {
                uint64_t flba = *(uint64_t *)(sec_buf + ((i * esz) % 512) + 32);
                if (flba > 0 && try_mount_vbr(flba)) return 1;
            }
        }
    }
    if (block_read(0, 1, hdr) == 0 && hdr[510] == 0x55 && hdr[511] == 0xAA) {
        for (int p = 0; p < 4; p++) {
            uint8_t type = hdr[446 + p * 16 + 4];
            uint32_t start = *(uint32_t *)(hdr + 446 + p * 16 + 8);
            if (type != 0 && start > 0 && try_mount_vbr(start)) return 1;
        }
    }
    return try_mount_vbr(0);
}

int fat32_is_mounted(void) { return mounted; }
const char *fat32_get_type_name(void) { return !mounted ? "NONE" : (is_fat16 ? "FAT16" : "FAT32"); }

static int name_eq_nocase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static void decode_short83(const char raw[11], char out[16]) {
    int p = 0, end8 = 7;
    while (end8 >= 0 && raw[end8] == ' ') end8--;
    for (int i = 0; i <= end8; i++) out[p++] = raw[i];
    int end3 = 10;
    while (end3 >= 8 && raw[end3] == ' ') end3--;
    if (end3 >= 8) {
        out[p++] = '.';
        for (int i = 8; i <= end3; i++) out[p++] = raw[i];
    }
    out[p] = 0;
}

static void extract_lfn_part(const Fat32LfnEnt *lfn, char buf[260]) {
    int idx = ((lfn->seq & 0x1F) - 1) * 13;
    if (idx < 0 || idx > 240) return;
    uint16_t chars[13];
    for (int i = 0; i < 5; i++) chars[i] = lfn->name1[i];
    for (int i = 0; i < 6; i++) chars[i + 5] = lfn->name2[i];
    for (int i = 0; i < 2; i++) chars[i + 11] = lfn->name3[i];
    for (int i = 0; i < 13; i++) {
        if (chars[i] == 0x0000 || chars[i] == 0xFFFF) { buf[idx + i] = 0; return; }
        buf[idx + i] = (char)(chars[i] & 0xFF);
    }
}

static uint8_t lfn_checksum(const char short83[11]) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)short83[i];
    return sum;
}

static void make_short83(const char *name, char out[11], int suffix) {
    memset(out, ' ', 11);
    const char *dot = strrchr(name, '.');
    int base_end = dot ? (int)(dot - name) : (int)strlen(name);
    int j = 0;
    for (int i = 0; i < base_end && j < 8; i++) {
        char c = name[i];
        if (c == ' ' || c == '.') continue;
        out[j++] = (char)toupper((unsigned char)c);
    }
    if (suffix > 0) {
        int pos = (j > 6) ? 6 : j;
        out[pos] = '~';
        out[pos + 1] = (char)('0' + (suffix % 10));
    }
    if (dot && dot[1]) {
        j = 8;
        for (int i = 1; dot[i] && j < 11; i++) {
            if (dot[i] != ' ') out[j++] = (char)toupper((unsigned char)dot[i]);
        }
    }
}
static int find_in_dir(uint32_t dir_clus, const char *target, Fat32ShortEnt *out_ent,
                       uint32_t *out_sec, uint32_t *out_off, uint32_t *lfn_start_idx) {
    char lfn_buf[260], short_buf[16];
    int has_lfn = 0;
    uint32_t clus = dir_clus, slot_idx = 0, first_lfn_slot = 0;

    while (((is_fat16 && clus == 1) || (clus >= 2 && clus < 0x0FFFFFF8U))) {
        uint64_t lba = (is_fat16 && clus == 1) ? root_dir_lba : cluster_to_lba(clus);
        uint32_t sec_cnt = (is_fat16 && clus == 1) ? root_dir_sectors : spc; for (uint32_t s = 0; s < sec_cnt; s++) {
            if (block_read(lba + s, 1, sec_buf) != 0) return 0;
            for (uint32_t off = 0; off < 512; off += 32, slot_idx++) {
                Fat32ShortEnt *e = (Fat32ShortEnt *)(sec_buf + off);
                uint8_t first = (uint8_t)e->name[0];
                if (first == 0x00) return 0;
                if (first == 0xE5) { has_lfn = 0; continue; }
                if (e->attr == 0x0F) {
                    Fat32LfnEnt *le = (Fat32LfnEnt *)e;
                    if (le->seq & 0x40) {
                        memset(lfn_buf, 0, sizeof(lfn_buf));
                        has_lfn = 1;
                        first_lfn_slot = slot_idx;
                    }
                    if (has_lfn) extract_lfn_part(le, lfn_buf);
                    continue;
                }
                if (e->attr & 0x08) { has_lfn = 0; continue; }
                decode_short83(e->name, short_buf);
                int match = (has_lfn && name_eq_nocase(lfn_buf, target)) ||
                            name_eq_nocase(short_buf, target);
                if (match) {
                    if (out_ent) *out_ent = *e;
                    if (out_sec) *out_sec = (uint32_t)(lba + s);
                    if (out_off) *out_off = off;
                    if (lfn_start_idx) *lfn_start_idx = has_lfn ? first_lfn_slot : slot_idx;
                    return 1;
                }
                has_lfn = 0;
            }
        }
        clus = (is_fat16 && clus == 1) ? 0x0FFFFFFF : fat_get_next(clus);
    }
    return 0;
}

static int write_dir_slot(uint32_t dir_clus, uint32_t slot_idx, const void *entry32) {
    if (is_fat16 && dir_clus == 1) {
        uint32_t sec = slot_idx / 16;
        if (sec >= root_dir_sectors) return 0;
        uint64_t lba = (uint64_t)root_dir_lba + sec;
        uint32_t off = (slot_idx % 16) * 32;
        if (block_read(lba, 1, sec_buf) != 0) return 0;
        memcpy(sec_buf + off, entry32, 32);
        return block_write(lba, 1, sec_buf) == 0;
    }
    uint32_t entries_per_clus = (spc * 512) / 32;
    uint32_t c_num = slot_idx / entries_per_clus;
    uint32_t rem   = slot_idx % entries_per_clus;
    uint32_t c = dir_clus;
    for (uint32_t i = 0; i < c_num; i++) {
        uint32_t nxt = fat_get_next(c);
        if (nxt >= 0x0FFFFFF8U || nxt < 2) {
            nxt = fat_alloc_cluster();
            if (!nxt) return 0;
            fat_set_next(c, nxt);
        }
        c = nxt;
    }
    uint64_t lba = cluster_to_lba(c) + (rem / 16);
    uint32_t off = (rem % 16) * 32;
    if (block_read(lba, 1, sec_buf) != 0) return 0;
    memcpy(sec_buf + off, entry32, 32);
    return block_write(lba, 1, sec_buf) == 0;
}

static int create_entry_in_dir(uint32_t dir_clus, const char *name, uint8_t attr,
                               uint32_t start_clus, Fat32ShortEnt *out_ent,
                               uint32_t *out_sec, uint32_t *out_off) {
    int nlen = (int)strlen(name);
    if (nlen <= 0 || nlen > 250) return 0;
    int num_lfn = (nlen + 12) / 13;
    int needed  = num_lfn + 1;

    char short83[11];
    int suffix = 0;
    do {
        make_short83(name, short83, suffix++);
        char test_name[16];
        decode_short83(short83, test_name);
        if (!find_in_dir(dir_clus, test_name, 0, 0, 0, 0)) break;
    } while (suffix < 10);

    uint32_t clus = dir_clus, slot = 0, run_start = 0;
    int run_len = 0, found_run = 0;
    while (((is_fat16 && clus == 1) || (clus >= 2 && clus < 0x0FFFFFF8U)) && !found_run) {
        uint64_t lba = (is_fat16 && clus == 1) ? root_dir_lba : cluster_to_lba(clus);
        uint32_t r_cnt = (is_fat16 && dir_clus == 1) ? root_dir_sectors : spc; for (uint32_t s = 0; s < r_cnt && !found_run; s++) {
            if (block_read(lba + s, 1, sec_buf) != 0) return 0;
            for (uint32_t off = 0; off < 512; off += 32, slot++) {
                uint8_t b = sec_buf[off];
                if (b == 0x00 || b == 0xE5) {
                    if (run_len == 0) run_start = slot;
                    if (++run_len >= needed) { found_run = 1; break; }
                } else {
                    run_len = 0;
                }
            }
        }
        if (!found_run) { if (is_fat16 && dir_clus == 1) return 0;
            uint32_t nxt = fat_get_next(clus);
            if (nxt >= 0x0FFFFFF8U || nxt < 2) {
                nxt = fat_alloc_cluster();
                if (!nxt) return 0;
                fat_set_next(clus, nxt);
            }
            clus = nxt;
        }
    }
    if (!found_run) return 0;

    uint8_t csum = lfn_checksum(short83);
    for (int idx = 0; idx < num_lfn; idx++) {
        int seq = num_lfn - idx;
        Fat32LfnEnt le;
        memset(&le, 0xFF, sizeof(le));
        le.seq         = (uint8_t)(seq | ((idx == 0) ? 0x40 : 0));
        le.attr        = 0x0F;
        le.type        = 0;
        le.checksum    = csum;
        le.fst_clus_lo = 0;

        int base = (seq - 1) * 13;
        uint16_t uchars[13];
        for (int k = 0; k < 13; k++) {
            int pos = base + k;
            if (pos < nlen) uchars[k] = (uint8_t)name[pos];
            else if (pos == nlen) uchars[k] = 0x0000;
            else uchars[k] = 0xFFFF;
        }
        for (int k = 0; k < 5; k++) le.name1[k] = uchars[k];
        for (int k = 0; k < 6; k++) le.name2[k] = uchars[k + 5];
        for (int k = 0; k < 2; k++) le.name3[k] = uchars[k + 11];
        if (!write_dir_slot(dir_clus, run_start + idx, &le)) return 0;
    }

    Fat32ShortEnt se;
    memset(&se, 0, sizeof(se));
    memcpy(se.name, short83, 11);
    se.attr        = attr;
    se.fst_clus_hi = (uint16_t)(start_clus >> 16);
    se.fst_clus_lo = (uint16_t)(start_clus & 0xFFFF);
    uint32_t short_slot = run_start + num_lfn;
    if (!write_dir_slot(dir_clus, short_slot, &se)) return 0;

    if (out_ent) *out_ent = se;
    if (out_sec || out_off) {
        if (is_fat16 && dir_clus == 1) {
            if (out_sec) *out_sec = root_dir_lba + (short_slot / 16);
            if (out_off) *out_off = (short_slot % 16) * 32;
        } else {
            uint32_t entries_per_clus = (spc * 512) / 32;
            uint32_t c = dir_clus;
            for (uint32_t i = 0; i < short_slot / entries_per_clus; i++) c = fat_get_next(c);
            uint32_t rem = short_slot % entries_per_clus;
            if (out_sec) *out_sec = (uint32_t)(cluster_to_lba(c) + (rem / 16));
            if (out_off) *out_off = (rem % 16) * 32;
        }
    }
    return 1;
}

static int resolve_parent(const char *path, uint32_t *out_parent, char *out_leaf) {
    while (*path == '/' || *path == '\\') path++;
    uint32_t cur = root_cluster;
    char part[256];
    Fat32ShortEnt ent;

    while (*path) {
        int len = 0;
        while (*path && *path != '/' && *path != '\\' && len < 255) part[len++] = *path++;
        part[len] = 0;
        while (*path == '/' || *path == '\\') path++;
        if (*path == 0) {
            *out_parent = cur;
            strcpy(out_leaf, part);
            return 1;
        }
        if (!find_in_dir(cur, part, &ent, 0, 0, 0) || !(ent.attr & 0x10)) return 0;
        uint32_t clus = ((uint32_t)ent.fst_clus_hi << 16) | ent.fst_clus_lo;
        cur = clus ? clus : root_cluster;
    }
    return 0;
}

Fat32File *fat32_open(const char *path, const char *mode) {
    if (!mounted || !path || !mode) return 0;
    int wr = (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'));
    uint32_t parent_clus = 0, dsec = 0, doff = 0;
    char leaf[256];
    if (!resolve_parent(path, &parent_clus, leaf)) return 0;

    Fat32ShortEnt ent;
    int exists = find_in_dir(parent_clus, leaf, &ent, &dsec, &doff, 0);
    if (!exists) {
        if (!wr || !create_entry_in_dir(parent_clus, leaf, 0x20, 0, &ent, &dsec, &doff)) return 0;
    } else if (ent.attr & 0x10) {
        return 0;
    }

    uint32_t clus = ((uint32_t)ent.fst_clus_hi << 16) | ent.fst_clus_lo;
    if (exists && mode[0] == 'w') {
        if (clus >= 2) fat_free_chain(clus);
        clus = 0;
        ent.file_size = 0;
    }

    for (int i = 0; i < FAT32_MAX_FILES; i++) {
        if (!file_pool[i].in_use) {
            Fat32File *f = &file_pool[i];
            memset(f, 0, sizeof(Fat32File));
            f->in_use        = 1;
            f->writable      = wr;
            f->first_cluster = clus;
            f->cur_cluster   = clus;
            f->size          = ent.file_size;
            f->pos           = (mode[0] == 'a') ? ent.file_size : 0;
            f->dir_sector    = dsec;
            f->dir_offset    = doff;
            return f;
        }
    }
    return 0;
}
static uint32_t seek_to_cluster(Fat32File *f, uint32_t target_idx, int alloc_missing) {
    uint32_t c = f->first_cluster;
    if (c < 2) {
        if (!alloc_missing) return 0;
        c = fat_alloc_cluster();
        if (!c) return 0;
        f->first_cluster = c;
    }
    for (uint32_t idx = 0; idx < target_idx; idx++) {
        uint32_t nxt = fat_get_next(c);
        if (nxt >= 0x0FFFFFF8U || nxt < 2) {
            if (!alloc_missing) return 0;
            nxt = fat_alloc_cluster();
            if (!nxt) return 0;
            fat_set_next(c, nxt);
        }
        c = nxt;
    }
    f->cur_cluster = c;
    f->cur_cluster_idx = target_idx;
    return c;
}

size_t fat32_read(void *ptr, size_t size, size_t nmemb, Fat32File *f) {
    if (!f || !f->in_use || !ptr || !size || !nmemb || f->pos >= f->size) return 0;
    uint64_t want = (uint64_t)size * nmemb;
    if (f->pos + want > f->size) want = f->size - f->pos;

    uint8_t *dst = (uint8_t *)ptr;
    uint32_t cbytes = spc * 512;
    size_t total = 0;
    while (want > 0) {
        uint32_t clus = seek_to_cluster(f, f->pos / cbytes, 0);
        if (clus < 2) break;
        uint32_t coff = f->pos % cbytes;
        uint32_t boff = coff % 512;
        uint32_t take = (512 - boff > want) ? (uint32_t)want : (512 - boff);
        if (block_read(cluster_to_lba(clus) + (coff / 512), 1, sec_buf) != 0) break;
        memcpy(dst, sec_buf + boff, take);
        dst += take; f->pos += take; total += take; want -= take;
    }
    return total / size;
}

size_t fat32_write(const void *ptr, size_t size, size_t nmemb, Fat32File *f) {
    if (!f || !f->in_use || !f->writable || !ptr || !size || !nmemb) return 0;
    uint64_t want = (uint64_t)size * nmemb;
    const uint8_t *src = (const uint8_t *)ptr;
    uint32_t cbytes = spc * 512;
    size_t total = 0;
    while (want > 0) {
        uint32_t clus = seek_to_cluster(f, f->pos / cbytes, 1);
        if (clus < 2) break;
        uint32_t coff = f->pos % cbytes;
        uint32_t boff = coff % 512;
        uint32_t take = (512 - boff > want) ? (uint32_t)want : (512 - boff);
        uint64_t lba = cluster_to_lba(clus) + (coff / 512);
        if (take < 512 && block_read(lba, 1, sec_buf) != 0) break;
        memcpy(sec_buf + boff, src, take);
        if (block_write(lba, 1, sec_buf) != 0) break;
        src += take; f->pos += take; total += take; want -= take;
        if (f->pos > f->size) f->size = f->pos;
    }
    return total / size;
}

int fat32_seek(Fat32File *f, int64_t offset, int whence) {
    if (!f || !f->in_use) return -1;
    int64_t tgt = (whence == 0) ? offset :
                  ((whence == 1) ? (int64_t)f->pos + offset : (int64_t)f->size + offset);
    if (tgt < 0) return -1;
    f->pos = (uint32_t)tgt;
    return 0;
}

int64_t fat32_tell(Fat32File *f) { return (f && f->in_use) ? (int64_t)f->pos : -1; }

void fat32_close(Fat32File *f) {
    if (!f || !f->in_use) return;
    if (f->writable && f->dir_sector > 0 && block_read(f->dir_sector, 1, sec_buf) == 0) {
        Fat32ShortEnt *e = (Fat32ShortEnt *)(sec_buf + f->dir_offset);
        e->fst_clus_hi = (uint16_t)(f->first_cluster >> 16);
        e->fst_clus_lo = (uint16_t)(f->first_cluster & 0xFFFF);
        e->file_size   = f->size;
        block_write(f->dir_sector, 1, sec_buf);
    }
    f->in_use = 0;
}

int fat32_remove(const char *path) {
    if (!mounted || !path) return -1;
    uint32_t pclus = 0, lfn_start = 0;
    char leaf[256];
    if (!resolve_parent(path, &pclus, leaf)) return -1;
    Fat32ShortEnt ent;
    if (!find_in_dir(pclus, leaf, &ent, 0, 0, &lfn_start)) return -1;
    uint32_t fclus = ((uint32_t)ent.fst_clus_hi << 16) | ent.fst_clus_lo;
    if (fclus >= 2) fat_free_chain(fclus);

    int nlen = (int)strlen(leaf);
    uint32_t total_slots = (uint32_t)((nlen + 12) / 13) + 1;
    uint8_t del[32];
    memset(del, 0, 32);
    del[0] = 0xE5;
    for (uint32_t s = 0; s < total_slots; s++) write_dir_slot(pclus, lfn_start + s, del);
    return 0;
}

int fat32_mkdir(const char *path) {
    if (!mounted || !path) return -1;
    uint32_t pclus = 0;
    char leaf[256];
    if (!resolve_parent(path, &pclus, leaf)) return -1;
    if (find_in_dir(pclus, leaf, 0, 0, 0, 0)) return -1;

    uint32_t nclus = fat_alloc_cluster();
    if (!nclus) return -1;
    if (!create_entry_in_dir(pclus, leaf, 0x10, nclus, 0, 0, 0)) {
        fat_free_chain(nclus);
        return -1;
    }

    memset(sec_buf, 0, 512);
    Fat32ShortEnt *dot = (Fat32ShortEnt *)sec_buf;
    memcpy(dot->name, ".          ", 11);
    dot->attr = 0x10;
    dot->fst_clus_hi = (uint16_t)(nclus >> 16);
    dot->fst_clus_lo = (uint16_t)(nclus & 0xFFFF);

    Fat32ShortEnt *dotdot = (Fat32ShortEnt *)(sec_buf + 32);
    memcpy(dotdot->name, "..         ", 11);
    dotdot->attr = 0x10;
    uint32_t up = (pclus == root_cluster) ? 0 : pclus;
    dotdot->fst_clus_hi = (uint16_t)(up >> 16);
    dotdot->fst_clus_lo = (uint16_t)(up & 0xFFFF);

    return block_write(cluster_to_lba(nclus), 1, sec_buf);
}

Fat32Dir *fat32_opendir(const char *path) {
    if (!mounted) return 0;
    uint32_t clus = root_cluster;
    if (path) {
        while (*path == '/' || *path == '\\') path++;
        char part[256];
        Fat32ShortEnt ent;
        while (*path) {
            int len = 0;
            while (*path && *path != '/' && *path != '\\' && len < 255) part[len++] = *path++;
            part[len] = 0;
            while (*path == '/' || *path == '\\') path++;
            if (!find_in_dir(clus, part, &ent, 0, 0, 0) || !(ent.attr & 0x10)) return 0;
            clus = ((uint32_t)ent.fst_clus_hi << 16) | ent.fst_clus_lo;
            if (!clus) clus = root_cluster;
        }
    }
    for (int i = 0; i < FAT32_MAX_DIRS; i++) {
        if (!dir_pool[i].in_use) {
            memset(&dir_pool[i], 0, sizeof(Fat32Dir));
            dir_pool[i].in_use = 1;
            dir_pool[i].start_cluster = clus;
            dir_pool[i].cur_cluster = clus;
            return &dir_pool[i];
        }
    }
    return 0;
}

int fat32_readdir(Fat32Dir *dir, Fat32DirEntry *out) {
    if (!dir || !dir->in_use || !out) return 0;
    char lfn_buf[260];
    int has_lfn = 0;

    while ((is_fat16 && dir->cur_cluster == 1) || (dir->cur_cluster >= 2 && dir->cur_cluster < 0x0FFFFFF8U)) {
        uint64_t lba = (is_fat16 && dir->cur_cluster == 1) ? root_dir_lba : cluster_to_lba(dir->cur_cluster);
        uint32_t d_secs = (is_fat16 && dir->cur_cluster == 1) ? root_dir_sectors : spc; while (dir->sector_idx < d_secs) {
            if (block_read(lba + dir->sector_idx, 1, sec_buf) != 0) return 0;
            while (dir->entry_off < 512) {
                Fat32ShortEnt *e = (Fat32ShortEnt *)(sec_buf + dir->entry_off);
                dir->entry_off += 32;
                uint8_t first = (uint8_t)e->name[0];
                if (first == 0x00) { dir->cur_cluster = 0x0FFFFFFF; return 0; }
                if (first == 0xE5) { has_lfn = 0; continue; }
                if (e->attr == 0x0F) {
                    Fat32LfnEnt *le = (Fat32LfnEnt *)e;
                    if (le->seq & 0x40) { memset(lfn_buf, 0, sizeof(lfn_buf)); has_lfn = 1; }
                    if (has_lfn) extract_lfn_part(le, lfn_buf);
                    continue;
                }
                if (e->attr & 0x08) { has_lfn = 0; continue; }
                if (has_lfn && lfn_buf[0]) strcpy(out->name, lfn_buf);
                else decode_short83(e->name, out->name);
                out->size   = e->file_size;
                out->is_dir = (e->attr & 0x10) ? 1 : 0;
                return 1;
            }
            dir->entry_off = 0;
            dir->sector_idx++;
        }
        dir->sector_idx = 0;
        dir->cur_cluster = (is_fat16 && dir->cur_cluster == 1) ? 0x0FFFFFFF : fat_get_next(dir->cur_cluster);
    }
    return 0;
}

void fat32_closedir(Fat32Dir *dir) {
    if (dir) dir->in_use = 0;
}







