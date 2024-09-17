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
    uint32_t magic_number;
    
    uint64_t size;
    bool used;

    char *caller_file;
    int caller_line;
    bool printed;
    uint32_t alloc_epoch;

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

#define malloc(req_size) kmalloc(__FILE__, __LINE__, req_size)
#define calloc(req_size) kcalloc(__FILE__, __LINE__, req_size)
#define calloc_align(req_size, align) kcalloc_align(__FILE__, __LINE__, req_size, align)

void *kmalloc(char *file, int line, size_t req_size);
void *kcalloc(char *file, int line, size_t req_size);
void *kcalloc_align(char *file, int line, size_t size, size_t align);

void mfree(void *ptr);

// debug
void mem_print(tty_interface *tty);
void mem_print_blocks(tty_interface *tty);
mem_stats mem_get_stats();

#endif