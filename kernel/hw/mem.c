#include "mem.h"

#include "../lib/string.h"
#include "../user/status.h"

#define MEM_1NODE_SIZE sizeof(mem_node_t)

static mem_node_t *mem_start;
uint64_t mem_area = 0;

void mem_print(tty_interface *tty)
{
    uint64_t total = 0;
    uint64_t used = 0;

    mem_node_t *current = mem_start;
    while (current != NULL)
    {
        total += current->size + MEM_1NODE_SIZE;
        if (current->used)
            used += current->size;

        current = current->next;
    }

    tprintf(tty, "[MEM] Status: %f total, ", total);
    tprintf(tty, "%f used, ", used);
    tprintf(tty, "%f free\n", total - used);
}

void init_mem(mem_segment_t *biggest_mem_segment)
{
    // printf("[MEM] Dyamic memory manager initializing\n");
    // printf("[MEM] Used memory segment 0x%p - 0x%p -> %f\n", biggest_mem_segment->start, biggest_mem_segment->start + biggest_mem_segment->len, biggest_mem_segment->len);

    mem_start = (mem_node_t *)biggest_mem_segment->start;
    mem_start->size = biggest_mem_segment->len - MEM_1NODE_SIZE;
    mem_start->used = 0;
    mem_start->next = NULL;
    mem_start->prev = NULL;

    mem_area = biggest_mem_segment->len;
}

void *malloc(size_t req_size)
{
    // find best block to allocate
    mem_node_t *best = (mem_node_t *)NULL;
    uint64_t best_size = mem_area - MEM_1NODE_SIZE + 1;

    mem_node_t *current = mem_start;
    while (current)
    {
        if (
            (!current->used) &&
            (current->size >= (req_size + MEM_1NODE_SIZE)) &&
            (current->size <= best_size))
        {
            best = current;
            best_size = current->size;
        }

        current = current->next;
    }

    if (best == NULL)
        return NULL;

    // if found (not null), then alloc and return

    best->size = best->size - req_size - MEM_1NODE_SIZE;

    mem_node_t *mem_node_alloc = (mem_node_t *)(((uint8_t *)best) + MEM_1NODE_SIZE + best->size);
    mem_node_alloc->size = req_size;
    mem_node_alloc->used = true;
    mem_node_alloc->next = best->next;
    mem_node_alloc->prev = best;

    if (best->next != NULL)
        best->next->prev = mem_node_alloc;
    best->next = mem_node_alloc;

    dprintf("[MEM] malloc %p (%d b)\n", ((uint8_t *)mem_node_alloc + MEM_1NODE_SIZE), req_size);

    status_update();
    return (void *)((uint8_t *)mem_node_alloc + MEM_1NODE_SIZE);
}

void *calloc(size_t req_size)
{
    void *ptr = malloc(req_size);
    if (!ptr)
        return 0;

    memset(ptr, 0, req_size);
    return ptr;
}

void *calloc_align(size_t size, size_t align)
{
    void *addr = calloc(size + align);
    if (addr)
        addr = (void *)(uintptr_t)addr + (align - (uintptr_t)addr % align);
    return addr;
}

void mfree(void *ptr)
{
    dprintf("[MEM] free %p\n", ptr);

    if (ptr == NULL)
        return;

    mem_node_t *free = (mem_node_t *)((uint8_t *)ptr - MEM_1NODE_SIZE);
    ptr = NULL;

    if (free == NULL)
        return;

    free->used = false;

    // merge next node into the one being freed
    mem_node_t *next = free->next;
    if (next != NULL && !next->used)
    {
        free->size += free->next->size;
        free->size += MEM_1NODE_SIZE;

        // remove next block from list
        free->next = free->next->next;
        if (free->next != NULL)
            free->next->prev = free;
    }

    // merge this into prev
    mem_node_t *prev = free->prev;
    if (prev != NULL && !prev->used)
    {
        // add size of previous block to current block
        prev->size += free->size;
        prev->size += MEM_1NODE_SIZE;

        // remove current node from list
        prev->next = free->next;
        if (free->next != NULL)
        {
            free->next->prev = prev;
        }
    }
    status_update();
}

mem_stats mem_get_stats()
{
    mem_stats stats;

    uint64_t total = 0;
    uint64_t used = 0;

    mem_node_t *current = mem_start;
    while (current != NULL)
    {
        total += current->size + MEM_1NODE_SIZE;
        if (current->used)
            used += current->size;

        current = current->next;
    }

    stats.total = total;
    stats.used = used;
    stats.free = total - used;
    return stats;
}