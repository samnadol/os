#include "dhcp.h"

#include "../l2/udp.h"
#include "../l1/arp.h"
#include "../../tty.h"
#include "../../../hw/mem.h"
#include "../../../lib/arpa/inet.h"
#include "../../../lib/string.h"
#include "../../../hw/cpu/system.h"
#include "../../../lib/bits.h"
#include "../../../hw/timer.h"
#include "../../../lib/random.h"

typedef struct dhcp_offer
{
    uint32_t server_ip;
    uint32_t offered_ip;
} dhcp_offer;

#define DHCP_MAX_OFFERS 5
static dhcp_offer offers[DHCP_MAX_OFFERS];
static int offer_count = 0;

void dhcp_process_offer(network_device *netdev, dhcp_packet *packet)
{
    if (offer_count > DHCP_MAX_OFFERS)
        return;

    offers[offer_count].server_ip = ntohl(packet->siaddr);
    offers[offer_count].offered_ip = ntohl(packet->yiaddr);
    offer_count++;
}

void dhcp_process_ack(network_device *netdev, ip_header *ip, udp_header *udp, dhcp_packet *packet)
{
    netdev->ip_c.ip = ntohl(packet->yiaddr);
    netdev->ip_c.gateway = ntohl(packet->giaddr);
    netdev->ip_c.dhcp = ntohl(ip->sip);

    dprintf(3, "[DHCP] accepted IP %i from server %i\n", netdev->ip_c.ip, netdev->ip_c.dhcp);

    uint8_t *current_packet_opt = packet->options;
    while (current_packet_opt[0] != DHCP_OPTION_END)
    {
        switch (current_packet_opt[0])
        {
        case DHCP_OPTION_SUBNET_MASK:
            netdev->ip_c.netmask = (current_packet_opt[5] | current_packet_opt[4] << 8 | current_packet_opt[3] << 16 | current_packet_opt[2] << 24);
            break;
        case DHCP_OPTION_ROUTER:
            netdev->ip_c.gateway = (current_packet_opt[5] | current_packet_opt[4] << 8 | current_packet_opt[3] << 16 | current_packet_opt[2] << 24);
            break;
        // case DHCP_OPTION_DNS_SERVER:
        //     netdev->ip_c.dns = (current_packet_opt[5] | current_packet_opt[4] << 8 | current_packet_opt[3] << 16 | current_packet_opt[2] << 24);
        //     break;
        case DHCP_OPTION_LEASE_TIME:
            netdev->ip_c.lease_time = (current_packet_opt[5] | current_packet_opt[4] << 8 | current_packet_opt[3] << 16 | current_packet_opt[2] << 24);
            break;
        }
        current_packet_opt += current_packet_opt[1] + 2;
    }

    netdev->ip_c.dns = ip_to_uint(1, 1, 1, 1);

    // these will be used for any internet request but will probably expire (ttl 60s) before used and have to be re-requested, so no point in requesting them now
    // uint8_t mac[6];
    // arp_get_mac(netdev, netdev->ip_c.gateway, mac);
    // arp_get_mac(netdev, netdev->ip_c.dns, mac);

    udp_uninstall_listener(68);
}

void dhcp_udp_listener(network_device *netdev, ip_header *ip, udp_header *udp, void *data, size_t data_size)
{
    dprintf(3, "[DHCP] packet processing started\n");

    dhcp_packet *packet = (dhcp_packet *)data;
    switch (packet->op)
    {
    case DHCP_BOOT_OP_REPLY:
        uint8_t *current_packet_op = packet->options;
        while (current_packet_op[0] != DHCP_OPTION_END)
        {
            if (current_packet_op[0] == DHCP_OPTION_MESSAGE_TYPE)
            {
                // 0 = option number, 1 = option length, 2 - (2+option_length) = option data
                switch (current_packet_op[2])
                {
                case DHCP_OP_OFFER:
                    dprintf(3, "[DHCP] got offer\n");
                    dhcp_process_offer(netdev, packet);
                    return;
                case DHCP_OP_ACK:
                    dprintf(3, "[DHCP] got ack\n");
                    dhcp_process_ack(netdev, ip, udp, packet);
                    return;
                }
            }
            current_packet_op += current_packet_op[1] + 2;
        }
        break;
    default:
        dprintf(3, "[DHCP] invalid DHCP server reply\n");
        break;
    }
}

bool dhcp_send_discover(network_device *netdev, uint32_t xid)
{
    dhcp_packet *packet = (dhcp_packet *)calloc(sizeof(dhcp_packet));
    if (!packet)
        panic("calloc failed (dhcp_send_discover)");

    packet->op = DHCP_BOOT_OP_REQUEST;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = 6;
    packet->xid = htonl(xid);

    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = 0;
    packet->giaddr = 0;

    memcpy(&packet->chaddr, &netdev->mac, 6);
    memset(&packet->chaddr + (sizeof(uint8_t) * 6), 0, 10);

    packet->magic_cookie = htonl(0x63825363);

    size_t opt_num = 0;

    packet->options[opt_num++] = DHCP_OPTION_MESSAGE_TYPE;
    packet->options[opt_num++] = 1; // option length
    packet->options[opt_num++] = DHCP_OP_DISCOVER;

    packet->options[opt_num++] = DHCP_OPTION_CLIENT_IDENTIFIER;
    packet->options[opt_num++] = 7; // option length
    packet->options[opt_num++] = DHCP_HTYPE_ETHERNET;
    packet->options[opt_num++] = netdev->mac[0];
    packet->options[opt_num++] = netdev->mac[1];
    packet->options[opt_num++] = netdev->mac[2];
    packet->options[opt_num++] = netdev->mac[3];
    packet->options[opt_num++] = netdev->mac[4];
    packet->options[opt_num++] = netdev->mac[5];

    packet->options[opt_num++] = DHCP_OPTION_END;

    dprintf(3, "[DHCP] sending discover\n");
    bool res = udp_send_packet(netdev, 0, 68, 0xFFFFFFFF, 67, packet, sizeof(dhcp_packet) - (312 - opt_num));
    mfree(packet);
    return res;
}

bool dhcp_send_request(network_device *netdev, uint32_t server_ip, uint32_t requested_ip, uint32_t xid)
{
    dhcp_packet *packet = (dhcp_packet *)calloc(sizeof(dhcp_packet));
    if (!packet)
        panic("calloc failed (dhcp_send_request)");

    dprintf(3, "[DHCP] sent request\n");

    packet->op = DHCP_BOOT_OP_REQUEST;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = 6;
    packet->hops = 0;
    packet->xid = htonl(xid);
    packet->secs = htons(0);
    packet->flags = htons(0);

    packet->ciaddr = 0;
    packet->yiaddr = 0;
    packet->siaddr = htonl(server_ip);
    packet->giaddr = 0;

    memcpy(&packet->chaddr, &netdev->mac, 6);
    memset(&packet->chaddr[6], 0, 10);
    memset(&packet->sname, 0, 64);
    memset(&packet->file, 0, 128);

    packet->magic_cookie = htonl(0x63825363);

    int opt = 0;
    packet->options[opt++] = DHCP_OPTION_MESSAGE_TYPE;
    packet->options[opt++] = 1;
    packet->options[opt++] = DHCP_OP_REQUEST;
    packet->options[opt++] = DHCP_OPTION_REQUESTED_IP;
    packet->options[opt++] = 4;
    packet->options[opt++] = GET_BYTE(htonl(requested_ip), 0x0);
    packet->options[opt++] = GET_BYTE(htonl(requested_ip), 0x8);
    packet->options[opt++] = GET_BYTE(htonl(requested_ip), 0x10);
    packet->options[opt++] = GET_BYTE(htonl(requested_ip), 0x18);
    packet->options[opt++] = DHCP_OPTION_SERVER_IDENTIFIER;
    packet->options[opt++] = 4;
    packet->options[opt++] = GET_BYTE(htonl(server_ip), 0x0);
    packet->options[opt++] = GET_BYTE(htonl(server_ip), 0x8);
    packet->options[opt++] = GET_BYTE(htonl(server_ip), 0x10);
    packet->options[opt++] = GET_BYTE(htonl(server_ip), 0x18);
    packet->options[opt++] = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    packet->options[opt++] = 4;
    packet->options[opt++] = DHCP_OPTION_SUBNET_MASK;
    packet->options[opt++] = DHCP_OPTION_ROUTER;
    packet->options[opt++] = DHCP_OPTION_DOMAIN_NAME;
    packet->options[opt++] = DHCP_OPTION_DNS_SERVER;
    packet->options[opt++] = DHCP_OPTION_END;

    bool res = udp_send_packet(netdev, 0x00000000, 68, 0xFFFFFFFF, 67, packet, sizeof(dhcp_packet) - (312 - opt));
    mfree(packet);
    return res;
}

bool dhcp_configuration_release(network_device *netdev)
{
    if (!netdev->ip_c.ip)
        return false;

    dhcp_packet *packet = (dhcp_packet *)calloc(sizeof(dhcp_packet));
    if (!packet)
        panic("calloc failed (dhcp_release_configuration)");

    packet->op = DHCP_BOOT_OP_REQUEST;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = 6;
    packet->xid = htonl(0x12345678);

    packet->ciaddr = htonl(netdev->ip_c.ip);
    packet->yiaddr = 0;
    packet->siaddr = htonl(netdev->ip_c.dhcp);
    packet->giaddr = 0;

    int opt = 0;
    packet->options[opt++] = DHCP_OPTION_MESSAGE_TYPE;
    packet->options[opt++] = 1;
    packet->options[opt++] = DHCP_OP_RELEASE;
    packet->options[opt++] = DHCP_OPTION_END;

    packet->magic_cookie = htonl(0x63825363);

    memcpy(&packet->chaddr, &netdev->mac, 6);
    memset(&packet->chaddr[6], 0, 10);
    memset(&packet->sname, 0, 64);
    memset(&packet->file, 0, 128);

    dprintf(3, "[DHCP] sending release\n");
    bool res = udp_send_packet(netdev, netdev->ip_c.ip, 68, netdev->ip_c.dhcp, 67, packet, sizeof(dhcp_packet) - (312 - opt));
    mfree(packet);

    memset(&(netdev->ip_c), 0, sizeof(ip_conf));

    return res;
}

bool dhcp_configuration_request(network_device *netdev, uint32_t timeout)
{
    if (netdev->ip_c.ip)
        return false;

    offer_count = 0;

    uint32_t xid = rand(UINT32_MAX);

    udp_install_listener(68, dhcp_udp_listener);
    dhcp_send_discover(netdev, xid);

    uint32_t ms_waited = 0;
    while (offer_count < 1)
    {
        timer_wait(10);
        ms_waited += 10;

        if (ms_waited > timeout)
            break;
    }

    for (int i = 0; i < offer_count; i++)
    {
        uint8_t mac[6];
        if (arp_get_mac(netdev, offers[i].offered_ip, mac))
        {
            dprintf(3, "[DHCP] offer for %i (currently in use)\n", offers[i].offered_ip);
            continue;
        }
        else
        {
            dprintf(3, "[DHCP] offer for %i (not currently in use)\n", offers[i].offered_ip);
            dhcp_send_request(netdev, offers[i].server_ip, offers[i].offered_ip, xid);
            return true;
        }
    }

    udp_uninstall_listener(68);
    return false;
}

void dhcp_init(network_device *netdev)
{
    dhcp_configuration_request(netdev, 3000);
}