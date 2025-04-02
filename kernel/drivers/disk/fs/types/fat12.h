#ifndef __FAT_12_
#define __FAT_12_

#include "../../ata.h"
#include "../fs.h"

// low level
fat_bootsector *fat_read_bs(ide_device *ide);
uint16_t *fat12_read_table(ide_device *ide, fat_bootsector *bs);
uint16_t *fat12_read_root(ide_device *ide, fat_bootsector *bs);
uint16_t *fat12_read_data(ide_device *ide, fat_bootsector *bs, uint16_t *fat, uint16_t offset_from_root);

// parsers & helpers
void fat12_parse_filename(char *dest, const char *fat_name);
uint16_t fat12_get_data_sector(FSDriver *driver, Path *path, EntryType type);
void fat12_free_listing(fat_directory_listing *toFree);

// frontend
fat_directory_listing *fat12_list_directory(FSDriver *driver, Path *path);
uint16_t *fat12_read_file(FSDriver *driver, Path *path);
void fat12_write_file(FSDriver *driver, Path *path, uint16_t *data);

// driver interface
FSDriver *fat12_init_driver(ide_device *ide);

#endif