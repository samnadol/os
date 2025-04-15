#include "fs.h"

#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../../../lib/time.h"
#include "../../../lib/string.h"
#include "types/fat12.h"

FSState *state;

char *fs_get_wd()
{
    return path_join(state->path);
}

void fs_init(ide_device *ide)
{
    if (!state)
    {
        state = (FSState *)calloc(sizeof(FSState));
        state->current_driver = fat12_init_driver(ide);

        state->path = (Path *)calloc(sizeof(Path));
        path_init(state->path);
    }
}

void fs_ls(char *dir)
{
    if (!state || !state->current_driver)
        return;

    Path *path = (strlen(dir) > 0 ? path_split(dir) : state->path);
    DirectoryListing *listing = state->current_driver->directoryListing(state->current_driver, path);
    DirectoryListing *startListing = listing;
    // printf("dir listing %x\n", listing);

    if (listing)
    {
        while (listing)
        {
            if (listing->info.type == DirectoryEntry)
            {
                printf("~%s\n", listing->info.name);
            }
            listing = listing->next;
        }
        while (startListing)
        {
            if (startListing->info.type != DirectoryEntry)
            {
                printf("%s\n", startListing->info.name);
            }
            //     if (startListing->info.type != DirectoryEntry)
            //     {
            //         printf("%s (%f) (%d)\n", startListing->info.name, startListing->info.size, make_time(startListing->info.creation_date.year, startListing->info.creation_date.month, startListing->info.creation_date.day, startListing->info.creation_time.hour, startListing->info.creation_time.minute, startListing->info.creation_time.second));
            //     }
            startListing = startListing->next;
        }
        // printf("\n");
    }
    else
    {
        printf("ls: no such directory\n");
    }
}

void fs_cd(char *dir)
{
    if (!state || !state->current_driver)
        return;

    if (strlen(dir) == 0)
    {
        return;
    }
    else if (strcmp(dir, ".") == 0)
    {
        return;
    }
    else if (strcmp(dir, "..") == 0)
    {
        path_remove_last(state->path);
        return;
    }
    else
    {
        Path *final = path_get_final(state->path, dir);
        FileInfo *info = (FileInfo *)calloc(sizeof(FileInfo));
        FileInfoType exists = state->current_driver->fileInfo(state->current_driver, final, DirectoryEntry, &info);
        if ((exists == FileInfoType_Standard && info->type == DirectoryEntry) || exists == FileInfoType_RootDir)
        {
            path_free(state->path);
            state->path = final;
        }
        else
        {
            path_free(final);
            printf("cd: no such directory\n");
        }
    }
}

void fs_cat(char *dir)
{
    if (!state || !state->current_driver)
        return;

    if (strlen(dir) == 0)
        return;

    Path *final = path_get_final(state->path, dir);
    // printf("%s\n", path_join(final));

    FileInfo *info = (FileInfo *)calloc(sizeof(FileInfo));
    int exists = state->current_driver->fileInfo(state->current_driver, final, FileEntry, &info);

    uint16_t *data = state->current_driver->readFile(state->current_driver, final);
    if (data)
    {
        for (int i = 0; i < info->size; i++)
        {
            printf("%c", ((uint8_t *)data)[i]);
            // if ((i+1) % 64 == 0)
            // {
            //     printf("\n");
            // }
        }
        printf("\n");
    }
    else
    {
        printf("cat: no such file\n");
    }

    path_free(final);
}

void fs_mkdir(char *dir)
{
    if (!state || !state->current_driver)
        return;

    Path *final = path_get_final(state->path, dir);
    int result = state->current_driver->createDirectory(state->current_driver, final);
    if (!result)
        printf("mkdir: failed!\n");
}

void fs_touch(char *dir)
{
    if (!state || !state->current_driver)
        return;

    Path *final = path_get_final(state->path, dir);
    int result = state->current_driver->createFile(state->current_driver, final);
    if (!result)
        printf("touch: failed!\n");

    // int num_repeats = 40;
    // char *text = "this was written from the os \n";
    // uint16_t *text_data = (uint16_t *)calloc(sizeof(uint16_t) * strlen(text) * num_repeats);
    // for (int i = 0; i < num_repeats; i++)
    //     memcpy((uint8_t *)(text_data + (i * strlen(text) / 2)), text, strlen(text));
    // state->current_driver->writeFile(state->current_driver, final, text_data, strlen(text) * num_repeats);
    // mfree(text_data);

    uint16_t arr[4] = { '1', '2', '3', '\0' };
    state->current_driver->writeFile(state->current_driver, final, arr, 4 * sizeof(uint16_t));
}

void fs_rm(char *dir)
{ // remove file
    if (!state || !state->current_driver)
        return;

    Path *final = path_get_final(state->path, dir);
    int result = state->current_driver->removeFile(state->current_driver, final);
    if (!result)
        printf("rm: failed!\n");
    path_free(final);
}

void fs_rmdir(char *dir)
{ // remove dir
    if (!state || !state->current_driver)
        return;

    Path *final = path_get_final(state->path, dir);
    int result = state->current_driver->removeDirectory(state->current_driver, final);
    if (!result)
        printf("rmdir: failed!\n");
    path_free(final);
}

void fs_fat()
{
    // print fat table
    printf("      [0] [1] [2] [3] [4] [5] [6] [7] [8] [9] [A] [B] [C] [D] [E] [F]\n");
    for (int i = 0; i < 128; i++)
    {
        printf("[%2xx] ", i);
        for (int j = 0; j < 16; j++)
        {
            printf("%3x ", state->current_driver->fat_table[(i * 16) + j]);
        }
        printf("\n");
    }
}