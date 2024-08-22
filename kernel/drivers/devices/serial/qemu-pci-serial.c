#include "qemu-pci-serial.h"

#include "../../../hw/mem.h"
#include "../../../hw/port.h"

com_device *com;

void qemu_pci_serial_out(pci_device *dev, char a)
{
    com_out(com, a);
}

void qemu_pci_serial_int_handler(registers_t *r, pci_device *dev, tty_interface *tty)
{
    com_int_handler(r, com, tty);
}

void qemu_pci_serial_int_enable(pci_device *dev)
{
    com_int_enable(com);
}

bool qemu_pci_serial_has_data(pci_device *dev)
{
    return com_has_data(com);
}

serial_device *qemu_pci_serial_init(pci_device *pci)
{
    printf("[QEM] Serial driver init for dev at MEMBAR: %x, IOBAR: ", pci->mem_base);
    printf("%x, BAR0 Type: ", pci->io_base);
    printf("%x\n", pci->bar_type);

    serial_device *new = (serial_device *)calloc(sizeof(serial_device));
    new->type = SERIAL_PCI;
    new->pci_device = pci;
    new->pci_out = &qemu_pci_serial_out;
    new->pci_int_handle = &qemu_pci_serial_int_handler;
    new->pci_has_data = &qemu_pci_serial_has_data;
    new->pci_int_enable = &qemu_pci_serial_int_enable;

    com = com_init(pci->io_base);

    return new;
}