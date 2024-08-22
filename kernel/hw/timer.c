#include "timer.h"

#include "port.h"
#include "cpu/isr.h"
#include "cpu/irq.h"
#include "../drivers/tty.h"

static uint64_t tick = 0;
static uint64_t poweron_epoch = 0;

bool timer_enabled()
{
    return tick > 0;
}

void timer_irq(registers_t *r)
{
    tick++;

    if (tick % TIMER_HZ == 0)
        poweron_epoch += 1;
}

uint64_t timer_get_epoch()
{
    return poweron_epoch;
}

uint64_t timer_get_tick()
{
    return tick;
}

void timer_wait(int ms)
{
    uint64_t endtick = (tick + ((ms * TIMER_HZ) / 1000));
    while (tick < endtick)
    {
        asm volatile("hlt");
    }
}

void timer_phase(int hz)
{
    int divisor = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void timer_init()
{
    dprintf("[TMR] Initializing\n");
    timer_phase(TIMER_HZ);
    irq_register(IRQ0, timer_irq);
}