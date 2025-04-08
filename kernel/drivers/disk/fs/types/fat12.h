#ifndef __FAT_12_
#define __FAT_12_

#include "../../ata.h"
#include "../fs.h"

typedef union __attribute__((packed)) date_t
{
    struct __attribute__((packed))
    {
        unsigned day : 5;
        unsigned month : 4;
        unsigned year : 7;
    };
    uint16_t raw;
} date_t;
typedef union __attribute__((packed)) time_t
{
    struct __attribute__((packed))
    {
        unsigned second : 5;
        unsigned minute : 6;
        unsigned hour : 5;
    };
    uint16_t raw;
} time_t;

typedef struct __attribute__((packed)) fat_bios_parameter_block
{
    uint16_t bytes_per_logical_sector;
    uint8_t logical_sectors_per_cluster;
    uint16_t reserved_logical_sectors;
    uint8_t num_fat_tables;
    uint16_t max_root_directory_entries;
    uint16_t total_logical_sectors_sm;
    uint8_t media_descriptor;
    uint16_t logical_sectors_per_fat;

    uint16_t physical_sectors_per_track;
    uint16_t number_of_heads;
    uint32_t number_of_hidden_sectors;
    uint32_t total_logical_sectors_lg;

    uint8_t physical_drive_number;
    uint8_t reserved;
    uint8_t extended_boot_signature;
    uint32_t volume_id;
    uint8_t partition_volume_label[11];
    uint8_t file_system_type[8];

    uint8_t _[499 - 51];
} fat_bios_parameter_block;
typedef union __attribute__((packed)) fat_bootsector
{
    struct __attribute__((packed))
    {
        uint8_t jmp[3];
        char oem_name[8];
        fat_bios_parameter_block ebpb;
        uint16_t signature;
    };
    uint16_t raw[256];
} fat_bootsector;

typedef union __attribute__((packed)) fat_directory_entry_standard
{
    struct __attribute__((packed))
    {
        char name[8];
        char ext[3];
        uint8_t attributes;
        uint8_t _;
        uint8_t creation_ms;
        time_t creation_time;
        date_t creation_date;
        date_t last_access_date;
        uint16_t first_cluster_number_high;
        time_t last_modify_time;
        date_t last_modify_date;
        uint16_t first_cluster_number_low;
        uint32_t file_size_bytes;
    };
    uint16_t raw[16];
} fat_directory_entry_standard;
typedef union __attribute__((packed)) fat_directory_entry_longname
{
    struct __attribute__((packed))
    {
        uint8_t order;
        uint16_t first5[5];
        uint8_t attribute;
        uint8_t long_entry_type;
        uint8_t checksum;
        uint16_t next6[6];
        uint16_t zero;
        uint16_t final2[2];
    };
    uint16_t raw[16];
} fat_directory_entry_longname;

typedef struct fat_directory_listing
{
    union
    {
        fat_directory_entry_standard contents[512];
        uint16_t raw[8192];
    };
} fat_directory_listing;

void fat12_parse_filename(char *dest, const char *fat_name);
void fat12_free_listing(fat_directory_listing *toFree);

FileInfoType fat12_get_file_info(FSDriver *driver, Path *path, EntryType type, FileInfo** info);
FileInfoType fat12_get_fat_file_info(FSDriver *driver, Path *path, EntryType type, fat_directory_entry_standard **output);

fat_directory_listing *fat12_get_directory_sector(FSDriver *driver, Path *path);

// driver interface

FSDriver *fat12_init_driver(ide_device *ide);

#endif