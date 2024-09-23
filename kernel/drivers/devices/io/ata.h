#pragma once

#include "ide.h"
#include <stdbool.h>

uint16_t ata_get_sector_size(ide_device d);

uint16_t *ata_28bit_pio_read_sector(ide_device d, uint32_t lba, uint32_t sectorcount);
bool ata_28bit_pio_write_sector(ide_device d, uint32_t lba, uint32_t sectorcount, uint16_t *data);

uint16_t ata_read_word(ide_device d, uint32_t lba, uint16_t offset);
bool ata_write_word(ide_device d, uint32_t lba, uint16_t offset, uint16_t data);