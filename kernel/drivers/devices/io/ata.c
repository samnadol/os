#include "ata.h"

#include "../../../hw/port.h"
#include "../../../hw/timer.h"
#include "../../../hw/mem.h"

uint16_t *ata_28bit_pio_read_sector(ide_device d, uint32_t lba, uint32_t sectorcount)
{
    ide_write(d.channel, ATA_REG_HDDEVSEL, 0xE0 | (d.drive << 4)); // select drive
    ide_write(d.channel, ATA_REG_SECCOUNT0, sectorcount);          // select sector count
    ide_write(d.channel, ATA_REG_LBA0, (uint8_t)lba);              // send first 8 bits of LBA
    ide_write(d.channel, ATA_REG_LBA1, (uint8_t)(lba >> 8));       // send second 8 bits of LBA
    ide_write(d.channel, ATA_REG_LBA2, (uint8_t)(lba >> 16));      // send third 8 bits of LBA
    ide_write(d.channel, ATA_REG_COMMAND, ATA_CMD_READ_PIO);       // send read command

    uint8_t status;
    uint32_t time = 0;
    while (time < 1000)
    {
        status = ide_read(d.channel, ATA_REG_STATUS);
        if (status & ATA_SR_DRQ || status & ATA_SR_ERR)
            break;

        timer_wait(1);
        time++;
    }
    if (status & ATA_SR_ERR)
        return 0;

    uint16_t *buf = (uint16_t *)malloc(sectorcount * d.sector_size * 2);
    if (status & ATA_SR_DRQ)
    {
        for (int i = 0; i < d.sector_size; i++)
            buf[i] = inw(d.channel.io_base + ATA_REG_DATA);
        status = ide_read(d.channel, ATA_REG_STATUS);
    }

    time = 0;
    while (time < 1000)
    {
        status = ide_read(d.channel, ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            break;
        timer_wait(1);
        time++;
    }

    return buf;
}

bool ata_28bit_pio_write_sector(ide_device d, uint32_t lba, uint32_t sectorcount, uint16_t *data)
{
    ide_write(d.channel, ATA_REG_HDDEVSEL, 0xE0 | (d.drive << 4)); // select drive
    ide_write(d.channel, ATA_REG_SECCOUNT0, sectorcount);          // select sector count
    ide_write(d.channel, ATA_REG_LBA0, (uint8_t)lba);              // send first 8 bits of LBA
    ide_write(d.channel, ATA_REG_LBA1, (uint8_t)(lba >> 8));       // send second 8 bits of LBA
    ide_write(d.channel, ATA_REG_LBA2, (uint8_t)(lba >> 16));      // send third 8 bits of LBA
    ide_write(d.channel, ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);      // send read command

    uint8_t status;
    uint32_t time = 0;
    while (time < 1000)
    {
        status = ide_read(d.channel, ATA_REG_STATUS);
        if (status & ATA_SR_DRQ || status & ATA_SR_ERR)
            break;

        timer_wait(1);
        time++;
    }
    if (status & ATA_SR_ERR)
        return 0;

    for (int i = 0; i < d.sector_size; i++)
        outw(d.channel.io_base + ATA_REG_DATA, data[i]);
    status = ide_read(d.channel, ATA_REG_STATUS);

    time = 0;
    while (time < 100000)
    {
        status = ide_read(d.channel, ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            break;
        timer_wait(10);
        time++;
    }

    return 1;
}

// reads 1 sector, counts words read until DRQ is 0
uint16_t ata_get_sector_size(ide_device d)
{
    ide_write(d.channel, ATA_REG_HDDEVSEL, 0xE0 | (d.drive << 4));
    ide_write(d.channel, ATA_REG_SECCOUNT0, 1);
    ide_write(d.channel, ATA_REG_LBA0, 0);
    ide_write(d.channel, ATA_REG_LBA1, 0);
    ide_write(d.channel, ATA_REG_LBA2, 0);
    ide_write(d.channel, ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint8_t status;
    uint32_t time = 0;
    while (time < 1000)
    {
        status = ide_read(d.channel, ATA_REG_STATUS);
        if (status & ATA_SR_DRQ || status & ATA_SR_ERR)
            break;

        timer_wait(1);
        time++;
    }

    if (status & ATA_SR_ERR)
        return 0;

    uint16_t num_words = 0;
    while (status & ATA_SR_DRQ)
    {
        inw(d.channel.io_base + ATA_REG_DATA);
        status = ide_read(d.channel, ATA_REG_STATUS);
        num_words++;
    }
    return num_words;
}

uint16_t ata_read_word(ide_device d, uint32_t lba, uint16_t offset)
{
    uint16_t *data = ata_28bit_pio_read_sector(d, lba, 1);
    uint16_t word = data[offset];
    mfree(data);
    return word;
}

bool ata_write_word(ide_device d, uint32_t lba, uint16_t offset, uint16_t data)
{
    uint16_t *old_data = ata_28bit_pio_read_sector(d, lba, 1);
    old_data[offset] = data;
    return ata_28bit_pio_write_sector(d, lba, 1, old_data);
}