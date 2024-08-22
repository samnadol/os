#ifndef SERIAL_H
#define SERIAL_H

#include "../hw/pci.h"
#include "../hw/cpu/irq.h"
#include "devices/serial/com.h"
#include "./tty.h"

#define SERIAL_COM1 0x3F8
#define SERIAL_IRQ 0x4

#define QEMU_SERIAL_VENDOR 0x1B36
#define QEMU_SERIAL_DEVICE 0x0002

enum SerialDeviceType
{
    SERIAL_COM = 0x0,
    SERIAL_PCI = 0x1,
};

enum SerialColors
{
    SERIAL_RED = 31,
    SERIAL_GREEN = 32,
    SERIAL_YELLOW = 33,
    SERIAL_BLUE = 34,
    SERIAL_MAGENTA = 35,
    SERIAL_CYAN = 36,
    SERIAL_WHITE = 37,
};

static uint8_t serialtty_colors[] = {
    SERIAL_WHITE,
    SERIAL_RED,
};

typedef struct serial_device
{
    enum SerialDeviceType type; // Type of device, PCI or COM port

    pci_device *pci_device; // Physical PCI device, if type = 0x1 PCI
    bool (*pci_has_data)(pci_device *dev);
    void (*pci_out)(pci_device *dev, char c);
    void (*pci_int_handle)(registers_t *r, pci_device *dev, tty_interface *tty);
    void (*pci_int_enable)(pci_device *dev);

    com_device *com_device; // Virtual COM device, if type = 0x0 COM
    bool (*com_has_data)(com_device *dev);
    void (*com_out)(com_device *dev, char c);
    void (*com_int_handle)(registers_t *r, com_device *dev, tty_interface *tty);
    void (*com_int_enable)(com_device *dev);
} serial_device;

// initializes serial ports
void serial_init();
void serial_int_enable();
tty_interface *serial_get_tty();

// deletes the character to the immediate left of the cursor
void serial_delc();

// writes given string to serial. returns number of chars written.
size_t serial_out(TTYColor color, const char *s);

// processes character from serial input.
void serial_in(tty_interface *tty, char c);

void serial_add_pci_device(pci_device *pci);

#endif