#include "system.h"

#include "../mem.h"

void panic(const char *error)
{
    cprintf(TTYColor_RED, "%s\n===========================\nMemory Stats:\n%db free\n%db used\n%db total\n===========================\nHALTING\n", error, mem_get_stats().free, mem_get_stats().used, mem_get_stats().total);

    while (1)
        asm volatile("hlt");
}