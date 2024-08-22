#ifndef RTL8139_H
#define RTL8139_H

#include "../../net/l0/ethernet.h"

#define RTL8139_VEND 0x10EC
#define RTL8139_RTL8139 0x8139

network_device *rtl8139_init(pci_device *pci);

enum RTL8139_Registers
{
    // ID Register
    RTL8139_REG_MAC = 0x00,
    // Multicast Register
    RTL8139_REG_MAR = 0x08,
    // RX Buffer Start Address
    RTL8139_REG_RBSTART = 0x30,
    // Command Register
    RTL8139_REG_CMD = 0x37,
    // Interrupt Mask Register
    RTL8139_REG_IMR = 0x3C,
    // Interrupt Status Register
    RTL8139_REG_ISR = 0x3E,
    // RX Configuration Register
    RTL8139_REG_RXC = 0x44,
    // CONFIG0 Register
    RTL8139_REG_CONF0 = 0x51,
    // CONFIG1 Register
    RTL8139_REG_CONF1 = 0x52,
};

#endif