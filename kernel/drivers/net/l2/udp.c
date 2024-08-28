#include "udp.h"

#include "../../../hw/mem.h"
#include "../../../hw/cpu/system.h"
#include "../../tty.h"
#include "../l1/ip.h"
#include "../../../lib/arpa/inet.h"
#include "../../../lib/string.h"

udp_listener *udp_port_listeners;

void udp_init(void)
{
    dprintf("[UDP] init, calloc %f\n", 65536 * sizeof(udp_listener));
    udp_port_listeners = (udp_listener *)calloc(65536 * sizeof(udp_listener));
    if (!udp_port_listeners)
        panic("calloc failed (udp_init)");
}

bool udp_install_listener(uint16_t dport, udp_listener listener)
{
    if (udp_port_listeners[dport])
        return false;

    udp_port_listeners[dport] = listener;
    return true;
}
void udp_uninstall_listener(uint16_t dport)
{
    udp_port_listeners[dport] = 0;
}

uint16_t udp_calculate_checksum(udp_header *packet, uint32_t sip, uint32_t dip, void *payload, size_t payload_size)
{
    uint32_t checksum = 0;

    ip_pseudo_header *pseudo_header = (ip_pseudo_header *)calloc(sizeof(ip_pseudo_header));
    if (!pseudo_header)
        panic("calloc failed (udp_calculate_checksum)");
    pseudo_header->sip = sip;
    pseudo_header->dip = dip;
    pseudo_header->proto = IP_PROTOCOL_UDP;
    pseudo_header->len = packet->len;

    uint8_t *current = (uint8_t *)pseudo_header;
    for (size_t i = 0; i < sizeof(ip_pseudo_header); i += 2)
        checksum += (((i + 1) == sizeof(ip_pseudo_header) ? 0 : current[i + 1] << 8) | current[i]);
    mfree(pseudo_header);

    uint32_t old_checksum = packet->checksum;
    packet->checksum = 0;
    current = (uint8_t *)packet;
    for (size_t i = 0; i < sizeof(udp_header); i += 2)
        checksum += (((i + 1) == sizeof(udp_header) ? 0 : current[i + 1] << 8) | current[i]);
    packet->checksum = old_checksum;

    current = (uint8_t *)payload;
    for (size_t i = 0; i < payload_size; i += 2)
        checksum += (((i + 1) == payload_size ? 0 : current[i + 1] << 8) | current[i]);

    // add first and second words of checksum together (0x12345678 -> (0x1234 + 0x5678) -> 0x68AC), and invert if all 1s (per spec)
    checksum = (checksum >> 16) + (checksum & 0xFFFF);
    if (checksum != 0xFFFFFFFF)
        checksum = ~checksum;

    return (uint16_t)checksum;
}

bool udp_send_packet(network_device *netdev, uint32_t sip, uint16_t sport, uint32_t dip, uint16_t dport, void *data, size_t data_size)
{
    udp_header *packet = (udp_header *)calloc(sizeof(udp_header));
    if (!packet)
        panic("calloc failed (udp_send_packet)");

    dprintf("[UDP] sending packet to port %d (ip %i)\n", dport, dip);

    packet->dport = htons(dport);
    packet->sport = htons(sport);
    packet->len = htons(sizeof(udp_header) + data_size);
    packet->checksum = udp_calculate_checksum(packet, htonl(sip), htonl(dip), data, data_size);

    bool res = ip_send_packet(netdev, sip, dip, IP_PROTOCOL_UDP, sizeof(udp_header), packet, data_size, data);
    mfree(packet);
    return res;
}

void udp_receive_packet(network_device *driver, ip_header *ip, udp_header *packet, void *data)
{
    dprintf("[UDP] got packet for port %d\n", ntohs(packet->dport));
    if (packet->checksum != udp_calculate_checksum(packet, ip->sip, ip->dip, data, ntohs(packet->len) - sizeof(udp_header)))
        printf("[UDP] packet checksum failed (expected %x, actual %x)\n", packet->checksum, udp_calculate_checksum(packet, ip->sip, ip->dip, data, ntohs(packet->len) - sizeof(udp_header)));
    else if (udp_port_listeners[ntohs(packet->dport)])
        udp_port_listeners[ntohs(packet->dport)](driver, ip, packet, data, ntohs(packet->len) - sizeof(udp_header));
}