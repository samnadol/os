#include "ide.h"

#include "../../../lib/bits.h"
#include "../../../hw/cpu/irq.h"
#include "../../../hw/port.h"
#include "../../../hw/timer.h"
#include "../../../hw/mem.h"

struct IDEChannel
{
    uint16_t base;  // I/O Base.
    uint16_t ctrl;  // Control Base
    uint16_t bmide; // Bus Master IDE
    uint8_t nIEN;   // nIEN (No Interrupt);
} channels[2];

struct ide_device
{
    uint8_t reserved;      // 0 (Empty) or 1 (This Drive really exists).
    uint8_t channel;       // 0 (Primary Channel) or 1 (Secondary Channel).
    uint8_t drive;         // 0 (Master Drive) or 1 (Slave Drive).
    uint16_t type;         // 0: ATA, 1:ATAPI.
    uint16_t signature;    // Drive Signature
    uint16_t capabilities; // Features.
    uint32_t command_sets; // Command Sets Supported.
    uint32_t size;         // Size in Sectors.
    uint8_t model[41];     // Model in string.
} ide_devices[4];

uint8_t *ide_buf;
static uint8_t atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void ide_write(uint8_t channel, uint8_t reg, uint8_t data)
{
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        outb(channels[channel].base + reg - 0x00, data);
    else if (reg < 0x0C)
        outb(channels[channel].base + reg - 0x06, data);
    else if (reg < 0x0E)
        outb(channels[channel].ctrl + reg - 0x0A, data);
    else if (reg < 0x16)
        outb(channels[channel].bmide + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

uint8_t ide_read(uint8_t channel, uint8_t reg)
{
    uint8_t result = 0;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);

    if (reg < 0x08)
        result = inb(channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = inb(channels[channel].base + reg - 0x06);
    else if (reg < 0x0E)
        result = inb(channels[channel].ctrl + reg - 0x0A);
    else if (reg < 0x16)
        result = inb(channels[channel].bmide + reg - 0x0E);

    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}

inline void insl(uint16_t addr, uint32_t *buffer, size_t quads)
{
    for (uint8_t i = 0; i < quads; i++)
        buffer[i] = inl(addr);
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t *buffer, uint16_t quads)
{
    uint16_t i;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);

    if (reg < 0x08)
        insl(channels[channel].base + reg - 0x00, buffer, quads);
    else if (reg < 0x0C)
        insl(channels[channel].base + reg - 0x06, buffer, quads);
    else if (reg < 0x0E)
        insl(channels[channel].ctrl + reg - 0x0A, buffer, quads);
    else if (reg < 0x16)
        insl(channels[channel].bmide + reg - 0x0E, buffer, quads);

    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void ide_device_init(pci_device *pci)
{
    ide_buf = (uint8_t *)calloc(2048);

    // if BARs are 0, ide controller is indicating it wants to use legacy ports, set BARs to those
    pci->bar[0] = pci->bar[0] ?: 0x1F0;
    pci->bar[1] = pci->bar[1] ?: 0x3F6;
    pci->bar[2] = pci->bar[2] ?: 0x170;
    pci->bar[3] = pci->bar[3] ?: 0x376;

    // set channel access ports to BARs
    channels[ATA_CHANNEL_PRIMARY].base = pci->bar[0];
    channels[ATA_CHANNEL_PRIMARY].ctrl = pci->bar[1];
    channels[ATA_CHANNEL_SECONDARY].base = pci->bar[2];
    channels[ATA_CHANNEL_SECONDARY].ctrl = pci->bar[3];
    channels[ATA_CHANNEL_PRIMARY].bmide = pci->bar[4] + 0;
    channels[ATA_CHANNEL_SECONDARY].bmide = pci->bar[4] + 8;

    // disable interrupts
    ide_write(ATA_CHANNEL_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_CHANNEL_SECONDARY, ATA_REG_CONTROL, 2);

    uint8_t drive_count = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
        {
            uint8_t type = 0xFF;

            ide_devices[drive_count].reserved = 0;

            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
            timer_wait(1);

            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            timer_wait(1);

            if (ide_read(i, ATA_REG_STATUS) == 0)
                continue;

            uint8_t status;
            while (1)
            {
                status = ide_read(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR))
                {
                    // if ATA_SR_ERR is 1, device is not ATA. it could be ATAPI, check
                    uint8_t cl = ide_read(i, ATA_REG_LBA1);
                    uint8_t ch = ide_read(i, ATA_REG_LBA2);

                    if (cl == 0x14 && ch == 0xEB)
                        type = IDE_INTERFACE_ATAPI;
                    else if (cl == 0x69 && ch == 0x96)
                        type = IDE_INTERFACE_ATAPI;
                    else
                        break; // device is not ATA or ATAPI, skip

                    ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                    timer_wait(1);
                    break;
                }

                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                {
                    type = IDE_INTERFACE_ATA;
                    break; // device is ATA
                }
            }

            if (type == 0xFF)
                continue;

            ide_read_buffer(i, ATA_REG_DATA, (uint32_t *)ide_buf, 128);
            ide_devices[drive_count].reserved = 1;
            ide_devices[drive_count].type = type;
            ide_devices[drive_count].channel = i;
            ide_devices[drive_count].drive = j;
            ide_devices[drive_count].signature = *((uint16_t *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[drive_count].capabilities = *((uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[drive_count].command_sets = *((uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS));

            if (ide_devices[drive_count].command_sets & (1 << 26)) // 48-bit addressing
                ide_devices[drive_count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            else // chs or 28-bit addressing
                ide_devices[drive_count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA));

            for (size_t k = 0; k < 40; k += 2)
            {
                ide_devices[drive_count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[drive_count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[drive_count].model[40] = 0;

            drive_count++;
        }

    // 4- Print Summary:
    for (size_t i = 0; i < 4; i++)
        if (ide_devices[i].reserved == 1)
        {
            printf("[IDE] Found %s Drive %f - %s\n",
                   (const char *[]){"ATA", "ATAPI"}[ide_devices[i].type], /* Type */
                   (ide_devices[i].size / 2) * 1024,                      /* Size */
                   ide_devices[i].model);
        }
}