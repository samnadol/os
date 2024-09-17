#ifndef NET_UDP_H
#define NET_UDP_H

#include <stdint.h>
#include "../l0/ethernet.h"
#include "../l1/ip.h"

typedef struct __attribute__((packed, aligned(1))) udp_header
{
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t checksum;
} udp_header;

typedef void (*udp_listener)(network_device *, ip_header *, udp_header *, void *, size_t);
typedef struct udp_listener_node {
    uint16_t dport;
    udp_listener listener;

    struct udp_listener_node *next;
    struct udp_listener_node *prev;
} udp_listener_node;


void udp_init(void);

bool udp_install_listener(uint16_t dport, udp_listener listener);
void udp_uninstall_listener(uint16_t dport);

bool udp_send_packet(network_device *netdev, uint32_t sip, uint16_t sport, uint32_t dip, uint16_t dport, void *data, size_t data_size);
void udp_receive_packet(network_device *netdev, ip_header *ip_packet, udp_header *packet, void *data);

#endif