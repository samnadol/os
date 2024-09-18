#pragma once

#include "../../../hw/pci.h"

typedef struct ide_channel_register
{
    uint16_t base;  // I/O Base.
    uint16_t ctrl;  // Control Base
    uint16_t bmide; // Bus Master IDE
    uint8_t nIEN;   // nIEN (No Interrupt);
} ide_channel_register;

typedef struct ide_device
{
    uint8_t reserved;             // 0 (Empty) or 1 (This Drive really exists).
    uint8_t channel;       // 0 (Primary Channel) or 1 (Secondary Channel).
    uint8_t drive;         // 0 (Master Drive) or 1 (Slave Drive).
    uint16_t type;         // 0: ATA, 1:ATAPI.
    uint16_t signature;    // Drive Signature
    uint16_t capabilities; // Features.
    uint32_t commandsets;  // Command Sets Supported.
    uint32_t size;         // Size in Sectors.
    uint32_t sector_size;
    uint8_t model[41];     // Model string.
} ide_device;

void ata_device_init(pci_device *pci);
