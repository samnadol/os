#include "./irq.h"
#include "./idt.h"
#include "pic.h"
#include "../../lib/string.h"
#include "../port.h"
#include "../mem.h"

typedef struct irq_routine
{
    void (*handler)(registers_t *);
    uint8_t int_no;
    struct irq_routine *next;
} irq_routine;
irq_routine *base = 0;

void irq_register(int n, void (*handler)(registers_t *))
{
    dprintf(0, "[INT] Registering IRQ 0x%x to handler %p\n", n, handler);

    irq_routine *routine = (irq_routine *)calloc(sizeof(irq_routine));
    routine->handler = handler;
    routine->int_no = n;
    routine->next = 0;

    if (!base)
        base = routine;
    else
    {
        irq_routine *current = base;
        while (current->next)
            current = current->next;
        current->next = routine;
    }
}

bool irq_deregister(void (*handler)(registers_t *))
{
    irq_routine *current = base;
    irq_routine *prev = 0;
    while (current->next)
    {
        if (current->handler == handler)
        {
            if (prev)
                prev->next = current->next;
            mfree(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

void irq_init()
{
    idt_gate_set(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_gate_set(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_gate_set(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_gate_set(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_gate_set(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_gate_set(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_gate_set(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_gate_set(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_gate_set(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_gate_set(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_gate_set(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_gate_set(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_gate_set(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_gate_set(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_gate_set(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_gate_set(47, (uint32_t)irq15, 0x08, 0x8E);
}

void irq_handler(registers_t *r)
{
    // if (r->int_no != 0x20)
    //     printf("[INT] 0x%x\n", r->int_no);

    irq_routine *current = base;
    if (!current)
        return;

    while (current)
    {
        if (r->int_no == current->int_no)
        {
            current->handler(r);
        }
        current = current->next;
    }

    pic_int_end(r->int_no - 32);
}

void irq_print(tty_interface *tty)
{
    irq_routine *current = base;
    if (!current)
        return;

    while (current)
    {
        tprintf(tty, "0x%x %p\n", current->int_no, current->handler);
        current = current->next;
    }
}