#ifndef __FAT_12_
#define __FAT_12_

#include "../../ata.h"
#include "../fs.h"

void fat12_parse_filename(char *dest, const char *fat_name);
void fat12_free_listing(fat_directory_listing *toFree);
int fat12_get_file_info(FSDriver *driver, Path *path, EntryType type, fat_directory_entry_standard** info);

// driver interface

FSDriver *fat12_init_driver(ide_device *ide);

#endif