#include "scheduler.h"

#include <stdarg.h>
#include <stdbool.h>

#include "../drivers/tty.h"
#include "../drivers/vga/vga.h"
#include "gui/gui.h"
#include "status.h"

static void (*scheduled_functions[10])(void *);
static void *scheduled_data[10];
static int current_func;

void scheduler_init()
{
    current_func = -1;
}

bool do_gui_update = false;
void scheduler_tick()
{
    if (current_func > -1)
    {
        scheduled_functions[current_func](scheduled_data[current_func]);
        current_func--;
    }

    switch (vga_get_mode())
    {
    case VGA_TEXT:
        status_update();
        break;
    case VGA_GUI:
        gui_update();
        break;
    }
}

void scheduler_add_task(void (*func)(), void *data)
{
    current_func++;
    scheduled_functions[current_func] = func;
    scheduled_data[current_func] = data;
}