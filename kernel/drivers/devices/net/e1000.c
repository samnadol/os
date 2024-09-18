#include "./e1000.h"

#include "../../../hw/cpu/interrupts.h"
#include "../../../hw/mmio.h"
#include "../../../hw/timer.h"
#include "../../../lib/bits.h"
#include "../../net/l2/udp.h"
#include "../../net/l1/ip.h"
#include "../../tty.h"
#include "../../../hw/mem.h"
#include "../../../hw/port.h"

inline void e1000_write32(network_device *dev, uint16_t p_address, uint32_t p_value)
{
    if (dev->pci_device->bar0_type == 0)
    {
        mmio_write32((uint64_t *)(uintptr_t)(dev->pci_device->bar[0] + p_address), p_value);
    }
    else
    {
        outl(dev->pci_device->bar[1], p_address);
        outl(dev->pci_device->bar[1] + 4, p_value);
    }
}

inline uint32_t e1000_read32(network_device *dev, uint16_t p_address)
{
    if (dev->pci_device->bar0_type == 0)
    {
        return mmio_read32((uint64_t *)(uintptr_t)(dev->pci_device->bar[0] + p_address));
    }
    else
    {
        outl(dev->pci_device->bar[1], p_address);
        return inl(dev->pci_device->bar[1] + 4);
    }
}

void e1000_int_enable(network_device *netdev)
{
    dprintf(1, "[e1000] Enabling interrupts\n");

    e1000_write32(
        netdev, E1000_REG_IMS,
        (E1000_REGBIT_IMS_TXDW | E1000_REGBIT_ICR_TXQE | E1000_REGBIT_ICR_LSC |
         E1000_REGBIT_ICR_RXSEQ | E1000_REGBIT_ICR_RXDMT0 | E1000_REGBIT_ICR_RXO |
         E1000_REGBIT_ICR_RXT0 | E1000_REGBIT_IMS_MDAC | E1000_REGBIT_IMS_RXCFG |
         E1000_REGBIT_IMS_PHYINT | E1000_REGBIT_IMS_GPI |
         E1000_REGBIT_IMS_TXD_LOW | E1000_REGBIT_IMS_SRPD));

    e1000_read32(netdev, E1000_REG_ICR);
}

void e1000_int_disable(network_device *netdev)
{
    dprintf(1, "[e1000] Disabling interrupts\n");

    e1000_write32(netdev, E1000_REG_IMC, 0xFFFFFFFF);
    e1000_write32(netdev, E1000_REG_ICR, 0xFFFFFFFF);
    e1000_read32(netdev, E1000_REG_STATUS);
}

bool e1000_detectEEPROM(network_device *dev)
{
    e1000_write32(dev, E1000_REG_EERD, 0x1);

    for (int i = 0; i < 1000 && !dev->eeprom_exists; i++)
        dev->eeprom_exists = e1000_read32(dev, E1000_REG_EERD) & E1000_REGBIT_EERD_DONE;

    return dev->eeprom_exists;
}

uint32_t e1000_read_eeprom(network_device *dev, uint8_t addr)
{
    uint16_t data = 0;
    uint32_t tmp = 0;

    if (dev->eeprom_exists)
    {
        e1000_write32(dev, E1000_REG_EERD, (1) | ((uint32_t)(addr) << 8));
        while (!((tmp = e1000_read32(dev, E1000_REG_EERD)) & (1 << 4)))
            ;
    }
    else
    {
        e1000_write32(dev, E1000_REG_EERD, (1) | ((uint32_t)(addr) << 2));
        while (!((tmp = e1000_read32(dev, E1000_REG_EERD)) & (1 << 1)))
            ;
    }

    data = (uint16_t)((tmp >> 16) & 0xFFFF);
    return data;
}

bool e1000_read_mac(network_device *dev)
{
    if (dev->eeprom_exists)
    {
        uint32_t temp;
        temp = e1000_read_eeprom(dev, 0x0);
        dev->mac[0] = temp & 0xFF;
        dev->mac[1] = temp >> 0x8;
        temp = e1000_read_eeprom(dev, 0x1);
        dev->mac[2] = temp & 0xFF;
        dev->mac[3] = temp >> 0x8;
        temp = e1000_read_eeprom(dev, 0x2);
        dev->mac[4] = temp & 0xFF;
        dev->mac[5] = temp >> 0x8;
    }
    else
    {
        if ((uint32_t *)(uintptr_t)(dev->pci_device->bar[1] + 0x5400) != 0)
        {
            // mac address memory is not 0
            for (int i = 0; i < 6; i++)
            {
                dev->mac[i] =
                    *((uint8_t *)(uintptr_t)(dev->pci_device->bar[1] + 0x5400 + i));
            }
        }
        else
            return false; // no mac address
    }
    return true;
}

void e1000_rx_init(network_device *dev)
{
    dprintf(1, "[e1000] rx init, calloc %f\n", (sizeof(e1000_rx_desc) * NUM_RX_DESC + 16) + (NUM_RX_DESC * (8192 + 16)));

    for (int i = 0; i < 128; i++)
    {
        e1000_write32(dev, E1000_REG_MTA + (i * 4), 0);
    }
    e1000_read32(dev, E1000_REG_MTA);

    e1000_rx_desc *rx_buf = (e1000_rx_desc *)(calloc_align(sizeof(e1000_rx_desc) * NUM_RX_DESC + 16, 16));
    if (!rx_buf)
        panic("calloc failed (e1000_rx_init 1)");

    for (int i = 0; i < NUM_RX_DESC; i++)
    {
        void *alloc = calloc(8192 + 16);
        if (!alloc)
            panic("calloc failed (e1000_rx_init 2)");

        rx_buf[i].addr = (uint64_t)(uintptr_t)(uint8_t *)alloc;
        rx_buf[i].status = 0;
    }
    dev->rx_descs = (uint8_t *)rx_buf;
    dev->rx_size = NUM_RX_DESC * sizeof(e1000_rx_desc);

    e1000_write32(dev, E1000_REG_RDBAL, (uint32_t)((uint64_t)(uintptr_t)rx_buf));
    e1000_write32(dev, E1000_REG_RDBAH, (uint32_t)((uint64_t)(uintptr_t)rx_buf >> 32));

    e1000_write32(dev, E1000_REG_RDLEN, dev->rx_size);

    e1000_write32(dev, E1000_REG_RDH, 0);
    e1000_write32(dev, E1000_REG_RDT, NUM_RX_DESC - 1);
    dev->rx_cur = 0;

    e1000_write32(dev, E1000_REG_RCTL,
                  E1000_REGBIT_RCTL_EN | E1000_REGBIT_RCTL_SBP |
                      E1000_REGBIT_RCTL_UPE | E1000_REGBIT_RCTL_MPE |
                      E1000_REGBIT_RCTL_LPE | E1000_REGBIT_RCTL_BAM |
                      E1000_REGBIT_RCTL_BSIZE_8192 | E1000_REGBIT_RCTL_BSEX |
                      E1000_REGBIT_RCTL_SECRC | E1000_REGBIT_RCTL_RDMTS_1_2);
}

void e1000_tx_init(network_device *dev)
{
    dprintf(1, "[e1000] tx init, calloc %f\n", sizeof(e1000_tx_desc) * NUM_TX_DESC + 16);

    e1000_tx_desc *tx_buf = (e1000_tx_desc *)(calloc_align(sizeof(e1000_tx_desc) * NUM_TX_DESC + 16, 16));
    if (!tx_buf)
        panic("calloc failed (e1000_tx_init)");

    for (int i = 0; i < NUM_TX_DESC; i++)
    {
        tx_buf[i].addr = 0;
        tx_buf[i].cmd = 0;
        tx_buf[i].status = E1000_REGBIT_TXD_STAT_DD;
    }
    dev->tx_descs = (uint8_t *)tx_buf;
    dev->tx_size = NUM_TX_DESC * sizeof(e1000_tx_desc);

    e1000_write32(dev, E1000_REG_TDBAL, (uint32_t)(uintptr_t)tx_buf);
    // e1000_write32(dev, E1000_REG_TDBAH, (uint32_t)((uint64_t)(uintptr_t)tx_buf >> 32));
    e1000_write32(dev, E1000_REG_TDBAH, 0);

    e1000_write32(dev, E1000_REG_TDLEN, dev->tx_size);

    e1000_write32(dev, E1000_REG_TDH, 0);
    e1000_write32(dev, E1000_REG_TDT, 0);
    dev->tx_cur = 0;

    e1000_write32(dev, E1000_REG_TCTL, E1000_REGBIT_TCTL_EN | E1000_REGBIT_TCTL_PSP | E1000_REGBIT_TCTL_CT_15 | E1000_REGBIT_TCTL_COLD_FULL | E1000_REGBIT_TCTL_RTLC);
    e1000_write32(dev, E1000_REG_TIPG, (10 << 0 | 10 << 10 | 10 << 20));
}

void e1000_read_link_status(network_device *dev)
{
    uint32_t status = e1000_read32(dev, E1000_REG_STATUS);

    uint8_t duplex = (status >> 0) & 1;
    uint8_t link_up = (status >> 1) & 1;
    uint8_t speed = (status >> 6) & 3;

    dprintf(1, "[e1000] Link is %s, %s duplex, %u mbps\n", (link_up ? "up" : "down"), (duplex ? "full" : "half"), (speed == 0 ? 10 : (speed == 1 ? 100 : ((speed == 2 || speed == 3) ? 1000 : 0))));
}

e1000_tx_desc *e1000_write_transmit_descriptor(network_device *driver, unsigned int ptr, void *addr, size_t packet_size, uint8_t cso, uint8_t cmd, uint8_t status, uint8_t css)
{
    e1000_tx_desc *descriptor = &((e1000_tx_desc *)driver->tx_descs)[ptr];
    descriptor->addr = (uint64_t)(uintptr_t)addr;
    descriptor->length = packet_size;
    descriptor->cso = cso;
    descriptor->cmd = cmd;
    descriptor->status = status;
    descriptor->css = css;
    return descriptor;
}

uint8_t e1000_packet_write(network_device *driver, ethernet_packet *packet, size_t data_size)
{
    e1000_write_transmit_descriptor(driver, driver->tx_cur, (void *)packet, sizeof(ethernet_header), 0, E1000_REGBIT_TXD_CMD_RS, 0, 0);
    driver->tx_cur = (driver->tx_cur + 1) % NUM_TX_DESC;

    e1000_tx_desc *_d = e1000_write_transmit_descriptor(driver, driver->tx_cur, (void *)packet->data, data_size, 0, E1000_REGBIT_TXD_CMD_EOP | E1000_REGBIT_TXD_CMD_RS, 0, 0);
    driver->tx_cur = (driver->tx_cur + 1) % NUM_TX_DESC;

    interrupts_disable();
    e1000_write32(driver, E1000_REG_TDT, driver->tx_cur);
    while (_d->status == 0)
        ;
    interrupts_reenable();

    return 0;
}

bool e1000_read_receive_descriptor(network_device *driver, unsigned int ptr)
{
    e1000_rx_desc *descriptor = &((e1000_rx_desc *)driver->rx_descs)[ptr];

    if (descriptor->errors)
    {
        dprintf(0, "[e1000] Packet has errors: %x\n", descriptor->errors);
        return true;
    }

    if (ISSET_BIT_INT(descriptor->status, E1000_REGBIT_RXD_STAT_DD))
    {
        if (!ISSET_BIT_INT(descriptor->status, E1000_REGBIT_RXD_STAT_EOP))
        {
            dprintf(1, "[e1000] Packet not supported\n");
            return true;
        }

        ethernet_header *header = (ethernet_header *)(uintptr_t)descriptor->addr;
        descriptor->status = 0;

        ethernet_packet packet;
        packet.header = *header;
        packet.data = (void *)(uintptr_t)(descriptor->addr + sizeof(ethernet_header));

        ethernet_receive_packet(driver, &packet, descriptor->length - sizeof(ethernet_header));

        return true;
    }

    return false;
}

static void e1000_read_packet(network_device *driver)
{
    uint32_t rdh = e1000_read32(driver, E1000_REG_RDH);
    uint32_t rdt = e1000_read32(driver, E1000_REG_RDT);
    size_t rx_buf_len = NUM_RX_DESC - ((rdt - rdh + NUM_RX_DESC) % NUM_RX_DESC);

    // parse the current RX descriptor in the ring buffer
    // write the current RX descriptor (the next one) to the card's register for "just read" descriptor
    e1000_read_receive_descriptor(driver, driver->rx_cur);
    e1000_write32(driver, E1000_REG_RDT, driver->rx_cur);

    // increment the current RX descriptor
    driver->rx_cur = (driver->rx_cur + 1) % (driver->rx_size / sizeof(e1000_rx_desc));

    // see if any other packets have come in while we were parsing that one, if so, read them
    if (rx_buf_len > 2)
        e1000_read_packet(driver);
}

void e1000_reset(network_device *netdev)
{
    e1000_write32(netdev, E1000_REG_RCTL, 0);
    e1000_write32(netdev, E1000_REG_TCTL, E1000_REGBIT_TCTL_PSP);
    e1000_read32(netdev, E1000_REG_STATUS);

    uint32_t ctrl = e1000_read32(netdev, E1000_REG_CTRL);
    ctrl |= E1000_REGBIT_CTRL_RST;
    e1000_write32(netdev, E1000_REG_CTRL, ctrl);

    do
    {
    } while (e1000_read32(netdev, E1000_REG_CTRL) & E1000_REGBIT_CTRL_RST);
}

void e1000_int_handle(network_device *netdev, registers_t *r)
{
    uint32_t icr = e1000_read32(netdev, E1000_REG_ICR);
    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_TXDW))
    {
        // printf("Transmit done\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_TXQE))
    {
        // printf("Transmit queue empty\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_LSC))
    {
        // printf("Link status changed\n");
        e1000_read_link_status(netdev);
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_RXSEQ))
    {
        dprintf(0, "Receive sequence error\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_RXDMT0))
    {
        // printf("RX Ring Buffer > 50%% Full\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_RXO))
    {
        dprintf(0, "Receive overrun\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_RXT0))
    {
        // printf("Receive done\n");
        // io_wait();
        e1000_read_packet(netdev);
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_MDAC))
    {
        dprintf(0, "MDIO access complete\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_RXCFG))
    {
        dprintf(0, "Receive config\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_PHYINT))
    {
        dprintf(0, "PHY interrupt\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_GPI))
    {
        dprintf(0, "GPI\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_TXD_LOW))
    {
        // printf("Transmit descriptor low\n");
    }

    if (ISSET_BIT_INT(icr, E1000_REGBIT_ICR_SRPD))
    {
        dprintf(0, "Small receive packet detected\n");
    }
}

network_device *e1000_init(pci_device *pci)
{
    network_device *netdev = (network_device *)calloc(sizeof(network_device));
    if (!netdev)
        panic("malloc failed (e1000 init)");

    netdev->pci_device = pci;
    netdev->eeprom_exists = false;

    netdev->write = &e1000_packet_write;
    netdev->int_enable = &e1000_int_enable;
    netdev->int_disable = &e1000_int_disable;
    netdev->int_handle = &e1000_int_handle;

    dprintf(0, "[e1000] Net driver init for dev at BAR0: %x, BAR1: %x, BAR0 Type: %x\n", pci->bar[0], pci->bar[1], pci->bar0_type);

    e1000_reset(netdev);
    e1000_write32(netdev, E1000_REG_EECD, E1000_REGBIT_EECD_SK | E1000_REGBIT_EECD_CS | E1000_REGBIT_EECD_DI);
    e1000_detectEEPROM(netdev);

    if (!e1000_read_mac(netdev))
        return NULL;

    dprintf(0, "[e1000] MAC Address: %m\n", netdev->mac);

    e1000_write32(netdev, E1000_REG_FCAL, 0);
    e1000_write32(netdev, E1000_REG_FCAH, 0);
    e1000_write32(netdev, E1000_REG_FCT, 0);
    e1000_write32(netdev, E1000_REG_FCTTV, 0);

    e1000_write32(netdev, E1000_REG_CTRL, E1000_REGBIT_CTRL_ASDE | E1000_REGBIT_CTRL_SLU);

    for (int i = 0; i < 0x80; i++)
        e1000_write32(netdev, 0x5200 + i * 4, 0);

    e1000_rx_init(netdev);
    e1000_tx_init(netdev);

    e1000_write32(netdev, E1000_REG_RADV, 0);
    e1000_write32(netdev, E1000_REG_RDTR, E1000_REGBIT_RDT_RDTR_FPD | 0);
    e1000_write32(netdev, E1000_REG_ITR, 5000);

    e1000_read_link_status(netdev);

    uint64_t ms_elapsed = 0;
    while (!((e1000_read32(netdev, E1000_REG_STATUS) >> 1) & 1))
    {
        timer_wait(10);
        ms_elapsed += 10;

        if (ms_elapsed > E1000_LINKUP_TIMEOUT)
        {
            dprintf(0, "[e1000] Device did not come up in time\n");
            return 0;
        }
    }
    dprintf(1, "[e1000] Configuration done\n");

    return netdev;
}