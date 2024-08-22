#include "ethernet.h"

#include "../../tty.h"
#include "../../../lib/string.h"
#include "../../../hw/mem.h"
#include "../../devices/net/e1000.h"
#include "../../devices/net/rtl8139.h"
#include "../../../lib/arpa/inet.h"
#include "../../../hw/cpu/system.h"
#include "../l1/arp.h"
#include "../l1/ip.h"
#include "../l2/udp.h"
#include "../l2/tcp.h"
#include "../l2/icmp.h"
#include "../l3/dhcp.h"
#include "../l3/dns.h"

struct network_device *devs[NUM_ETHERNET_DEVICE];

bool ethernet_send_packet(network_device *dev, uint8_t dmac[6], enum EtherType ethertype, void *data, size_t data_size)
{
    ethernet_packet *packet = (ethernet_packet *)calloc(sizeof(ethernet_packet));
    if (!packet)
        panic("calloc failed (ethernet_send_packet 2)");

    dprintf("[ETH] sending packet to %m\n", dmac);

    memcpy(packet->header.source_mac, dev->mac, 6);
    memcpy(packet->header.destination_mac, dmac, 6);

    packet->header.ethertype = htons(ethertype);
    packet->data = data;

    dev->write(dev, packet, data_size);
    mfree(packet);

    return true;
}

void ethernet_receive_packet(network_device *driver, ethernet_packet *packet, size_t data_size)
{
    dprintf("[ETH] got packet for %m\n", packet->header.destination_mac);
    switch (ntohs(packet->header.ethertype))
    {
    case ETHERTYPE_ARP:
        arp_receive_packet(driver, (arp_packet *)packet->data);
        break;

    case ETHERTYPE_IP:
        ip_receive_packet(driver, (ip_header *)packet->data, data_size - sizeof(ethernet_header), packet->data + sizeof(ip_header));
        break;
    }
}

void ethernet_irq_handler(registers_t *r)
{
    for (int i = 0; i < NUM_ETHERNET_DEVICE; i++)
    {
        if (devs[i])
            if (devs[i]->pci_device->int_line == (r->int_no - IRQ0))
                devs[i]->int_handle(devs[i], r);
    }
}

void ethernet_irq_enable()
{
    dprintf("[ETH] Enabling IRQs for all NICs\n");
    for (int i = 0; i < NUM_ETHERNET_DEVICE; i++)
        if (devs[i])
            devs[i]->int_enable(devs[i]);
}

void ethernet_irq_disable()
{
    dprintf("[ETH] Disabling IRQs for all NICs\n");
    for (int i = 0; i < NUM_ETHERNET_DEVICE; i++)
        if (devs[i])
            devs[i]->int_disable(devs[i]);
}

bool ethernet_assign_slot(network_device *dev)
{
    for (int i = 0; i < 2; i++)
    {
        if (!devs[i])
        {
            devs[i] = dev;
            dprintf("[ETH] Assigned ethernet driver to interface %d\n", i);
            return true;
        }
    }

    return false;
}

void ethernet_device_init(pci_device *pci)
{
    dprintf("[ETH] NIC init %x:%x, int line 0x%x\n", pci->vendor, pci->device, pci->int_line);

    network_device *dev = NULL;
    switch (pci->vendor)
    {
    case E1000_VEND:
        switch (pci->device)
        {
        case E1000_E1000:
        case E1000_I217:
        case E1000_82577LM:
        // case E1000_82579LM:
            dev = e1000_init(pci);
            break;
        default:
            dprintf("[ETH] Unknown Intel NIC\n");
        }
        break;
    // case RTL8139_VEND:
    //     switch (pci->device)
    //     {
    //     case RTL8139_RTL8139:
    //         dev = rtl8139_init(pci);
    //         break;
    //     default:
    //         dprintf("[ETH] Unknown Realtek NIC\n");
    //     }
    //     break;
    default:
        dprintf("[ETH] Unknown NIC\n");
    }

    if (dev == NULL)
        return;

    dev->ip_c.ip = 0;
    dev->ip_c.netmask = 0;
    dev->ip_c.gateway = 0;
    dev->ip_c.dns = 0;
    dev->ip_c.lease_time = 0;

    if (!ethernet_assign_slot(dev))
    {
        dprintf("[ETH] Too many network devices!\n");
        return;
    }

    dprintf("[ETH] NIC configuration done\n");

    irq_register(IRQ0 + pci->int_line, ethernet_irq_handler);
    dev->int_enable(dev);

    dhcp_init(dev);
}

network_device *ethernet_first_netdev()
{
    return devs[0];
}