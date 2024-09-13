#ifndef DNS_H
#define DNS_H

#include <stdint.h>
#include "../l0/ethernet.h"
#include "../../tty.h"

enum DNSMessage
{
    DNS_MESSAGE_QUERY = 0,
    DNS_MESSAGE_RESPONSE = 1,
};

#define DNS_RCODE_NO_ERROR 0b0000
#define DNS_RCODE_NO_SUCH_NAME 0b0011

enum DNSOpcode
{
    DNS_OPCODE_QUERY = 0,
    DNS_OPCODE_IQUERY = 1,
    DNS_OPCODE_STATUS = 2,
};

enum DNSType
{
    DNS_TYPE_A = 1,
    DNS_TYPE_NS = 2,
    DNS_TYPE_CNAME = 5,
    DNS_TYPE_SOA = 6,
    DNS_TYPE_PTR = 12,
    DNS_TYPE_MX = 15,
    DNS_TYPE_TXT = 16,
    DNS_TYPE_AAAA = 28,
    DNS_TYPE_SRV = 33,
    DNS_TYPE_OPT = 41,
    DNS_TYPE_ANY = 255,
};

enum DNSClass
{
    DNS_CLASS_IN = 1,
    DNS_CLASS_ANY = 255,
};

typedef struct __attribute__((packed, aligned(1))) dns_header
{
    uint16_t transaction_id;
    union
    {
        struct
        {
            uint8_t rcode : 4;
            uint8_t z : 3;
            uint8_t ra : 1;
            uint8_t rd : 1;
            uint8_t tc : 1;
            uint8_t aa : 1;
            uint8_t opcode : 4;
            uint8_t qr : 1;
        };
        uint16_t raw;
    } flags;
    uint16_t num_questions;
    uint16_t num_answers;
    uint16_t num_auth_rrs;
    uint16_t num_add_rrs;
} dns_header;

typedef struct __attribute__((packed, aligned(1))) dns_query
{
    uint16_t type;
    uint16_t class;
} dns_query;

#pragma pack(push, 1)
typedef struct dns_answer
{
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t len;

    uint8_t *data;
    char *domain;
    uint64_t created;
    struct dns_answer *next;
    struct dns_answer *prev;
} dns_answer;
#pragma pack(pop)

void dns_init();
void dns_print(tty_interface *tty);
dns_answer *dns_get_ip(network_device *netdev, uint32_t dns_server_ip, char *domain, size_t timeout);

#endif