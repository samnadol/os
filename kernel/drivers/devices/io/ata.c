#include "ata.h"

#include "../../../lib/bits.h"
#include "../../../hw/cpu/irq.h"
#include "../../../hw/mem.h"
#include "../../../hw/port.h"
#include "../../../hw/timer.h"

#define IRQ_ATA_PRIMARY IRQ0 + 14
#define IRQ_ATA_SECONDARY IRQ0 + 15

#define IDE_ATA 0x00
#define IDE_ATAPI 0x01

#define ATA_MASTER 0x00
#define ATA_SLAVE 0x01

// Channels:
#define ATA_PRIMARY 0x00
#define ATA_SECONDARY 0x01

// Directions:
#define ATA_READ 0x00
#define ATA_WRITE 0x01

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07
#define ATA_REG_SECCOUNT1 0x08
#define ATA_REG_LBA3 0x09
#define ATA_REG_LBA4 0x0A
#define ATA_REG_LBA5 0x0B
#define ATA_REG_CONTROL 0x0C
#define ATA_REG_ALTSTATUS 0x0C
#define ATA_REG_DEVADDRESS 0x0D

#define ATA_IDENT_DEVICETYPE 0
#define ATA_IDENT_CYLINDERS 2
#define ATA_IDENT_HEADS 6
#define ATA_IDENT_SECTORS 12
#define ATA_IDENT_SERIAL 20
#define ATA_IDENT_MODEL 54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID 106
#define ATA_IDENT_MAX_LBA 120
#define ATA_IDENT_COMMANDSETS 164
#define ATA_IDENT_MAX_LBA_EXT 200

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_BSY 0x80  // Busy
#define ATA_SR_DRDY 0x40 // Drive ready
#define ATA_SR_DF 0x20   // Drive write fault
#define ATA_SR_DSC 0x10  // Drive seek complete
#define ATA_SR_DRQ 0x08  // Data request ready
#define ATA_SR_CORR 0x04 // Corrected data
#define ATA_SR_IDX 0x02  // Index
#define ATA_SR_ERR 0x01  // Error

#define ATA_ER_BBK 0x80   // Bad block
#define ATA_ER_UNC 0x40   // Uncorrectable data
#define ATA_ER_MC 0x20    // Media changed
#define ATA_ER_IDNF 0x10  // ID mark not found
#define ATA_ER_MCR 0x08   // Media change request
#define ATA_ER_ABRT 0x04  // Command aborted
#define ATA_ER_TK0NF 0x02 // Track 0 not found
#define ATA_ER_AMNF 0x01  // No address mark

uint8_t *ide_buf;
ide_channel_register ide_channel_registers[2];
ide_device ide_devices[4];
uint8_t static atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// static void ata_primary_handler(registers_t *r)
// {
//     dprintf(0, "14\n");
// }

// static void ata_secondary_handler(registers_t *r)
// {
//     dprintf(0, "14\n");
// }

void ide_write(uint8_t channel, uint8_t reg, uint8_t data)
{
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | ide_channel_registers[channel].nIEN);
    if (reg < 0x08)
        outb(ide_channel_registers[channel].base + reg - 0x00, data);
    else if (reg < 0x0C)
        outb(ide_channel_registers[channel].base + reg - 0x06, data);
    else if (reg < 0x0E)
        outb(ide_channel_registers[channel].ctrl + reg - 0x0A, data);
    else if (reg < 0x16)
        outb(ide_channel_registers[channel].bmide + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, ide_channel_registers[channel].nIEN);
}

uint8_t ide_read(uint8_t channel, uint8_t reg)
{
    uint8_t result = 0;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | ide_channel_registers[channel].nIEN);
    if (reg < 0x08)
        result = inb(ide_channel_registers[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = inb(ide_channel_registers[channel].base + reg - 0x06);
    else if (reg < 0x0E)
        result = inb(ide_channel_registers[channel].ctrl + reg - 0x0A);
    else if (reg < 0x16)
        result = inb(ide_channel_registers[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, ide_channel_registers[channel].nIEN);
    return result;
}

void insl(uint16_t addr, uint16_t *buffer, size_t quads)
{
    for (size_t i = 0; i < quads; i++)
    {
        buffer[i] = inl(addr);
    }
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint16_t *buffer, uint16_t quads)
{
    uint16_t i;

    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, 0x80 | ide_channel_registers[channel].nIEN);
    }

    if (reg < 0x08)
    {
        insl(ide_channel_registers[channel].base + reg - 0x00, buffer, quads);
    }
    else if (reg < 0x0C)
    {
        insl(ide_channel_registers[channel].base + reg - 0x06, buffer, quads);
    }
    else if (reg < 0x0E)
    {
        insl(ide_channel_registers[channel].ctrl + reg - 0x0A, buffer, quads);
    }
    else if (reg < 0x16)
    {
        insl(ide_channel_registers[channel].bmide + reg - 0x0E, buffer, quads);
    }

    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, ide_channel_registers[channel].nIEN);
    }
}

void ata_device_init(pci_device *pci)
{
    // ide_buf = (uint8_t *)calloc(2048);

    // ide_channel_registers[ATA_PRIMARY].base = (pci->bar[0] & 0xFFFFFFFC) + 0x1F0 * (!(pci->bar[0]));
    // ide_channel_registers[ATA_PRIMARY].ctrl = (pci->bar[1] & 0xFFFFFFFC) + 0x3F6 * (!(pci->bar[1]));
    // ide_channel_registers[ATA_SECONDARY].base = (pci->bar[2] & 0xFFFFFFFC) + 0x170 * (!(pci->bar[2]));
    // ide_channel_registers[ATA_SECONDARY].ctrl = (pci->bar[3] & 0xFFFFFFFC) + 0x376 * (!(pci->bar[3]));
    // ide_channel_registers[ATA_PRIMARY].bmide = (pci->bar[4] & 0xFFFFFFFC) + 0;   // Bus Master IDE
    // ide_channel_registers[ATA_SECONDARY].bmide = (pci->bar[4] & 0xFFFFFFFC) + 8; // Bus Master IDE

    // ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    // ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

    // // 3- Detect ATA-ATAPI Devices:
    // int count = 0;
    // for (int i = 0; i < 2; i++)     // iterate through master and slave ide controllers
    //     for (int j = 0; j < 2; j++) // iterate through both devices on controllers
    //     {
    //         bool err = false;
    //         uint8_t type;

    //         ide_devices[count].reserved = 0; // assuming no drive is present to start

    //         // select drive
    //         ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // select drive number
    //         timer_wait(1);

    //         // send ATA identify command
    //         ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    //         timer_wait(1);

    //         uint8_t status = ide_read(i, ATA_REG_STATUS);
    //         if (status == 0)
    //             continue; // if status = 0, then no device is present

    //         while (true)
    //         {
    //             status = ide_read(i, ATA_REG_STATUS);
    //             if ((status & ATA_SR_ERR)) // device is not ATA
    //             {
    //                 unsigned char cl = ide_read(i, ATA_REG_LBA1);
    //                 unsigned char ch = ide_read(i, ATA_REG_LBA2);

    //                 if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96))
    //                     type = IDE_ATAPI;
    //                 else
    //                     continue; // device is not ATAPI or ATA, do not try to continue

    //                 ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
    //                 timer_wait(1);
    //                 break;
    //             }
    //             if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) // device is ATA, all good
    //             {
    //                 type = IDE_ATA;
    //                 break;
    //             }
    //         }

    //         // (V) Read Identification Space of the Device:
    //         ide_read_buffer(i, ATA_REG_DATA, (uint16_t *)ide_buf, 128);

    //         // (VI) Read Device Parameters:
    //         ide_devices[count].reserved = 1;
    //         ide_devices[count].type = type;
    //         ide_devices[count].channel = i;
    //         ide_devices[count].drive = j;
    //         ide_devices[count].signature = *((uint32_t *)(ide_buf + ATA_IDENT_DEVICETYPE));
    //         ide_devices[count].capabilities = *((uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES));
    //         ide_devices[count].commandsets = *((uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS));

    //         if (type == IDE_ATA)
    //         {
    //             ide_devices[count].sector_size = 512;
    //             if (ISSET_BIT_INT(ide_devices[count].commandsets, (1UL << 26UL)))
    //             {
    //                 ide_devices[count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
    //             }
    //             else
    //             {
    //                 ide_devices[count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA));
    //             }
    //         }
    //         else if (type == IDE_ATAPI)
    //         {
    //             // printf("fdsfds\n");
    //         }

    //         // (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
    //         for (int k = 0; k < 40; k += 2)
    //         {
    //             ide_devices[count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
    //             ide_devices[count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
    //         }
    //         ide_devices[count].model[40] = 0; // Terminate String.

    //         count++;
    //     }

    // // 4- Print Summary:
    // for (int i = 0; i < 4; i++)
    // {
    //     if (ide_devices[i].reserved == 1)
    //     {
    //         printf("Found %s Drive %f - 0x%p\n",
    //                (const char *[]){"ATA", "ATAPI"}[ide_devices[i].type], /* Type */
    //                ide_devices[i].size / 2,                               /* Size */
    //                ide_devices[i].model);                                 /* Model */
    //     }
    // }

    // irq_register(IRQ_ATA_PRIMARY, ata_primary_handler);
    // irq_register(IRQ_ATA_SECONDARY, ata_secondary_handler);
    // return &ata_io;
}

void ide_initialize(pci_device *pci)
{
    int j, k, count = 0;
}