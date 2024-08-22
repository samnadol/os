#include "com.h"

#include "../../../hw/port.h"
#include "../../../hw/cpu/pic.h"
#include "../../serial.h"
#include "../../../hw/mem.h"

bool com_has_data(com_device *dev)
{
    return (inb(dev->io_port + R_LINE_STATUS) & 0x1) == 1;
}

void com_out(com_device *dev, char a)
{
    while ((inb(dev->io_port + R_LINE_STATUS) & 0x20) == 0)
        ;
    outb(dev->io_port, a);
}

char com_in(com_device *dev)
{
    while ((inb(dev->io_port + R_LINE_STATUS) & 0x1) == 0)
        ;
    return inb(dev->io_port);
}

void com_int_handler(registers_t *r, com_device *dev, tty_interface *tty)
{
    tty->tty_kb_in(tty, com_in(dev));
}

void com_int_enable(com_device *dev)
{
    outb(dev->io_port + W_DLAB0_INTERRUPT_ENABLE, 0x01);
}

com_device *com_init(uint32_t io_port)
{
    com_device *dev = (com_device *)calloc(sizeof(com_device));
    dev->io_port = io_port;

    outb(dev->io_port + W_DLAB0_INTERRUPT_ENABLE, 0x00); // Disable all interrupts
    outb(dev->io_port + W_LINE_CONTROL, 0x80);           // DLAB enable (set baud rate divisor)
    outb(dev->io_port + W_DLAB1_BAUD_LSB, 0x1);          // Set divisor to 3 (lo byte) 38400 baud
    outb(dev->io_port + W_DLAB1_BAUD_MSB, 0x00);         //                  (hi byte)
    outb(dev->io_port + W_LINE_CONTROL, 0x03);           // DLAB disable, 8 bits, no parity, one stop bit
    outb(dev->io_port + W_FIFO_CONTROL, 0xC7);           // Enable FIFO, clear them, with 14-byte threshold
    outb(dev->io_port + W_MODEM_CONTROL, 0xB);           // IRQs enabled, RTS/DSR set
    outb(dev->io_port + W_MODEM_CONTROL, 0x1E);          // Set in loopback mode, test the serial chip

    // Test serial chip (send byte 0xAE and check if serial returns same byte)
    outb(dev->io_port + W_DLAB0_TRANSMIT, 0xAE);
    if (inb(dev->io_port + R_DLAB0_RECIEVE) != 0xAE)
        return 0;
        
    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(dev->io_port + W_MODEM_CONTROL, 0x0F);

    return dev;
}