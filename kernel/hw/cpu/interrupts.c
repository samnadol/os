#include "interrupts.h"

#include <stdbool.h>
#include "../../drivers/tty.h"

static bool interrupts_ever_enabled = false;

void interrupts_enable()
{
    dprintf(0, "[INT] Interrupts enabled\n");
    asm volatile("sti");
    interrupts_ever_enabled = true;
}

void interrupts_disable()
{
    asm volatile("cli");
}

void interrupts_reenable()
{
    if (interrupts_ever_enabled)
        asm volatile("sti");
}