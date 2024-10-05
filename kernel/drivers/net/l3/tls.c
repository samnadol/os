#include "tls.h"

#include "../../../hw/mem.h"
#include "../../../lib/string.h"
#include "../../../lib/arpa/inet.h"
#include "../../../lib/random.h"
#include "../l2/tcp.h"

typedef struct __attribute__((packed))
{
    uint8_t type; // 0x16
    uint16_t version;
    uint16_t length;
} tls_main_header;

enum TLSHeaderTypes
{
    ChangeCipherSpec = 0x14,
    Alert = 0x15,
    Handshake = 0x16,
    ApplicationData = 0x17,
};

typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t length[3];
    uint8_t data[508];
} tls_handshake_header;

enum TLSHandshakePacketTypes
{
    HelloRequest = 0x00,
    ClientHello = 0x01,
    ServerHello = 0x02,
    Certificate = 0x0B,
    ServerKeyExchange = 0x0C,
    CertificateRequest = 0x0D,
    ServerHelloDone = 0x0E,
    CertificateVerify = 0x0F,
    ClientKeyExchange = 0x10,
    Finished = 0x14,
};

void sendTLSHandshake(network_device *netdev)
{
    tls_handshake_header *handshake = (tls_handshake_header *)calloc(sizeof(tls_handshake_header));
    handshake->type = ClientHello;
    handshake->length[0] = 0xF;
    handshake->length[1] = 0;
    handshake->length[2] = 0;
    handshake->data[0] = rand(UINT8_MAX);
    handshake->data[1] = rand(UINT8_MAX);
    handshake->data[2] = rand(UINT8_MAX);
    handshake->data[3] = rand(UINT8_MAX);

    tls_main_header *tls = (tls_main_header *)calloc(sizeof(tls_main_header) + sizeof(tls_handshake_header));
    tls->type = Handshake;
    tls->version = htons(0x0303);
    tls->length = htons(512);
    
    memcpy(tls + sizeof(tls_main_header), handshake, sizeof(tls_handshake_header));

    tcp_connection_open(netdev, netdev->ip_c.ip, ip_to_uint(1, 1, 1, 1), 45678, 443, 1000);
    tcp_connection_transmit(netdev, netdev->ip_c.ip, ip_to_uint(1, 1, 1, 1), 45678, 443, tls, sizeof(tls_main_header) + sizeof(tls_handshake_header), 1000);
}