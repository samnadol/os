#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>
#include "../l0/ethernet.h"
#include "../l1/ip.h"

typedef struct __attribute__((packed, aligned(1))) icmp_packet
{
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t checksum;
    union
    {
        struct
        {

            uint16_t echo_id;
            uint16_t echo_seq;
        } echo_packet;
    };
    uint16_t data[];
} icmp_packet;

enum ICMP_Packet_Types
{
    ICMP_PACKET_REQUEST = 0x8,
    ICMP_PACKET_REPLY = 0x0,
    ICMP_PACKET_UNREACAHABLE = 0x3,
    ICMP_PACKET_FRAGMENTATION_REQ = 0x4,
};

bool icmp_send_packet(network_device *netdev, uint32_t dest_ip, size_t seq);
void icmp_receive_packet(network_device *driver, ip_header *ip_packet, icmp_packet *packet, void *data);

#endif