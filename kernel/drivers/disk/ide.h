#pragma once

#include "../../hw/pci.h"

enum ATAPI_CMD
{
    ATAPI_CMD_READ = 0xA8,
    ATAPI_CMD_EJECT = 0x1B,
};

enum ATA_IDENT
{
    ATA_IDENT_DEVICETYPE = 0,
    ATA_IDENT_CYLINDERS = 2,
    ATA_IDENT_HEADS = 6,
    ATA_IDENT_SECTORS = 12,
    ATA_IDENT_SERIAL = 20,
    ATA_IDENT_MODEL = 54,
    ATA_IDENT_CAPABILITIES = 98,
    ATA_IDENT_FIELDVALID = 106,
    ATA_IDENT_MAX_LBA = 120,
    ATA_IDENT_COMMANDSETS = 164,
    ATA_IDENT_MAX_LBA_EXT = 200
};

enum IDE_INTERFACE
{
    IDE_INTERFACE_ATA = 0x00,
    IDE_INTERFACE_ATAPI = 0x01
};

enum ATA_DEVICE
{
    ATA_DEVICE_MASTER = 0x00,
    ATA_DEVICE_SLAVE = 0x01
};

enum ATA_CHANNEL
{
    ATA_CHANNEL_PRIMARY = 0x00,
    ATA_CHANNEL_SECONDARY = 0x01
};

enum ATA_DIRECTION
{
    ATA_DIRECTION_READ = 0x00,
    ATA_DIRECTION_WRITE = 0x01
};

enum ATA_SR
{
    ATA_SR_BSY = 0x80,  // Busy
    ATA_SR_DRDY = 0x40, // Drive ready
    ATA_SR_DF = 0x20,   // Drive write fault
    ATA_SR_DSC = 0x10,  // Drive seek complete
    ATA_SR_DRQ = 0x08,  // Data request ready
    ATA_SR_CORR = 0x04, // Corrected data
    ATA_SR_IDX = 0x02,  // Index
    ATA_SR_ERR = 0x01   // Error
};

enum ATA_ER
{
    ATA_ER_BBK = 0x80,   // Bad block
    ATA_ER_UNC = 0x40,   // Uncorrectable data
    ATA_ER_MC = 0x20,    // Media changed
    ATA_ER_IDNF = 0x10,  // ID mark not found
    ATA_ER_MCR = 0x08,   // Media change request
    ATA_ER_ABRT = 0x04,  // Command aborted
    ATA_ER_TK0NF = 0x02, // Track 0 not found
    ATA_ER_AMNF = 0x01   // No address mark
};

enum ATA_CMD
{
    ATA_CMD_READ_PIO = 0x20,
    ATA_CMD_READ_PIO_EXT = 0x24,
    ATA_CMD_READ_DMA = 0xC8,
    ATA_CMD_READ_DMA_EXT = 0x25,
    ATA_CMD_WRITE_PIO = 0x30,
    ATA_CMD_WRITE_PIO_EXT = 0x34,
    ATA_CMD_WRITE_DMA = 0xCA,
    ATA_CMD_WRITE_DMA_EXT = 0x35,
    ATA_CMD_CACHE_FLUSH = 0xE7,
    ATA_CMD_CACHE_FLUSH_EXT = 0xEA,
    ATA_CMD_PACKET = 0xA0,
    ATA_CMD_IDENTIFY_PACKET = 0xA1,
    ATA_CMD_IDENTIFY = 0xEC,
};

enum ATA_REG
{
    ATA_REG_DATA = 0x00,
    ATA_REG_ERROR = 0x01,
    ATA_REG_FEATURES = 0x01,
    ATA_REG_SECCOUNT0 = 0x02,
    ATA_REG_LBA0 = 0x03,
    ATA_REG_LBA1 = 0x04,
    ATA_REG_LBA2 = 0x05,
    ATA_REG_HDDEVSEL = 0x06,
    ATA_REG_COMMAND = 0x07,
    ATA_REG_STATUS = 0x07,
    ATA_REG_SECCOUNT1 = 0x08,
    ATA_REG_LBA3 = 0x09,
    ATA_REG_LBA4 = 0x0A,
    ATA_REG_LBA5 = 0x0B,
    ATA_REG_CONTROL = 0x0C,
    ATA_REG_ALTSTATUS = 0x0C,
    ATA_REG_DEVADDRESS = 0x0D
};

typedef struct ide_channel
{
    uint16_t io_base;   // I/O Base.
    uint16_t ctrl_base; // Control Base
    uint16_t bmIDE;     // Bus Master IDE
    uint8_t nIEN;       // nIEN (No Interrupt);
} ide_channel;

typedef struct ide_device
{
    uint8_t reserved;      // 0 (Empty) or 1 (This Drive really exists).
    ide_channel channel;   // channel (primary or secondary)
    uint8_t drive;         // 0 (Master Drive) or 1 (Slave Drive).
    uint16_t type;         // 0: ATA, 1:ATAPI.
    uint16_t signature;    // Drive Signature
    uint16_t capabilities; // Features.
    uint32_t command_sets; // Command Sets Supported.
    uint32_t size;         // Size in Sectors.
    uint32_t sector_size;  // sector size, in words
    uint8_t model[41];     // Model in string.
} ide_device;

void ide_device_init(pci_device *pci);

void ide_write(ide_channel c, uint8_t reg, uint8_t data);
uint8_t ide_read(ide_channel c, uint8_t reg);

void ide_test(tty_interface *tty, uint16_t word);