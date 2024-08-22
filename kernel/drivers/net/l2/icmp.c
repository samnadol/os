#include "icmp.h"

#include "../../../hw/mem.h"

uint16_t icmp_calculate_checksum(icmp_packet *packet)
{
    uint32_t checksum = 0;
    uint8_t *data = (uint8_t *)packet;
    for (size_t i = 0; i < sizeof(icmp_packet); i += 2)
    {
        checksum += (data[i] << 8) | data[i + 1];
    }

    while (checksum >> 16)
    {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    return ~checksum;
}

bool icmp_send_packet(network_device *netdev, uint32_t dest_ip, size_t seq)
{
    icmp_packet *packet = (icmp_packet *)calloc(sizeof(icmp_packet));
    packet->icmp_type = ICMP_PACKET_REQUEST;
    packet->icmp_code = 0;
    packet->echo_packet.echo_id = 0xEF;
    packet->echo_packet.echo_seq = seq;
    packet->checksum = icmp_calculate_checksum(packet);

    printf("[ICMP] Sent ID %x, seq %d\n", packet->echo_packet.echo_id, packet->echo_packet.echo_seq);

    bool res = ip_send_packet(netdev, netdev->ip_c.ip, dest_ip, IP_PROTOCOL_ICMP, sizeof(icmp_packet), packet, 0, 0);
    mfree(packet);
    return res;
}

void icmp_process_reply(network_device *netdev, icmp_packet *packet)
{
    printf("[ICMP] Reply ID %x, seq %d\n", packet->echo_packet.echo_id, packet->echo_packet.echo_seq);
}

void icmp_process_request(network_device *netdev, icmp_packet *packet)
{
    printf("[ICMP] Request\n");
}

void icmp_process_unreach(network_device *netdev, icmp_packet *packet)
{
    printf("[ICMP] Destination unreachable\n");
}

void icmp_receive_packet(network_device *netdev, ip_header *ip_packet, icmp_packet *packet, void *data)
{
    switch (packet->icmp_type)
    {
    case ICMP_PACKET_REQUEST:
        icmp_process_request(netdev, packet);
        break;
    case ICMP_PACKET_REPLY:
        icmp_process_reply(netdev, packet);
        break;
    case ICMP_PACKET_UNREACAHABLE:
        icmp_process_unreach(netdev, packet);
        break;
    default:
        break;
    }
}