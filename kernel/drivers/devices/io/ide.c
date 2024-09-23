#include "ide.h"

#include "ata.h"
#include "../../../lib/bits.h"
#include "../../../hw/cpu/irq.h"
#include "../../../hw/port.h"
#include "../../../hw/timer.h"
#include "../../../hw/mem.h"

ide_channel ide_channels[2];
ide_device ide_devices[4];

uint8_t *ide_buf;
static uint8_t atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void ide_write(ide_channel c, uint8_t reg, uint8_t data)
{
    if (reg > 0x07 && reg < 0x0C)
        ide_write(c, ATA_REG_CONTROL, 0x80 | c.nIEN);
    if (reg < 0x08)
        outb(c.io_base + reg - 0x00, data);
    else if (reg < 0x0C)
        outb(c.io_base + reg - 0x06, data);
    else if (reg < 0x0E)
        outb(c.ctrl_base + reg - 0x0A, data);
    else if (reg < 0x16)
        outb(c.bmIDE + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(c, ATA_REG_CONTROL, c.nIEN);
}

uint8_t ide_read(ide_channel c, uint8_t reg)
{
    uint8_t result = 0;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(c, ATA_REG_CONTROL, 0x80 | c.nIEN);

    if (reg < 0x08)
        result = inb(c.io_base + reg - 0x00);
    else if (reg < 0x0C)
        result = inb(c.io_base + reg - 0x06);
    else if (reg < 0x0E)
        result = inb(c.ctrl_base + reg - 0x0A);
    else if (reg < 0x16)
        result = inb(c.bmIDE + reg - 0x0E);

    if (reg > 0x07 && reg < 0x0C)
        ide_write(c, ATA_REG_CONTROL, c.nIEN);
    return result;
}

inline void insl(uint16_t addr, uint32_t *buffer, size_t quads)
{
    for (uint8_t i = 0; i < quads; i++)
        buffer[i] = inl(addr);
}

void ide_read_buffer(ide_channel c, uint8_t reg, uint32_t *buffer, uint16_t quads)
{
    uint16_t i;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(c, ATA_REG_CONTROL, 0x80 | c.nIEN);

    if (reg < 0x08)
        insl(c.io_base + reg - 0x00, buffer, quads);
    else if (reg < 0x0C)
        insl(c.io_base + reg - 0x06, buffer, quads);
    else if (reg < 0x0E)
        insl(c.ctrl_base + reg - 0x0A, buffer, quads);
    else if (reg < 0x16)
        insl(c.bmIDE + reg - 0x0E, buffer, quads);

    if (reg > 0x07 && reg < 0x0C)
        ide_write(c, ATA_REG_CONTROL, c.nIEN);
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
    ide_channels[ATA_CHANNEL_PRIMARY].io_base = pci->bar[0];
    ide_channels[ATA_CHANNEL_PRIMARY].ctrl_base = pci->bar[1];
    ide_channels[ATA_CHANNEL_SECONDARY].io_base = pci->bar[2];
    ide_channels[ATA_CHANNEL_SECONDARY].ctrl_base = pci->bar[3];
    ide_channels[ATA_CHANNEL_PRIMARY].bmIDE = pci->bar[4] + 0;
    ide_channels[ATA_CHANNEL_SECONDARY].bmIDE = pci->bar[4] + 8;

    // disable interrupts
    ide_write(ide_channels[ATA_CHANNEL_PRIMARY], ATA_REG_CONTROL, 2);
    ide_write(ide_channels[ATA_CHANNEL_SECONDARY], ATA_REG_CONTROL, 2);

    uint8_t drive_count = 0;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 2; j++)
        {
            uint8_t type = 0xFF;

            ide_devices[drive_count].reserved = 0;

            ide_write(ide_channels[i], ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
            timer_wait(1);

            ide_write(ide_channels[i], ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            timer_wait(1);

            if (ide_read(ide_channels[i], ATA_REG_STATUS) == 0)
                continue;

            uint8_t status;
            while (true)
            {
                status = ide_read(ide_channels[i], ATA_REG_STATUS);
                if ((status & ATA_SR_ERR))
                {
                    // if ATA_SR_ERR is 1, device is not ATA. it could be ATAPI, check
                    uint8_t cl = ide_read(ide_channels[i], ATA_REG_LBA1);
                    uint8_t ch = ide_read(ide_channels[i], ATA_REG_LBA2);

                    if (cl == 0x14 && ch == 0xEB)
                        type = IDE_INTERFACE_ATAPI;
                    else if (cl == 0x69 && ch == 0x96)
                        type = IDE_INTERFACE_ATAPI;
                    else
                        break; // device is not ATA or ATAPI, skip

                    ide_write(ide_channels[i], ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
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

            ide_read_buffer(ide_channels[i], ATA_REG_DATA, (uint32_t *)ide_buf, 128);
            ide_devices[drive_count].reserved = 1;
            ide_devices[drive_count].type = type;
            ide_devices[drive_count].channel = ide_channels[i];
            ide_devices[drive_count].drive = j;
            ide_devices[drive_count].signature = *((uint16_t *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[drive_count].capabilities = *((uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[drive_count].command_sets = *((uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS));
            ide_devices[drive_count].sector_size = ata_get_sector_size(ide_devices[drive_count]);

            if (ide_devices[drive_count].command_sets & (1 << 26))
                ide_devices[drive_count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA_EXT)); // 48-bit addressing
            else
                ide_devices[drive_count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA)); // chs or 28-bit addressing

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
            printf("[IDE] %s drive, %d %f sectors - %s\n",
                   (const char *[]){"ATA", "ATAPI"}[ide_devices[i].type], /* Type */
                   ide_devices[i].size,                                   /* size in sectors */
                   ide_devices[i].sector_size * 2,                        /* size of each sector*/
                   ide_devices[i].model);                                 /* Model */

            if (ide_devices[i].type == 0)
            {
                uint16_t data = ata_read_word(ide_devices[i], 0, 0);
                printf("%x\n", data);
        
                ata_write_word(ide_devices[i], 0, 0, 0x2345);

                data = ata_read_word(ide_devices[i], 0, 0);
                printf("%x\n", data);
            }
        }
}