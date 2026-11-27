#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HEAP_SIZE (16 * 1024 * 1024)

typedef struct BlockHdr {
    size_t size;
    int    free;
    struct BlockHdr *next;
} BlockHdr;

static uint8_t heap_buf[HEAP_SIZE] __attribute__((aligned(16)));
static BlockHdr *heap_head = 0;

static void heap_init_once(void) {
    if (heap_head) return;
    heap_head = (BlockHdr *)heap_buf;
    heap_head->size = HEAP_SIZE - sizeof(BlockHdr);
    heap_head->free = 1;
    heap_head->next = 0;
}

void *malloc(size_t size) {
    if (size == 0) size = 16;
    size = (size + 15ULL) & ~15ULL;
    heap_init_once();

    BlockHdr *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            if (cur->size >= size + sizeof(BlockHdr) + 32) {
                BlockHdr *split = (BlockHdr *)((uint8_t *)(cur + 1) + size);
                split->size = cur->size - size - sizeof(BlockHdr);
                split->free = 1;
                split->next = cur->next;
                cur->size = size;
                cur->next = split;
            }
            cur->free = 0;
            return (void *)(cur + 1);
        }
        cur = cur->next;
    }
    return 0;
}

void free(void *ptr) {
    if (!ptr) return;
    BlockHdr *hdr = ((BlockHdr *)ptr) - 1;
    hdr->free = 1;

    BlockHdr *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(BlockHdr) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }

    BlockHdr *hdr = ((BlockHdr *)ptr) - 1;
    if (hdr->size >= size) return ptr;

    void *np = malloc(size);
    if (!np) return 0;
    memcpy(np, ptr, hdr->size);
    free(ptr);
    return np;
}

void kheap_stats(size_t *total_b, size_t *used_b, size_t *free_b, size_t *used_cnt, size_t *free_cnt) {
    heap_init_once();
    size_t ub = 0, fb = 0, uc = 0, fc = 0;
    BlockHdr *cur = heap_head;
    while (cur) {
        if (cur->free) { fb += cur->size; fc++; }
        else { ub += cur->size + sizeof(BlockHdr); uc++; }
        cur = cur->next;
    }
    if (total_b) *total_b = HEAP_SIZE;
    if (used_b)  *used_b  = ub;
    if (free_b)  *free_b  = fb;
    if (used_cnt) *used_cnt = uc;
    if (free_cnt) *free_cnt = fc;
}
