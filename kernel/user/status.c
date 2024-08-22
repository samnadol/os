#include "status.h"

#include "../drivers/vga/vga.h"
#include "../drivers/vga/modes/text.h"
#include "../lib/string.h"
#include "../hw/cpu/cpuid.h"
#include "../drivers/tty.h"
#include "../hw/mem.h"

char *buffer;

void status_write(uint8_t x, uint8_t y, char *ptr)
{
    int i;
    for (i = x; i < (x + strlen(ptr)); i++)
        vga_setc(ptr[i - x], i, y, VGA_TEXT_WHITE_ON_GREEN);
    for (; i < 80; i++)
        vga_setc(' ', i, y, VGA_TEXT_WHITE_ON_BLACK);
}

void status_update()
{
    memset(buffer, 0, 79);
    mem_stats mem = mem_get_stats();
    sprintf(buffer, "os %f / %f (%d%%)", mem.used, mem.total, (mem.used * 100) / mem.total);
    // sprintf(buffer, "os %f", mem.used);
    status_write(0, 0, buffer);
}

void status_init()
{
    buffer = (char *)calloc(79 * sizeof(char));
}