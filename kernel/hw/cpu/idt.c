#include "idt.h"

#define low_16(address) (uint16_t)((address) & 0xFFFF)
#define high_16(address) (uint16_t)(((address) >> 16) & 0xFFFF)

idt_gate_t idt[256];
idt_register_t idt_reg;

void idt_gate_set(int n, uint32_t handler, uint16_t sel, uint8_t flags)
{
    idt[n].low_offset = low_16(handler);
    idt[n].high_offset = high_16(handler);

    idt[n].flags = flags | 0x60;
    idt[n].selector = sel; // see GDT

    idt[n].always0 = 0;
    // 0x8E = 1  00 0 1  110
    //        P DPL 0 D Type
}

void idt_init()
{
    idt_reg.base = (uint32_t)&idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;

    /* Don't make the mistake of loading &idt -- always load &idt_reg */
    asm volatile("lidt (%0)" : : "r"(&idt_reg));
}
