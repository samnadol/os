#ifndef NET_DHCP_H
#define NET_DHCP_H

#include <stdint.h>
#include "../l0/ethernet.h"

typedef struct __attribute__((packed, aligned(1))) dhcp_packet
{
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;

    // client IP address
    uint32_t ciaddr;
    // your IP address
    uint32_t yiaddr;
    // server IP address
    uint32_t siaddr;
    // gateway ip address
    uint32_t giaddr;

    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic_cookie;
    uint8_t options[312];
} dhcp_packet;

#define DHCP_BOOT_OP_REQUEST 0x01
#define DHCP_BOOT_OP_REPLY 0x02

#define DHCP_OP_DISCOVER 0x01
#define DHCP_OP_OFFER 0x02
#define DHCP_OP_REQUEST 0x03
#define DHCP_OP_DECLINE 0x04
#define DHCP_OP_ACK 0x05
#define DHCP_OP_NAK 0x06
#define DHCP_OP_RELEASE 0x07
#define DHCP_OP_INFORM 0x08

#define DHCP_HTYPE_ETHERNET 0x01

#define DHCP_OPTION_SUBNET_MASK 0x01
#define DHCP_OPTION_ROUTER 0x03
#define DHCP_OPTION_DNS_SERVER 0x06
#define DHCP_OPTION_HOST_NAME 0x0C
#define DHCP_OPTION_DOMAIN_NAME 0x0F
#define DHCP_OPTION_REQUESTED_IP 0x32
#define DHCP_OPTION_LEASE_TIME 0x33
#define DHCP_OPTION_MESSAGE_TYPE 0x35
#define DHCP_OPTION_SERVER_IDENTIFIER 0x36
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 0x37
#define DHCP_OPTION_CLIENT_IDENTIFIER 0x3D
#define DHCP_OPTION_END 0xFF

#define DHCP_TIMEOUT_MS 3000

void dhcp_init(network_device *netdev);

bool dhcp_configuration_release(network_device *netdev);
bool dhcp_configuration_request(network_device *netdev, uint32_t timeout);

#endif