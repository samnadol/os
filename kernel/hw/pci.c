#include "pci.h"

#include "./port.h"
#include "../hw/mem.h"
#include "cpu/system.h"
#include "../lib/string.h"
#include "../drivers/net/l0/ethernet.h"
#include "../drivers/serial.h"

pci_device *active_pci_device_list;

void pci_conf_set_addr(uint32_t bus, uint32_t slot, uint32_t function, uint16_t offset)
{
    outl(PORT_PCI_CONF_ADDR, (uint32_t)((bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC) | ((uint32_t)0x80000000)));
}

uint32_t pci_conf_inl(uint32_t bus, uint32_t slot, uint32_t function, uint16_t offset)
{
    pci_conf_set_addr(bus, slot, function, offset);
    return inl(PORT_PCI_CONF_DATA);
}

uint16_t pci_conf_inw(uint32_t bus, uint32_t slot, uint32_t function, uint8_t offset)
{
    uint32_t value = pci_conf_inl(bus, slot, function, offset & ~0x2);
    return (uint16_t)((value >> ((offset & 2) * 8)) & 0xFFFF);
}

void pci_conf_outl(uint32_t bus, uint32_t slot, uint32_t function, uint16_t offset, uint32_t data)
{
    pci_conf_set_addr(bus, slot, function, offset);
    outl(PORT_PCI_CONF_DATA, data);
}

void pci_conf_outw(uint32_t bus, uint32_t slot, uint32_t function, uint16_t offset, uint16_t value)
{
    uint32_t old_value = pci_conf_inw(bus, slot, function, offset & ~0x2);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t new_value = (old_value & ~mask) | (value << shift);

    pci_conf_outl(bus, slot, function, offset & ~0x2, new_value);
}

uint16_t pci_get_vendor_id(uint32_t bus, uint32_t slot, uint32_t function)
{
    return pci_conf_inl(bus, slot, function, 0x0);
}

uint16_t pci_get_device_id(uint32_t bus, uint32_t slot, uint32_t function)
{
    return pci_conf_inl(bus, slot, function, 0x0) >> 16;
}

uint8_t pci_get_class_id(uint32_t bus, uint32_t slot, uint32_t function)
{
    return (pci_conf_inl(bus, slot, function, 0x8) >> 24) & 0xFF;
}

uint8_t pci_get_subclass_id(uint32_t bus, uint32_t slot, uint32_t function)
{
    return (pci_conf_inl(bus, slot, function, 0x8) >> 16) & 0xFF;
}

bool pci_enable(pci_device *dev)
{
    uint16_t command_reg = pci_conf_inw(dev->bus, dev->slot, dev->function, 0x04 /* command register offset in pci configuration space */);

    command_reg |= (1 << 8); // Set bit 08 for DMA
    command_reg |= (1 << 2); // Set bit 02 for bus mastering
    command_reg |= (1 << 1); // Set bit 01 for memory space access
    command_reg |= (1 << 0); // Set bit 00 for IO space access

    pci_conf_outw(dev->bus, dev->slot, dev->function, 0x04, command_reg);

    uint16_t verify_command = pci_conf_inw(dev->bus, dev->slot, dev->function, 0x04);
    if (verify_command != command_reg)
        return false;

    return true;
}

bool pci_device_active(uint32_t bus, uint32_t slot, uint32_t function)
{
    pci_device *dev = active_pci_device_list;
    while (dev)
    {
        if (dev->bus == bus && dev->slot == slot && dev->function == function)
            return true;
        dev = dev->next;
    }
    return false;
}

void pci_print(tty_interface *tty)
{
    for (uint32_t bus = 0; bus <= 0xFF; bus++)
    {
        for (uint32_t slot = 0; slot < 32; slot++)
        {
            for (uint32_t function = 0; function < 8; function++)
            {
                uint16_t vendor = pci_get_vendor_id(bus, slot, function);
                if (vendor == 0xFFFF)
                    continue;

                tprintf(tty, "%d:%d:%d %s\n", bus, slot, function, pci_device_active(bus, slot, function) ? "(ACTIVE)" : "");
                tprintf(tty, "\t%x %x %x\n", vendor, pci_get_device_id(bus, slot, function), pci_get_class_id(bus, slot, function));
            }
        }
    }
}

void init_device(pci_device *dev, void (*driver_init)(pci_device *dev))
{
    if (!pci_enable(dev))
    {
        dprintf("Failed to enable device\n");
        return;
    }

    for (int i = 0x10; i < 0x28; i += 0x4)
    {
        uint32_t res = pci_conf_inl(dev->bus, dev->slot, dev->function, i);
        if (res == 0)
            continue;
        else if ((res & 0b1) == PCI_BAR_IO)
            dev->io_base = (uint32_t)(res & ~0b111);
        else if ((res & 0b0) == PCI_BAR_MEM)
            dev->mem_base = (uint32_t)(res & ~0b111);
    }

    dev->bar_type = pci_conf_inl(dev->bus, dev->slot, dev->function, 0x10) & 0b1;
    dev->int_line = pci_conf_inl(dev->bus, dev->slot, dev->function, 0x3C) & 0xFF;

    driver_init(dev);
}

void pci_init()
{
    dprintf("[PCI] Scanning for devices\n");

    for (uint32_t bus = 0; bus <= 0xFF; bus++)
    {
        for (uint32_t slot = 0; slot < 32; slot++)
        {
            for (uint32_t function = 0; function < 8; function++)
            {
                uint16_t vendor = pci_get_vendor_id(bus, slot, function);
                if (vendor == 0xFFFF)
                    continue;

                pci_device *new = malloc(sizeof(pci_device));
                if (!new)
                    panic("malloc failed (pci_init)");

                new->bus = bus;
                new->slot = slot;
                new->function = function;

                new->vendor = vendor;
                new->device = pci_get_device_id(bus, slot, function);
                new->class = pci_get_class_id(bus, slot, function);
                new->subclass = pci_get_subclass_id(bus, slot, function);
                new->next = NULL;

                switch (new->class)
                {
                // case 0x01:
                //     printf("[PCI] Mass Storage Controller\n");
                //     break;
                case 0x02:
                    dprintf("[PCI] Ethernet Controller\n");
                    switch (new->subclass)
                    {
                    case 0x00:
                        init_device(new, ethernet_device_init);
                        break;
                    default:
                        mfree(new);
                        continue;
                    }
                    break;
                // case 0x07:
                // case 0x0c:
                //     dprintf("[PCI] Serial Controller\n");
                //     init_device(new, serial_add_pci_device);
                //     break;
                default:
                    mfree(new);
                    continue;
                }

                if (!active_pci_device_list)
                {
                    active_pci_device_list = new;
                }
                else
                {
                    pci_device *i;
                    for (i = active_pci_device_list; i->next != NULL; i = i->next)
                        ;
                    i->next = new;
                }
            }
        }
    }
}