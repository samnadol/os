#include "tcp.h"

#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../l3/dns.h"
#include "../../../lib/arpa/inet.h"
#include "../../../lib/string.h"
#include "../../../lib/random.h"

// tcp connection cache
typedef struct tcp_cache_entry
{
    uint16_t port;
    uint8_t state;
    uint32_t seq;
    uint32_t ack;

    uint32_t seqbase;
    uint32_t ackbase;
    uint32_t expseq;
    bool closing;

    struct tcp_cache_entry *next;
    struct tcp_cache_entry *prev;
} tcp_cache_entry;
tcp_cache_entry *tcp_cache;

// tcp multipart reconstruction cache
typedef struct tcp_datapart
{
    uint32_t seqno;
    uint8_t num_parts;

    void *data;
    size_t data_length;

    struct tcp_datapart *next;
    struct tcp_datapart *prev;
} tcp_datapart;
tcp_datapart *tcp_dataparts;

// tcp port listener cache
tcp_listener_node *tcp_port_listeners;

tcp_listener_node *get_tcp_listener(uint16_t dport)
{
    tcp_listener_node *current = tcp_port_listeners;
    while (current)
    {
        if (current->dport == dport)
            return current;
        current = current->next;
    }
    return 0;
}

void set_tcp_listener(uint16_t dport, tcp_listener listener)
{
    tcp_listener_node *llnode = (tcp_listener_node *)calloc(sizeof(tcp_listener_node));
    llnode->dport = dport;
    llnode->listener = listener;

    tcp_listener_node *current = tcp_port_listeners;

    if (!current)
    {
        tcp_port_listeners = llnode;
    }
    else
    {
        while (current->next)
            current = current->next;
        current->next = llnode;
        llnode->prev = current;
    }
}

void del_tcp_listener(uint16_t dport)
{
    tcp_listener_node *todel = get_tcp_listener(dport);
    if (!(todel->next) && !(todel->prev))
        tcp_port_listeners = 0;
    else
    {
        todel->next->prev = todel->prev;
        todel->prev->next = todel->next;
    }
    mfree(todel);
}

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
    // printf("= TCPN sending packet to port %d (ip %i), size %d\n", dport, dip, sizeof(ip_header) + data_size + sizeof(tcp_header) + sizeof(ethernet_header));

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

void tcp_assemble_data(network_device *netdev, ip_header *ip, tcp_header *tcp)
{
    if (get_tcp_listener(tcp->dport))
    {
        tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
        if (ce)
        {
            tcp_datapart *complete_data = tcp_get_datapart(ce->seq - 1);
            if (complete_data)
            {
                // printf("= TCPN finished data packet, len %d, %d parts\n", complete_data->data_length, complete_data->num_parts);
                tcp_listener listener = get_tcp_listener(tcp->dport)->listener;
                listener(netdev, tcp, complete_data->data, complete_data->data_length);
                tcp_delete_datapart(ce->seq);
            }
            else
            {
                // printf("= TCPN datapart missing for seq %d\n", ce->seq);
            }
        }
    }
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
        // printf("[TCP] received, checksum invalid (expected %x, actual %x)\n", tcp_calculate_checksum(tcp, ip->sip, ip->dip, headerdata_payload, payload_len), checksum);
        return;
    }
    tcp->checksum = checksum;

    tcp->sport = ntohs(tcp->sport);
    tcp->dport = ntohs(tcp->dport);

    tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
    if (ce)
    {
        if (tcp->flags.syn && tcp->flags.ack)
        {
            tcp_cache_entry *ce = tcp_get_cache(tcp->dport);
            ce->state = 1;
            ce->ackbase = ntohl(tcp->seqno);

            ce->seq = ntohl(tcp->ackno);
            ce->ack = ntohl(tcp->seqno) + 1;

            // printf("= TCPN SYN ACK seq %d (%d), ack %d (%d)\n", ntohl(tcp->seqno) - ce->ackbase, ntohl(tcp->seqno), ntohl(tcp->ackno) - ce->seqbase, ntohl(tcp->ackno));
            tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ntohl(tcp->ackno), ntohl(tcp->seqno) + 1, false, true, false, false, NULL, 0);
            return;
        }

        if (tcp->flags.ack)
        {
            ce->seq = ntohl(tcp->ackno);
            ce->ack = ntohl(tcp->seqno) + payload_len;

            if (payload_len > 0)
            {
                if (ce->state == 1)
                {
                    // printf("= TCPN ACK (data)\n");
                    tcp_add_datapart(ce->seq, payload, payload_len);
                    tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ce->seq, ce->ack, false, true, false, false, NULL, 0);
                }
            }
            else
            {
                if (ce->state == 1 && !tcp->flags.fin)
                {
                    // printf("= TCPN ACK (no data)\n");
                }

                if (ce->state == 2)
                {
                    // printf("= TCPN ACK (to FIN, connection closed)\n");
                    tcp_assemble_data(netdev, ip, tcp);
                    tcp_remove_cache(tcp->dport);
                }
            }
        }

        if (tcp->flags.fin)
        {
            if (ce->state == 1)
            {
                // printf("= TCPN FIN (new FIN, from server)\n");
                ce->state = 2;
                tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ce->seq, ce->ack + 1, false, true, true, false, NULL, 0);
            }
            else
            {
                // printf("= TCPN FIN (server's response to client FIN) %d\n", ce->state);
                tcp_send_packet(netdev, netdev->ip_c.ip, tcp->dport, ntohl(ip->sip), tcp->sport, ce->seq, ce->ack, false, true, false, false, NULL, 0);
                tcp_assemble_data(netdev, ip, tcp);
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

    ce->expseq = ce->seq + data_size;
    if (!tcp_send_packet(netdev, sip, sport, dip, dport, ce->seq, ce->ack, false, true, false, true, data, data_size))
        return false;

    size_t retransmissions = 0;
    for (int t = 0; t < timeout; t += 10)
    {
        timer_wait(10);

        if (ce->seq == ce->expseq)
            return true;

        if (t % (TCP_RETRANSMISSION_DELAY / 10)) // retranmission delay
        {
            // if (!tcp_send_packet(netdev, sip, sport, dip, dport, ce->seq, ce->ack, false, true, false, true, data, data_size))
            //     return false;
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
    ce->state = 0;
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

        if (ce->state > 0)
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
    if (get_tcp_listener(port))
        return false;

    set_tcp_listener(port, listener);
    return true;
}

void tcp_uninstall_listener(uint16_t port)
{
    del_tcp_listener(port);
}

void tcp_init()
{
    dprintf(3, "[TCP] init");
    // tcp_port_listeners = (tcp_listener *)calloc(65536 * sizeof(tcp_listener));
}