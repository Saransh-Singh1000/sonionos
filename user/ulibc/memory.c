#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define UHEAP_BASE 0x01800000ULL
#define UHEAP_SIZE (16ULL * 1016ULL * 1016ULL)

typedef struct BlockHdr {
    size_t size;
    int    free;
    struct BlockHdr *next;
} BlockHdr;

static BlockHdr *uheap_head = 0;

static void uheap_init(void) {
    if (uheap_head) return;
    uheap_head = (BlockHdr *)(uintptr_t)UHEAP_BASE;
    uheap_head->size = (size_t)UHEAP_SIZE - sizeof(BlockHdr);
    uheap_head->free = 1;
    uheap_head->next = 0;
}

void *malloc(size_t size) {
    if (size == 0) size = 16;
    size = (size + 15ULL) & ~15ULL;
    uheap_init();
    BlockHdr *cur = uheap_head;
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
    BlockHdr *cur = uheap_head;
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

