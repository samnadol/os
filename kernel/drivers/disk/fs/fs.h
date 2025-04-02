#pragma once

#include "path.h"
#include "../ata.h"


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
    // 13 bytes
    uint16_t bytes_per_logical_sector;
    uint8_t logical_sectors_per_cluster;
    uint16_t reserved_logical_sectors;
    uint8_t num_fat_tables;
    uint16_t max_root_directory_entries;
    uint16_t total_logical_sectors_sm;
    uint8_t media_descriptor;
    uint16_t logical_sectors_per_fat;

    // 12 bytes
    uint16_t physical_sectors_per_track;
    uint16_t number_of_heads;
    uint32_t number_of_hidden_sectors;
    uint32_t total_logical_sectors_lg;

    // 26 bytes
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

typedef struct __attribute__((packed)) fat_directory_listing
{
    union __attribute__((packed))
    {
        fat_directory_entry_standard contents[512];
        uint16_t raw[8192];
    };
    uint16_t num_contents;
} fat_directory_listing;

typedef enum
{
    FileEntry,
    DirectoryEntry,
} EntryType;

typedef struct FSDriver
{
    uint16_t *(*readFile)(struct FSDriver *, Path *);
    void (*writeFile)(struct FSDriver *, Path *, uint16_t *);

    uint16_t (*entryExists)(struct FSDriver *, Path *, EntryType);
    fat_directory_listing *(*directoryListing)(struct FSDriver *, Path *);

    ide_device *ide;
    union fat_bootsector *fat_bs;
    uint16_t *fat_table;
} FSDriver;

typedef struct FSState
{
    FSDriver *current_driver;
    Path *path;
} FSState;

void fs_init(ide_device *ide);

void fs_ls(char *dir);
void fs_cd(char *dir);
void fs_cat(char *dir);
void fs_mkdir(char *dir);
void fs_write(char *dir);

char *fs_get_wd();