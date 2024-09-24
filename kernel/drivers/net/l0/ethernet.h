#ifndef DRIVERS_NET_NET_H
#define DRIVERS_NET_NET_H

#include <stdbool.h>
#include "../../../hw/pci.h"
#include "../../../hw/cpu/irq.h"

#define NUM_ETHERNET_DEVICE 2
#define NUM_RX_DESC 32
#define NUM_TX_DESC 16

#define E1000_VEND 0x8086    // Vendor ID for Intel
#define E1000_E1000 0x100E   // Device ID for the e1000 Qemu, Bochs, and VirtualBox emmulated NICs
#define E1000_I217 0x153A    // Device ID for Intel I217
#define E1000_82577LM 0x10EA // Device ID for Intel 82577LM
#define E1000_82579LM 0x1502 // Device ID for Intel 82579LM

typedef struct __attribute__((packed, aligned(1))) ethernet_header
{
    uint8_t destination_mac[6];
    uint8_t source_mac[6];
    uint16_t ethertype;
} ethernet_header;

typedef struct __attribute__((packed, aligned(1))) ethernet_packet
{
    ethernet_header header;
    void *data;
} ethernet_packet;

typedef struct ip_conf
{
    uint32_t ip;            // ip address
    uint32_t netmask;       // netmask
    uint32_t gateway;       // gateway
    uint32_t dns;           // dns server ip
    uint32_t dhcp;          // dhcp server ip
    uint64_t lease_time;    // lease time of ip
} ip_conf;

typedef struct network_device
{
    pci_device *pci_device; // Physical PCI Device

    uint8_t mac[6]; // physical mac address

    ip_conf ip_c;

    bool eeprom_exists; // A flag indicating if EEPROM exists

    uint16_t rx_cur; // Current Receive Descriptor Buffer
    uint16_t tx_cur; // Current Transmit Descriptor Buffer

    void *rx_descs; // Receive Descriptor Buffer
    int rx_size;    // Recieve Descriptor Buffer Length
    void *tx_descs; // Transmit Descriptor Buffer
    int tx_size;    // Transmit Descriptor Buffer Length

    void (*int_enable)(struct network_device *dev);
    void (*int_disable)(struct network_device *dev);
    void (*int_handle)(struct network_device *dev, registers_t *r);

    uint8_t (*write)(struct network_device *driver, ethernet_packet *packet, size_t data_size);
} network_device;

enum EtherType
{
    ETHERTYPE_IP = 0x0800,
    ETHERTYPE_ARP = 0x0806,
};

void ethernet_device_init(pci_device *pci);
network_device *ethernet_first_netdev();

void ethernet_irq_enable();
void ethernet_irq_disable();

bool ethernet_send_packet(network_device *driver, uint8_t destination_mac[6], enum EtherType ethertype, void *data, size_t data_size);
void ethernet_receive_packet(network_device *driver, ethernet_packet *packet, size_t data_size);


#endif