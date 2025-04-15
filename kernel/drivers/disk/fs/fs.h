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

typedef enum EntryType
{
    FileEntry,
    DirectoryEntry,
} EntryType;

typedef enum FileInfoType
{
    FileInfoType_Nonexistent,
    FileInfoType_Standard,
    FileInfoType_RootDir,
} FileInfoType;

typedef struct FileInfo
{
    char *name;
    // char *ext;
    EntryType type;
    uint32_t size;

    date_t creation_date;
    time_t creation_time;
} FileInfo;

typedef struct DirectoryListing
{
    FileInfo info;
    struct DirectoryListing *next;
} DirectoryListing;

typedef struct FSDriver
{
    uint16_t *(*readFile)(struct FSDriver *, Path *);
    void (*writeFile)(struct FSDriver *, Path *, uint16_t *, size_t);
    
    int (*createDirectory)(struct FSDriver *, Path *);
    int (*createFile)(struct FSDriver *, Path *);

    int (*removeDirectory)(struct FSDriver *, Path *);
    int (*removeFile)(struct FSDriver *, Path *);

    FileInfoType (*fileInfo)(struct FSDriver *, Path *, EntryType, FileInfo **);
    DirectoryListing *(*directoryListing)(struct FSDriver *, Path *);

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
void fs_touch(char *dir);
void fs_rm(char *dir);
void fs_rmdir(char *dir);

void fs_fat();
char *fs_get_wd();