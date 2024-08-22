#ifndef HW_PORT_H
#define HW_PORT_H

#include <stdint.h>
#include <stddef.h>

#define PORT_VGA_CTRL 0x3D4
#define PORT_VGA_DATA 0x3D5

#define PORT_KB_DATA 0x60

#define PORT_PCI_CONF_ADDR 0xCF8
#define PORT_PCI_CONF_DATA 0xCFC

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);

#endif