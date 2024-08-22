#ifndef QEMU_PCI_SERIAL_H
#define QEMU_PCI_SERIAL_H

#include "../../serial.h"
#include "../../tty.h"

serial_device *qemu_pci_serial_init(pci_device *pci);
bool qemu_pci_serial_has_data(pci_device *dev);
void qemu_pci_serial_out(pci_device *dev, char a);
void qemu_pci_serial_int_handler(registers_t *r, pci_device *dev, tty_interface *tty);

#endif