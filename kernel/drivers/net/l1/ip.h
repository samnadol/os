#pragma once

#include "../l0/ethernet.h"

enum IPProtocolNumbers
{
    IP_PROTOCOL_ICMP = 0x01,
    IP_PROTOCOL_TCP = 0x06,
    IP_PROTOCOL_UDP = 0x11,
};

typedef struct __attribute__((packed, aligned(1))) ip_header
{
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t ecn : 2;
    uint8_t dscp : 6;
    uint16_t total_length;
    uint16_t identification;
    uint16_t fragment_offset : 13;
    uint8_t flags : 3;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t sip;
    uint32_t dip;
} ip_header;

#pragma pack(push, 1)
typedef struct ip_pseudo_header
{
    uint32_t sip;
    uint32_t dip;
    uint8_t _;
    uint8_t proto;
    uint16_t len;
} ip_pseudo_header;
#pragma pack(pop)

bool ip_send_packet(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t protocol, size_t protocol_size, void *protocol_packet, size_t data_size, void *data);
void ip_receive_packet(network_device *netdev, ip_header *packet, size_t data_size, void *data);
uint32_t ip_to_uint(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);