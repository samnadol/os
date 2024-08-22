#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include "../../drivers/tty.h"

typedef struct
{
    // data segment selector
    uint32_t ds;
    // general purpose registers pushed by pusha
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    // pushed by isr procedure
    uint32_t int_no, err_code;
    // pushed by CPU automatically
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

inline void io_wait(void)
{
    asm volatile("outb %%al, $0x80"
                 :
                 : "a"(0));
}

void panic(const char *error);

#endif