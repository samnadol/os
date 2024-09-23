#include "serial.h"

#include "../hw/mem.h"
#include "com.h"
#include "devices/serial/qemu-pci-serial.h"
#include "../user/shell.h"
#include "../lib/string.h"

serial_device *devices[4];
tty_interface *ttys[4];
TTYColor prevColor;
size_t device_count = 0;

size_t serial_out(TTYColor color, const char *s)
{
    if (device_count == 0)
        return 0;

    if (prevColor != color) {
        char buf[32];
        sprintf(buf, "\x1B[%dm", serialtty_colors[color]);
        serial_out(prevColor, buf);
        prevColor = color;
    }

    size_t i = 0;
    for (int j = 0; j < device_count; j++)
    {
        i = 0;
        while (s[i] != 0)
        {
            switch (devices[j]->type)
            {
            case SERIAL_COM:
                if (s[i] == '\n')
                    devices[j]->com_out(devices[j]->com_device, '\r');
                devices[j]->com_out(devices[j]->com_device, s[i]);
                break;
            case SERIAL_PCI:
                if (s[i] == '\n')
                    devices[j]->pci_out(devices[j]->pci_device, '\r');
                devices[j]->pci_out(devices[j]->pci_device, s[i]);
                break;
            }
            i++;
        }
    }
    return i;
}

void serial_in(tty_interface *tty, char c)
{
    if (c == 0x7F || c == 0x08)
        shell_backspace(tty);
    else if (c == 0xD)
        shell_enter(tty);
    else if (c == 0x3)
        shell_control(tty, 'c', false);
    else
        shell_add_key(tty, c);
}

void serial_handle_interrupt(registers_t *r)
{
    for (int j = 0; j < device_count; j++)
    {
        switch (devices[j]->type)
        {
        case SERIAL_COM:
            if (devices[j]->com_has_data(devices[j]->com_device))
            {
                devices[j]->com_int_handle(r, devices[j]->com_device, ttys[j]);
            }
            break;
        case SERIAL_PCI:
            if (devices[j]->pci_has_data(devices[j]->pci_device))
            {
                devices[j]->pci_int_handle(r, devices[j]->pci_device, ttys[j]);
            }
            break;
        }
    }
}

void serial_int_enable()
{
    for (int j = 0; j < device_count; j++)
    {
        switch (devices[j]->type)
        {
        case SERIAL_COM:
            irq_register(IRQ0 + SERIAL_IRQ, serial_handle_interrupt);
            devices[j]->com_int_enable(devices[j]->com_device);
            break;
        case SERIAL_PCI:
            irq_register(IRQ0 + devices[j]->pci_device->int_line, serial_handle_interrupt);
            devices[j]->pci_int_enable(devices[j]->pci_device);
            break;
        }
    }
}

void serial_add_com_device()
{
    serial_device *new = (serial_device *)calloc(sizeof(serial_device));
    new->type = SERIAL_COM;
    new->com_device = com_init(SERIAL_COM1);
    new->com_out = &com_out;
    new->com_int_handle = &com_int_handler;
    new->com_int_enable = &com_int_enable;
    new->com_has_data = &com_has_data;

    devices[device_count] = new;
    ttys[device_count] = tty_add_interface(TTYType_SERIAL_COM, NULL, serial_delc, serial_out, serial_in, new);
    device_count++;
}

void serial_add_pci_device(pci_device *pci)
{
    printf("[SER] Adding PCI serial device %x:%x\n", pci->vendor, pci->device);

    serial_device *dev = 0;
    switch (pci->vendor)
    {
    case QEMU_SERIAL_VENDOR:
        switch (pci->device)
        {
        case QEMU_SERIAL_DEVICE:
            dev = qemu_pci_serial_init(pci);
            break;
        default:
            printf("[SER] Unknown QEMU serial card\n");
        }
        break;
    default:
        printf("[SER] Unknown serial card\n");
    }

    if (dev == NULL)
        return;

    devices[device_count] = dev;
    ttys[device_count] = tty_add_interface(TTYType_SERIAL_PCI, NULL, serial_delc, serial_out, serial_in, dev);
    device_count++;
}

void serial_init()
{
    serial_add_com_device();
}

void serial_delc()
{
    serial_out(prevColor, "\033[1D\033[0K");
}

tty_interface *serial_get_tty()
{
    return ttys[0];
}