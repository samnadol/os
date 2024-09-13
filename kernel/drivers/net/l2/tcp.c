#include "tcp.h"

#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../l3/dns.h"
#include "../../../lib/arpa/inet.h"
#include "../../../lib/string.h"
#include "../../../lib/random.h"

typedef struct tcp_cache_entry
{
    uint16_t port;
    bool syn;
    uint32_t seq;
    uint32_t ack;

    uint32_t seqbase;
    uint32_t ackbase;
    bool closing;

    struct tcp_cache_entry *next;
    struct tcp_cache_entry *prev;
} tcp_cache_entry;
tcp_cache_entry *tcp_cache;

tcp_listener *tcp_port_listeners;
tcp_datapart *tcp_dataparts;

tcp_cache_entry *tcp_get_cache(uint16_t port)
{
    tcp_cache_entry *current = tcp_cache;
    while (current)
    {
        if (current->port == port)
            return current;
        current = current->next;
    }

    return 0;
}

void tcp_remove_cache(uint16_t port)
{
    tcp_cache_entry *ce = tcp_get_cache(port);

    if (ce == tcp_cache) // if first entry, then set first entry to next entry
        tcp_cache = ce->next;
    else // if not first entry, then set previous entry's next to next
        ce->prev->next = ce->next;
    ce->next->prev = ce->prev;

    mfree(ce);
}

void tcp_add_datapart(uint32_t seqno, void *data, size_t data_length)
{
    tcp_datapart *c1 = tcp_dataparts;
    while (c1)
        if (c1->seqno != seqno)
            c1 = c1->next;
        else
            break;

    if (c1 && c1->seqno == seqno)
    {
        size_t newsize = c1->data_length + data_length;
        void *newdata = calloc(newsize);
        c1->num_parts++;

        memcpy(newdata, c1->data, c1->data_length);
        memcpy(newdata + c1->data_length - 1, data, data_length);
        mfree(c1->data);

        c1->data = newdata;
        c1->data_length = newsize;
    }
    else
    {
        tcp_datapart *new = (tcp_datapart *)calloc(sizeof(tcp_datapart));
        new->seqno = seqno;
        new->next = 0;
        new->num_parts = 1;

        data_length += 1; // add room for null termination
        new->data = calloc(data_length);
        new->data_length = data_length;
        memcpy(new->data, data, data_length);

        if (!tcp_dataparts)
        {
            tcp_dataparts = new;
        }
        else
        {
            tcp_datapart *c2 = tcp_dataparts;
            while (c2->next)
                c2 = c2->next;

            new->prev = c2;
            c2->next = new;
        }
    }
}

tcp_datapart *tcp_get_datapart(uint32_t seqno)
{
    tcp_datapart *c1 = tcp_dataparts;
    while (c1)
        if (c1->seqno != seqno)
            c1 = c1->next;
        else
            break;

    if (c1 && c1->seqno == seqno)
        return c1;
    else
        return 0;
}

void tcp_delete_datapart(uint32_t seqno)
{
    tcp_datapart *c1 = tcp_dataparts;
    while (c1)
        if (c1->seqno != seqno)
            c1 = c1->next;
        else
            break;

    if (c1 && c1->seqno == seqno)
    {
        if (c1 == tcp_dataparts)
        {
            // first
            tcp_dataparts = c1->next;
            c1->next->prev = 0;
            mfree(c1);
        }
        else
        {
            // not first
            c1->prev->next = c1->next;
            c1->next->prev = c1->prev;
            mfree(c1);
        }
    }
}

uint16_t tcp_calculate_checksum(tcp_header *header, uint32_t sip, uint32_t dip, void *payload, size_t payload_size)
{
    uint32_t checksum = 0;

    ip_pseudo_header *pseudo_header = (ip_pseudo_header *)calloc(sizeof(ip_pseudo_header));
    if (!pseudo_header)
        panic("calloc failed (tcp_calculate_checksum)");
    pseudo_header->sip = sip;
    pseudo_header->dip = dip;
    pseudo_header->proto = IP_PROTOCOL_TCP;
    pseudo_header->len = htons(payload_size + sizeof(tcp_header));

    uint8_t *current = (uint8_t *)pseudo_header;
    for (size_t i = 0; i < sizeof(ip_pseudo_header); i += 2)
        checksum += (current[i + 1] << 8) | current[i];
    mfree(pseudo_header);

    uint32_t old_checksum = header->checksum;
    header->checksum = 0;
    current = (uint8_t *)header;
    for (size_t i = 0; i < sizeof(tcp_header); i += 2)
        checksum += (current[i + 1] << 8) | current[i];
    header->checksum = old_checksum;

    current = (uint8_t *)payload;
    for (size_t i = 0; i < payload_size / 2; i++)
        checksum += (current[(i * 2) + 1] << 8) | current[i * 2];
    if (payload_size % 2 != 0)
        checksum += current[payload_size - 1];

    // add first and second words of checksum together (0x12345678 -> (0x1234 + 0x5678) -> 0x68AC)
    checksum = (checksum >> 16) + (checksum & 0xFFFF);

    return (uint16_t)~checksum;
}

bool tcp_send_packet(network_device *netdev, uint32_t sip, uint16_t sport, uint32_t dip, uint16_t dport, uint32_t seqno, uint32_t ackno, bool syn, bool ack, bool fin, bool psh, void *data, size_t data_size)
{
    dprintf(3, "[TCP] sending packet to port %d (ip %i)\n", dport, dip);

    tcp_header *header = (tcp_header *)calloc(sizeof(tcp_header));
    header->sport = htons(sport);
    header->dport = htons(dport);
    header->seqno = htonl(seqno);
    header->ackno = htonl(ackno);
    header->data_offset = sizeof(tcp_header) / sizeof(uint32_t);
    header->_ = 0;

    header->flags.syn = syn;
    header->flags.ack = ack;
    header->flags.fin = fin;
    header->flags.psh = psh;

    header->window_size = htons(32768);
    header->checksum = tcp_calculate_checksum(header, htonl(sip), htonl(dip), data, data_size);
    header->urgent_pointer = 0;

    bool res = ip_send_packet(netdev, sip, dip, IP_PROTOCOL_TCP, sizeof(tcp_header), header, data_size, data);
    mfree(header);
    return res;
}

void tcp_receive_packet(network_device *netdev, ip_header *ip, tcp_header *tcp, void *headerdata_payload)
{
    size_t payload_len = ntohs(ip->total_length) - sizeof(ip_header) - sizeof(tcp_header);
    size_t headerdata_len = ntohs(ip->total_length) - sizeof(ip_header) - (tcp->data_offset * sizeof(uint32_t)) - payload_len;

    void *payload = headerdata_payload + headerdata_len;
    // void *headerdata = headerdata_payload;

    uint16_t checksum = tcp->checksum;
    tcp->checksum = 0;
    if (checksum != tcp_calculate_checksum(tcp, ip->sip, ip->dip, headerdata_payload, payload_len))
    {
        printf("[TCP] received, checksum invalid (expected %x, actual %x)\n", tcp_calculate_checksum(tcp, ip->sip, ip->dip, headerdata_payload, payload_len), checksum);
        return;
    }
    tcp->checksum = checksum;

    tcp->sport = ntohs(tcp->sport);
    tcp->dport = ntohs(tcp->dport);

    dprintf(3, "[TCP] got packet for port %d\n", tcp->dport);

    if (tcp->flags.syn && tcp->flags.ack) // SYN ACK
    {
        tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
        if (ce)
        {
            ce->syn = true;
            ce->seq = ntohl(tcp->ackno);
            ce->ack = ntohl(tcp->seqno) + 1;
            ce->ackbase = ntohl(tcp->seqno);

            // printf("[TCP] SYN ACK seq %d (%d), ack %d (%d)\n", ntohl(tcp->seqno) - tcp_cache[i].ackbase, ntohl(tcp->seqno), ntohl(tcp->ackno) - tcp_cache[i].seqbase, ntohl(tcp->ackno));
            tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ntohl(tcp->ackno), ntohl(tcp->seqno) + 1, false, true, false, false, NULL, 0);
        }
        return;
    }

    if (tcp->flags.fin && tcp->flags.ack) // FIN
    {
        tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
        if (ce)
        {
            ce->syn = false;
            ce->ack = 0;

            if (ce->closing)
            {
                dprintf(3, "[TCP] connection closed (client initiated)\n");
                tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ntohl(tcp->ackno), ntohl(tcp->seqno) + 1, false, true, false, false, NULL, 0);
                tcp_remove_cache(tcp->dport);
            }
            else
            {
                dprintf(3, "[TCP] got FIN (server initiated)\n");
                tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ntohl(tcp->ackno), ntohl(tcp->seqno) + 1, false, true, true, false, NULL, 0);
            }
        }
        return;
    }

    if (tcp->flags.psh) // DATA END
    {
        if (tcp_port_listeners[tcp->dport])
        {
            tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
            if (ce)
            {
                ce->seq = ntohl(tcp->ackno);
                ce->ack = ntohl(tcp->seqno) + payload_len;

                if (payload_len > 0)
                    tcp_add_datapart(ce->seq, payload, payload_len);
                // printf("[TCP] PSH seq %d, ack %d, len %d\n", ntohl(tcp->seqno), ntohl(tcp->ackno), payload_len);

                tcp_datapart *complete_data = tcp_get_datapart(ce->seq);
                dprintf(3, "[TCP] finished data packet, len %d, %d parts\n", complete_data->data_length, complete_data->num_parts);
                tcp_listener listener = tcp_port_listeners[tcp->dport];
                listener(netdev, tcp, complete_data->data, complete_data->data_length);
                tcp_delete_datapart(ce->seq);

                tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ce->seq, ce->ack, false, true, false, false, NULL, 0);
            }
        }
        return;
    }

    if (tcp->flags.ack) // ACK / DATA PART
    {
        tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
        if (ce)
        {
            if (ce->syn)
            {
                ce->ack = ntohl(tcp->seqno);
                ce->seq = ntohl(tcp->ackno);

                if (payload_len > 0)
                    tcp_add_datapart(ce->seq, payload, payload_len);
                // printf("[TCP] ACK seq %d, ack %d, len %d\n", ntohl(tcp->seqno), ntohl(tcp->ackno), payload_len);
            }
            else
            {
                dprintf(3, "[TCP] connection closed (server initiated)\n");
                tcp_remove_cache(tcp->dport);
            }
        }
    }
}

bool tcp_connection_transmit(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, void *data, size_t data_size, size_t timeout)
{
    tcp_cache_entry *ce = tcp_get_cache(sport);
    if (!ce)
        return false;

    uint32_t exp_seq = ce->seq + data_size;
    if (!tcp_send_packet(netdev, sip, sport, dip, dport, ce->seq, ce->ack, false, true, false, true, data, data_size))
        return false;

    size_t retransmissions = 0;
    for (int t = 0; t < timeout; t += 10)
    {
        timer_wait(10);

        if (ce->seq == exp_seq)
            return true;

        if (t % (TCP_RETRANSMISSION_DELAY / 10)) // retranmission delay
        {
            if (!tcp_send_packet(netdev, sip, sport, dip, dport, ce->seq, ce->ack, false, true, false, true, data, data_size))
                return false;
            retransmissions += 1;
        }

        if (retransmissions > TCP_RETRANSMISSION_MAX)
            break;
    }

    return false;
}

bool tcp_connection_open(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, size_t timeout)
{
    if (tcp_get_cache(sport))
    {
        printf("[TCP] port %d is already in use\n", sport);
        return false;
    }

    tcp_cache_entry *ce = (tcp_cache_entry *)calloc(sizeof(tcp_cache_entry));
    ce->port = sport;
    ce->syn = false;
    ce->seq = rand(32768);
    ce->seqbase = ce->seq;
    ce->ack = 0;
    ce->ackbase = 0;
    ce->closing = false;
    ce->prev = 0;
    ce->next = 0;

    if (!tcp_cache)
    {
        tcp_cache = ce;
    }
    else
    {
        tcp_cache_entry *current = tcp_cache;
        while (current->next)
            current = current->next;
        current->next = ce;
        ce->prev = current;
    }

    if (!tcp_send_packet(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, sport, dip, dport, ce->seq, ce->ack, true, false, false, false, NULL, 0))
        return false;

    for (int t = 0; t < timeout; t += 10)
    {
        timer_wait(10);

        if (ce->syn)
        {
            dprintf(3, "[TCP] connection established\n");
            return true;
        }
    }

    printf("[TCP] connection timed out\n");
    return false;
}

void tcp_connection_close(network_device *netdev, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, size_t timeout)
{
    tcp_cache_entry *ce = tcp_get_cache(sport);
    tcp_send_packet(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, sport, dip, dport, ce->seq, ce->ack, false, true, true, false, NULL, 0);
    ce->closing = true;
}

bool tcp_install_listener(uint16_t port, tcp_listener listener)
{
    if (tcp_port_listeners[port])
    {
        return false;
    }

    tcp_port_listeners[port] = listener;
    return true;
}

void tcp_uninstall_listener(uint16_t port)
{
    tcp_port_listeners[port] = NULL;
}

void tcp_init()
{
    dprintf(3, "[TCP] init, calloc %f\n", 65536 * sizeof(tcp_listener));
    tcp_port_listeners = (tcp_listener *)calloc(65536 * sizeof(tcp_listener));
}