#include "fs.h"

#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
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
    // printf("dir listing %x\n", listing);

    if (listing)
    {
        while (listing)
        {
            printf("%c%s ", listing->info.type == DirectoryEntry ? '~' : ' ', listing->info.name);
            listing = listing->next;
        }
        printf("\n");
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
        
    // if (strlen(dir) == 0)
    // {
    //     return;
    // }
    // else if (strcmp(dir, ".") == 0)
    // {
    //     return;
    // }
    // else 
    if (strcmp(dir, "..") == 0)
    {
        path_remove_last(state->path);
        return;
    }
    else
    {
        Path *final = path_get_final(state->path, dir);
        FileInfo *info = (FileInfo *)calloc(sizeof(FileInfo));
        FileInfoType exists = state->current_driver->fileInfo(state->current_driver, final, DirectoryEntry, &info);
        if ((exists == FileInfoType_Standard) || exists == FileInfoType_RootDir)
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

    FileInfo *info = (FileInfo *)calloc(sizeof(FileInfo));
    int exists = state->current_driver->fileInfo(state->current_driver, final, FileEntry, &info);

    uint16_t *data = state->current_driver->readFile(state->current_driver, final);
    if (data)
    {
        for (int i = 0; i < info->size; i++)
            printf("%c", ((uint8_t *)data)[i]);
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

    uint16_t arr[3] = { 1, 2, 3 };
    state->current_driver->writeFile(state->current_driver, final, arr, 3);
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