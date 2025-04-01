#pragma once

#include "../ata.h"


typedef enum EntryType
{
    FileEntry,
    DirectoryEntry,
} EntryType;

typedef struct Path
{
    char *components[256];
    uint8_t num_components;
} Path;

typedef struct Entry
{
    uint8_t *data;

    EntryType type;
    Path *path;
} Entry;

typedef struct FSDriver
{
    Entry *(*readFile)(struct FSDriver *, Path *);
    void (*writeFile)(struct FSDriver *, Path *, Entry *);

    Entry *(*directoryListing)(struct FSDriver *, Path *);

    int (*entryExists)(struct FSDriver *, Path *, EntryType);

    ide_device *ide;
    union fat_bootsector *fat_bs;
    uint16_t *fat_table;
} FSDriver;

typedef struct FSState
{
    FSDriver *current_driver;
    Path *wd;
} FSState;

void fs_init(ide_device *ide);

void fs_ls(char *dir);
void fs_cd(char *dir);
void fs_cat(char *dir);
void fs_mkdir(char *dir);
void fs_write(char *dir);

char *fs_get_wd();