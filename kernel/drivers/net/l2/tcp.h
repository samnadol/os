#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../l0/ethernet.h"
#include "../l1/ip.h"

#define TCP_RETRANSMISSION_MAX 5
#define TCP_RETRANSMISSION_DELAY 1000

typedef struct __attribute__((packed, aligned(1))) tcp_header
{
    uint16_t sport : 16;
    uint16_t dport : 16;
    uint32_t seqno : 32;
    uint32_t ackno : 32;
    uint8_t _ : 4;
    uint8_t data_offset : 4;
    union
    {
        struct
        {
            uint8_t fin : 1;
            uint8_t syn : 1;
            uint8_t rst : 1;
            uint8_t psh : 1;
            uint8_t ack : 1;
            uint8_t urg : 1;
            uint8_t ece : 1;
            uint8_t cwr : 1;
        };
        uint8_t raw;
    } flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
} tcp_header;

typedef bool (*tcp_listener)(network_device *, tcp_header *, void *, size_t);

typedef struct tcp_listener_node {
    uint16_t dport;
    tcp_listener listener;

    struct tcp_listener_node *next;
    struct tcp_listener_node *prev;
} tcp_listener_node;

void tcp_init();
void tcp_receive_packet(network_device *driver, ip_header *ip_packet, tcp_header *packet, void *data);

bool tcp_install_listener(uint16_t port, tcp_listener listener);
void tcp_uninstall_listener(uint16_t port);

bool tcp_connection_open(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, size_t timeout);
bool tcp_connection_transmit(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, void *data, size_t data_size, size_t timeout);
void tcp_connection_close(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, size_t timeout);