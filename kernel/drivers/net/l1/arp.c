#include "arp.h"

#include "../../../lib/bits.h"
#include "../../../hw/timer.h"
#include "../../../hw/cpu/system.h"
#include "../../../lib/arpa/inet.h"
#include "../../../hw/mem.h"
#include "../../../lib/string.h"

typedef struct arp_entry
{
    uint32_t ip;
    uint8_t mac[6];

    struct arp_entry *next;
    struct arp_entry *prev;

    uint16_t ttl;
    uint64_t created;
} arp_entry;
arp_entry *arp_cache;

void arp_init()
{
    arp_cache = 0;
}

void arp_print(tty_interface *tty)
{
    tprintf(tty, "Current ARP Table:\n");

    struct arp_entry *current = arp_cache;
    while (current)
    {
        tprintf(tty, "\t%i %m ttl: %d\n", current->ip, current->mac, current->ttl);
        tprintf(tty, "\tcreated %d, expires %d, now %d\n", (uint32_t)current->created, (uint32_t)(current->created + current->ttl), (uint32_t)timer_get_epoch());
        current = current->next;
    }
}

bool arp_send_request(network_device *netdev, uint32_t ip)
{
    dprintf("[ARP] sending request for %i\n", ip);

    arp_packet *packet = (arp_packet *)malloc(sizeof(arp_packet));
    packet->hardware_type = htons(ARP_HW_ETHERNET);
    packet->protocol_type = htons(ARP_PROTOCOL_IP);
    packet->hwsize = 6;
    packet->prosize = 4;
    packet->opcode = htons(ARP_OPCODE_REQUEST);

    packet->smac[0] = netdev->mac[0];
    packet->smac[1] = netdev->mac[1];
    packet->smac[2] = netdev->mac[2];
    packet->smac[3] = netdev->mac[3];
    packet->smac[4] = netdev->mac[4];
    packet->smac[5] = netdev->mac[5];

    packet->dmac[0] = 0;
    packet->dmac[1] = 0;
    packet->dmac[2] = 0;
    packet->dmac[3] = 0;
    packet->dmac[4] = 0;
    packet->dmac[5] = 0;

    packet->sip = htonl(netdev->ip_c.ip);
    packet->dip = htonl(ip);

    uint8_t dmac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bool res = ethernet_send_packet(netdev, dmac, ETHERTYPE_ARP, packet, sizeof(arp_packet));
    mfree(packet);
    return res;
}

bool arp_send_reply(network_device *netdev, uint32_t dip, uint8_t *mac)
{
    arp_packet *packet = (arp_packet *)malloc(sizeof(arp_packet));
    if (!packet)
        panic("malloc failed (arp_send_reply)");

    packet->hardware_type = htons(ARP_HW_ETHERNET);
    packet->protocol_type = htons(ARP_PROTOCOL_IP);
    packet->hwsize = 0x06;
    packet->prosize = 0x04;
    packet->opcode = htons(ARP_OPCODE_REPLY);

    packet->smac[0] = netdev->mac[0];
    packet->smac[1] = netdev->mac[1];
    packet->smac[2] = netdev->mac[2];
    packet->smac[3] = netdev->mac[3];
    packet->smac[4] = netdev->mac[4];
    packet->smac[5] = netdev->mac[5];

    packet->dmac[0] = mac[0];
    packet->dmac[1] = mac[1];
    packet->dmac[2] = mac[2];
    packet->dmac[3] = mac[3];
    packet->dmac[4] = mac[4];
    packet->dmac[5] = mac[5];

    packet->sip = htonl(netdev->ip_c.ip);
    packet->dip = dip;

    bool res = ethernet_send_packet(netdev, mac, ETHERTYPE_ARP, packet, sizeof(arp_packet));
    mfree(packet);
    return res;
}

bool arp_has_ip(uint32_t ip)
{
    struct arp_entry *current = arp_cache;
    while (current)
    {
        if (current->ip == ip)
            return true;
        current = current->next;
    }
    return false;
}

bool arp_get_mac(network_device *netdev, uint32_t ip, uint8_t mac[6])
{
    struct arp_entry *current = arp_cache;
    while (current)
    {
        if (current->ip == ip)
        {
            if ((uint32_t)(timer_get_epoch() - current->created) > current->ttl)
            {
                // expired
                dprintf("[ARP] cached mac for %i is expired\n", ip);

                if (current == arp_cache) // if first entry, then set first entry to next entry
                    arp_cache = current->next;
                else // if not first entry, then set previous entry's next to next
                    current->prev->next = current->next;
                current->next->prev = current->prev;

                mfree(current);
                break;
            }
            else
            {
                dprintf("[ARP] cached mac for %i is %m\n", ip, current->mac);
                memcpy(mac, current->mac, 6);
                return true;
            }
        }
        current = current->next;
    }

    // don't have in arp table
    if (!arp_send_request(netdev, ip))
        return false;

    // block until arp response is recieved
    uint64_t ms_elapsed = 0;
    while (!arp_has_ip(ip))
    {
        timer_wait(10);
        ms_elapsed += 10;

        if (ms_elapsed > ARP_TIMEOUT_MS)
            return false;
    }

    return arp_get_mac(netdev, ip, mac);
}

void arp_process_request(network_device *netdev, arp_packet *packet)
{
    if (ntohl(packet->dip) == netdev->ip_c.ip)
        arp_send_reply(netdev, packet->sip, packet->smac);
}

void arp_process_reply(network_device *netdev, arp_packet *packet)
{
    dprintf("[ARP] got reply for %i -> %m\n", ntohl(packet->sip), packet->smac);
    if (!arp_has_ip(ntohl(packet->sip)))
    {
        struct arp_entry *arp_entry = (struct arp_entry *)malloc(sizeof(struct arp_entry));
        if (!packet)
            panic("malloc failed (arp_process_reply)");

        arp_entry->ip = ntohl(packet->sip);
        memcpy(arp_entry->mac, packet->smac, 6);
        arp_entry->ttl = 60;
        arp_entry->created = timer_get_epoch();
        arp_entry->next = 0;

        if (!arp_cache)
            arp_cache = arp_entry;
        else
        {
            struct arp_entry *current = arp_cache;
            while (current->next)
                current = current->next;

            arp_entry->prev = current;

            current->next = arp_entry;
        }
    }
}

void arp_receive_packet(network_device *netdev, arp_packet *packet)
{
    switch (ntohs(packet->opcode))
    {
    case ARP_OPCODE_REQUEST:
        arp_process_request(netdev, packet);
        break;
    case ARP_OPCODE_REPLY:
        arp_process_reply(netdev, packet);
        break;
    }
}