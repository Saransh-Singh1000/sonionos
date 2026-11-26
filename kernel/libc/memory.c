#include "stdlib.h"
#include "string.h"
#include "errno.h"

typedef struct BlockHeader {
    size_t size;
    int    is_free;
    struct BlockHeader *next;
} BlockHeader;

#define HEADER_SIZE ((sizeof(BlockHeader) + 15) & ~(size_t)15)

static BlockHeader *heap_head = NULL;
static size_t total_heap_size = 0;

static inline size_t align16(size_t n) {
    return (n + 15) & ~(size_t)15;
}

void mem_init(uintptr_t heap_start, size_t heap_size) {
    uintptr_t aligned_start = (heap_start + 15) & ~(uintptr_t)15;
    size_t lost = (size_t)(aligned_start - heap_start);
    if (heap_size <= lost + HEADER_SIZE) return;

    heap_head = (BlockHeader *)aligned_start;
    heap_head->size = heap_size - lost - HEADER_SIZE;
    heap_head->is_free = 1;
    heap_head->next = NULL;
    total_heap_size = heap_head->size;
}

void *malloc(size_t size) {
    if (size == 0 || !heap_head) return NULL;
    size = align16(size);

    BlockHeader *curr = heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size >= size + HEADER_SIZE + 32) {
                BlockHeader *split = (BlockHeader *)((uint8_t *)curr + HEADER_SIZE + size);
                split->size = curr->size - size - HEADER_SIZE;
                split->is_free = 1;
                split->next = curr->next;

                curr->size = size;
                curr->next = split;
            }
            curr->is_free = 0;
            return (void *)((uint8_t *)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }
    errno = ENOMEM;
    return NULL;
}

void free(void *ptr) {
    if (!ptr) return;
    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);
    block->is_free = 1;

    BlockHeader *curr = heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += HEADER_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void *calloc(size_t num, size_t size) {
    if (num != 0 && size > (size_t)-1 / num) {
        errno = ENOMEM;
        return NULL;
    }
    size_t total = num * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) { free(ptr); return NULL; }

    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);
    if (block->size >= new_size) return ptr;

    void *new_ptr = malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}

size_t mem_get_used(void) {
    size_t used = 0;
    BlockHeader *curr = heap_head;
    while (curr) {
        if (!curr->is_free) used += curr->size;
        curr = curr->next;
    }
    return used;
}

size_t mem_get_free(void) {
    return total_heap_size - mem_get_used();
}
