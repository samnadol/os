#pragma once

#include <stdint.h>
#include "../../tty.h"
#include "../l0/ethernet.h"

#define ARP_HW_ETHERNET 0x1
#define ARP_PROTOCOL_IP 0x800

#define ARP_OPCODE_REQUEST 0x1
#define ARP_OPCODE_REPLY 0x2

#define ARP_TIMEOUT_MS 100

typedef struct __attribute__((packed, aligned(1))) arp_packet
{
    uint16_t hardware_type; // hardware type (always ethernet, 0x1)
    uint16_t protocol_type; // protocol type (always ip, 0x800)

    uint8_t hwsize;  // size of hardware address (mac) in bytes
    uint8_t prosize; // size of protocol address (ip) in bytes

    uint16_t opcode; // ARP opcode

    uint8_t smac[6]; // source mac address
    uint32_t sip;    // source ip address

    uint8_t dmac[6]; // sestination mac address
    uint32_t dip;    // sestination ip address
} arp_packet;

void arp_init();
void arp_print(tty_interface *tty);

bool arp_get_mac(network_device *netdev, uint32_t ip, uint8_t mac[6]);

bool arp_send_request(network_device *netdev, uint32_t ip);
void arp_receive_packet(network_device *netdev, arp_packet *packet);