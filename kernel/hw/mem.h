#ifndef LIB_MEM_H
#define LIB_MEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../drivers/tty.h"

typedef struct mem_segment
{
	uint64_t *start;
	uint64_t len;
} mem_segment_t;

typedef struct mem_node
{
    uint64_t size;
    bool used;
    struct mem_node *next;
    struct mem_node *prev;
} mem_node_t;

typedef struct mem_stats
{
    uint32_t used;
    uint32_t free;
    uint32_t total;
} mem_stats;

// memory management
void init_mem(mem_segment_t *biggest_mem_segment);

void *malloc(size_t req_size);
void *calloc(size_t req_size);
void *calloc_align(size_t req_size, size_t alignment);

void mfree(void *ptr);

// debug
void mem_print(tty_interface *tty);
mem_stats mem_get_stats();

#endif