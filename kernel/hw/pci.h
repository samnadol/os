#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stdint.h>
#include <stddef.h>
#include "../drivers/tty.h"

#define PCI_COMMAND_IO 0x1
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_MASTER 0x4

#define PCI_BAR_MEM 0x0
#define PCI_BAR_IO 0x1

typedef struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;

    uint8_t class;
    uint8_t subclass;

    uint16_t vendor;
    uint16_t device;

    uint8_t bar_type;  // Type of BAR0
    uint16_t io_base;  // IO Base Address (BAR)
    uint64_t mem_base; // MMIO Base Address (BAR)

    uint16_t int_line;

    struct pci_device *next;
} pci_device;

void pci_print(tty_interface *tty);
void pci_init();

uint32_t pci_conf_inl(uint32_t bus, uint32_t slot, uint32_t function, uint16_t offset);
void pci_conf_outl(uint32_t bus, uint32_t slot, uint32_t function, uint16_t offset, uint32_t data);

uint16_t pci_get_vendor_id(uint32_t bus, uint32_t slot, uint32_t function);
uint16_t pci_get_device_id(uint32_t bus, uint32_t slot, uint32_t function);

#endif