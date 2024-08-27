#include "ip.h"

#include "arp.h"
#include "../../../lib/bits.h"
#include "../../tty.h"
#include "../../../lib/arpa/inet.h"
#include "../../../lib/string.h"
#include "../../../hw/cpu/system.h"
#include "../../../hw/mem.h"
#include "../l2/udp.h"
#include "../l2/tcp.h"
#include "../l2/icmp.h"

uint32_t ip_to_uint(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    return b1 << 24 | b2 << 16 | b3 << 8 | b4;
}

bool ip_is_cidr_subnet(uint32_t ip, uint32_t netip, uint32_t mask)
{
    return (
        (GET_BYTE(ip, 0) & GET_BYTE(mask, 0)) == (GET_BYTE(netip, 0) & GET_BYTE(mask, 0)) && (GET_BYTE(ip, 8) & GET_BYTE(mask, 8)) == (GET_BYTE(netip, 8) & GET_BYTE(mask, 8)) && (GET_BYTE(ip, 16) & GET_BYTE(mask, 16)) == (GET_BYTE(netip, 16) & GET_BYTE(mask, 16)) && (GET_BYTE(ip, 24) & GET_BYTE(mask, 24)) == (GET_BYTE(netip, 24) & GET_BYTE(mask, 24)));
}

uint16_t ip_calculate_checksum(ip_header *packet)
{
    uint32_t checksum = 0;

    uint8_t *data = (uint8_t *)packet;
    for (size_t i = 0; i < sizeof(ip_header); i += 2)
        checksum += (data[i] << 8) | data[i + 1];

    while (checksum >> 16)
        checksum = (checksum & 0xFFFF) + (checksum >> 16);

    return htons((uint16_t)~checksum);
}

bool ip_send_packet(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t protocol, size_t protocol_size, void *protocol_packet, size_t data_size, void *data)
{
    uint8_t dmac[6];
    if (dip == 0xFFFFFFFF)
        // broadcast
        memset(dmac, 0xFF, 6);
    else if (ip_is_cidr_subnet(dip, netdev->ip_c.gateway, netdev->ip_c.netmask))
    {
        // dprintf("[IP] ip is local, returning it's mac\n");
        if (!arp_get_mac(netdev, dip, dmac))
            return false;
    }
    else if (!arp_get_mac(netdev, netdev->ip_c.gateway, dmac))
    {
        // dprintf("[IP] ip is not local, returning gateway mac");
        return false;
    }

    ip_header *packet = (ip_header *)malloc(sizeof(ip_header) + protocol_size + data_size);
    if (!packet)
        panic("malloc failed (ip_send_packet)");

    dprintf("[IP] sending packet to ip %i (mac %m)\n", dip, dmac);

    packet->version = 4;
    packet->ihl = 5;
    packet->dscp = 0;
    packet->ecn = 0;
    packet->total_length = htons(sizeof(ip_header) + protocol_size + data_size);
    packet->identification = htons(0);
    packet->flags = 0;
    packet->fragment_offset = htons(0);
    packet->ttl = 64;
    packet->protocol = protocol;
    packet->header_checksum = htons(0);
    packet->sip = htonl(sip);
    packet->dip = htonl(dip);

    memcpy((uint8_t *)packet + sizeof(ip_header), protocol_packet, protocol_size);
    memcpy((uint8_t *)packet + sizeof(ip_header) + protocol_size, data, data_size);

    packet->header_checksum = ip_calculate_checksum(packet);

    bool res = ethernet_send_packet(netdev, dmac, ETHERTYPE_IP, packet, sizeof(ip_header) + protocol_size + data_size);
    mfree(packet);
    return res;
}

void ip_receive_packet(network_device *netdev, ip_header *packet, size_t data_size, void *data)
{
    if (packet->version != 4)
        return;

    dprintf("[IP] got packet for %i\n", ntohl(packet->dip));

    uint16_t checksum = packet->header_checksum;
    packet->header_checksum = 0;
    if (checksum != ip_calculate_checksum(packet))
        printf("[IP] packet checksum failed (expected %x, actual %x)\n", ntohs(checksum), ntohs(ip_calculate_checksum(packet)));
    else
        // packet is valid, process
        switch (packet->protocol)
        {
        case IP_PROTOCOL_UDP:
            udp_receive_packet(netdev, packet, data, data + sizeof(udp_header));
            break;
        case IP_PROTOCOL_ICMP:
            icmp_receive_packet(netdev, packet, data, data + sizeof(icmp_packet));
            break;
        case IP_PROTOCOL_TCP:
            tcp_receive_packet(netdev, packet, data, data + sizeof(tcp_header));
            break;
        default:
            printf("[IP] Recieved packet with unknown protocol\n");
        }
}