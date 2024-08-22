#ifndef COM_H
#define COM_H

#include <stdint.h>
#include <stdbool.h>
#include "../../../hw/cpu/irq.h"

enum COM_Read_Registers
{
    R_DLAB0_RECIEVE = 0x0,
    R_DLAB1_BAUD_LSB = 0x0,
    R_DLAB0_INTERRUPT_ENABLE = 0x1,
    R_DLAB1_BAUD_MSB = 0x1,
    R_INTERRUPT_IDENT = 0x2,
    R_LINE_CONTROL = 0x3,
    R_MODEM_CONTROL = 0x4,
    R_LINE_STATUS = 0x5,
    R_MODEM_STATUS = 0x6,
    R_SCRATCH = 0x7,
};

enum COM_Write_Registers
{
    W_DLAB0_TRANSMIT = 0x0,
    W_DLAB1_BAUD_LSB = 0x0,
    W_DLAB0_INTERRUPT_ENABLE = 0x1,
    W_DLAB1_BAUD_MSB = 0x1,
    W_FIFO_CONTROL = 0x2,
    W_LINE_CONTROL = 0x3,
    W_MODEM_CONTROL = 0x4,
    W_SCRATCH = 0x7,
};

typedef struct com_device
{
    uint16_t io_port; // IO Port to use, if type = 0x0 COM
} com_device;

com_device *com_init(uint32_t io_port);
bool com_has_data(com_device *dev);
void com_out(com_device *dev, char a);
void com_int_handler(registers_t *r, com_device *dev, tty_interface *tty);
void com_int_enable(com_device *dev);

#endif