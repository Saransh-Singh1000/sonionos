#ifndef SONION_FAT32_H
#define SONION_FAT32_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int      in_use;
    int      writable;
    uint32_t first_cluster;
    uint32_t cur_cluster;
    uint32_t cur_cluster_idx;
    uint32_t size;
    uint32_t pos;
    uint32_t dir_sector;
    uint32_t dir_offset;
} Fat32File;

typedef struct {
    char     name[256];
    uint32_t size;
    int      is_dir;
} Fat32DirEntry;

typedef struct {
    int      in_use;
    uint32_t start_cluster;
    uint32_t cur_cluster;
    uint32_t sector_idx;
    uint32_t entry_off;
} Fat32Dir;

int        fat32_init(void);
int         fat32_is_mounted(void);
const char *fat32_get_type_name(void);
Fat32File *fat32_open(const char *path, const char *mode);
size_t     fat32_read(void *ptr, size_t size, size_t nmemb, Fat32File *f);
size_t     fat32_write(const void *ptr, size_t size, size_t nmemb, Fat32File *f);
int        fat32_seek(Fat32File *f, int64_t offset, int whence);
int64_t    fat32_tell(Fat32File *f);
void       fat32_close(Fat32File *f);
int        fat32_remove(const char *path);
int        fat32_mkdir(const char *path);
Fat32Dir  *fat32_opendir(const char *path);
int        fat32_readdir(Fat32Dir *dir, Fat32DirEntry *out);
void       fat32_closedir(Fat32Dir *dir);

#endif


