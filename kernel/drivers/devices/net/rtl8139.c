#include "rtl8139.h"

#include "../../tty.h"
#include "../../../hw/mmio.h"
#include "../../../hw/port.h"
#include "../../../hw/mem.h"
#include "../../../lib/string.h"

inline void rtl8139_write8(network_device *dev, uint16_t address, uint8_t value)
{
    if (dev->pci_device->bar_type == 0)
    {
        mmio_write8((uint64_t *)(uintptr_t)(dev->pci_device->mem_base + address), value);
    }
    else
    {
        outb(dev->pci_device->io_base + address, value);
    }
}

inline uint8_t rtl8139_read8(network_device *dev, uint16_t address)
{
    if (dev->pci_device->bar_type == 0)
    {
        return mmio_read8((uint64_t *)(uintptr_t)(dev->pci_device->mem_base + address));
    }
    else
    {
        return inb(dev->pci_device->io_base + address);
    }
}

inline void rtl8139_write32(network_device *dev, uint16_t address, uint32_t value)
{
    if (dev->pci_device->bar_type == 0)
    {
        mmio_write32((uint64_t *)(uintptr_t)(dev->pci_device->mem_base + address), value);
    }
    else
    {
        outl(dev->pci_device->io_base + address, value);
    }
}

inline uint32_t rtl8139_read32(network_device *dev, uint16_t address)
{
    if (dev->pci_device->bar_type == 0)
    {
        return mmio_read8((uint64_t *)(uintptr_t)(dev->pci_device->mem_base + address));
    }
    else
    {
        return inl(dev->pci_device->io_base + address);
    }
}

inline void rtl8139_write16(network_device *dev, uint16_t address, uint16_t value)
{
    if (dev->pci_device->bar_type == 0)
    {
        mmio_write16((uint64_t *)(uintptr_t)(dev->pci_device->mem_base + address), value);
    }
    else
    {
        outw(dev->pci_device->io_base + address, value);
    }
}

inline uint16_t rtl8139_read16(network_device *dev, uint16_t address)
{
    if (dev->pci_device->bar_type == 0)
    {
        return mmio_read16((uint64_t *)(uintptr_t)(dev->pci_device->mem_base + address));
    }
    else
    {
        return inw(dev->pci_device->io_base + address);
    }
}

bool rtl8139_read_mac(network_device *dev)
{
    for (int i = 0; i < 6; i++)
        dev->mac[i] = rtl8139_read8(dev, RTL8139_REG_MAC + i);
    return true;
}

uint8_t rtl8139_packet_write(network_device *driver, ethernet_packet *packet, size_t data_size)
{
    return 0;
}

void rtl8139_int_enable(network_device *netdev)
{
}

void rtl8139_int_handle(network_device *dev, registers_t *r)
{
    // uint16_t status = rtl8139_read16(dev, RTL8139_REG_ISR);
    // rtl8139_write16(dev, RTL8139_REG_ISR, 0x05);

    printf("[RTL8139] INTERRUPT\n");

    // if (status & TOK)
    // {
    //     // Sent
    // }
    // if (status & ROK)
    // {
    //     // Received
    //     receive_packet();
    // }
}

network_device *rtl8139_init(pci_device *pci)
{
    network_device *netdev = (network_device *)malloc(sizeof(network_device));
    if (!netdev)
        panic("malloc failed (e1000 init)");

    netdev->pci_device = pci;

    printf("[RTL8139] Driver init for dev at MEMBAR: 0x%x, IOBAR: 0x%x, BAR0 Type: %x\n", netdev->pci_device->mem_base, netdev->pci_device->io_base, netdev->pci_device->bar_type);

    rtl8139_write8(netdev, RTL8139_REG_CONF1, 0x0);
    rtl8139_read_mac(netdev);

    printf("[RTL8139] MAC Address: %m\n", netdev->mac);

    rtl8139_write8(netdev, RTL8139_REG_CMD, 0x10);
    while ((rtl8139_read8(netdev, RTL8139_REG_CMD) & 0x10) != 0)
        ;

    // init rx
    void *ptr = calloc(8192 + 16);
    rtl8139_write32(netdev, RTL8139_REG_RBSTART, (uintptr_t)ptr);

    // interrupt mask register
    rtl8139_write16(netdev, RTL8139_REG_IMR, 0xFF);

    // recieve packets to all MACs
    rtl8139_write32(netdev, RTL8139_REG_RXC, 0xF | (1 << 7));

    netdev->write = &rtl8139_packet_write;
    netdev->int_enable = &rtl8139_int_enable;
    // netdev->int_disable = &rtl8139_int_disable;
    netdev->int_handle = &rtl8139_int_handle;

    return netdev;
}