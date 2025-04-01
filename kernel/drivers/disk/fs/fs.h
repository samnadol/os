#pragma once

#include "../ata.h"

typedef enum PathType
{
    FilePath,
    DirectoryPath,
} PathType;

typedef struct File
{
    uint8_t *data;

    struct Path *path;
    struct File *next; // optional, for use with FileListing
} File;

typedef struct PathListing
{
    struct Path *first;
} PathListing;

typedef struct Path
{
    char *components[256];
    uint8_t num_components;

    uint16_t fs_specific_header[256];
    PathType type;

    struct Path *next; // used in directorylisting
} Path;

typedef struct FSDriver
{
    File *(*readFile)(struct FSDriver *, Path *);
    void (*writeFile)(struct FSDriver *, Path *, File *);

    PathListing *(*directoryListing)(struct FSDriver *, Path *);
    void (*freePathListing)(struct FSDriver *, PathListing *);

    int (*fileExists)(struct FSDriver *, Path *);
    int (*directoryExists)(struct FSDriver *, Path *);

    ide_device *ide;
    union FAT_BS *fat_bs;
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

char *fs_get_wd();