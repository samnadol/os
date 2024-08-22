#include "port.h"

uint8_t inb(uint16_t p_port)
{
    uint8_t l_ret;
    asm volatile("inb %1, %0" : "=a"(l_ret) : "dN"(p_port));
    return l_ret;
}

uint16_t inw(uint16_t p_port)
{
    uint16_t l_ret;
    asm volatile("inw %1, %0" : "=a"(l_ret) : "dN"(p_port));
    return l_ret;
}

inline uint32_t inl(uint16_t p_port)
{
    uint32_t l_ret;
    asm volatile("inl %1, %0" : "=a"(l_ret) : "dN"(p_port));
    return l_ret;
}

void outb(uint16_t p_port, uint8_t p_data)
{
    asm volatile("outb %1, %0" : : "dN"(p_port), "a"(p_data));
}

void outw(uint16_t p_port, uint16_t p_data)
{
    asm volatile("outw %1, %0" : : "dN"(p_port), "a"(p_data));
}

void outl(uint16_t p_port, uint32_t p_data)
{
    asm volatile("outl %1, %0" : : "dN"(p_port), "a"(p_data));
}