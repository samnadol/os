#include "fs.h"

#include "../../../hw/mem.h"
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
    Path *path = (strlen(dir) > 0 ? path_split(dir) : state->path);
    fat_directory_listing *listing = (fat_directory_listing *)state->current_driver->directoryListing(state->current_driver, path);

    if (listing)
    {
        for (int i = 0; i < 512; i++)
        {
            if ((listing->contents[i].raw[0] & 0xFF) == 0x00)
                break; // no more listings in this sector
            if ((listing->contents[i].raw[0] & 0xFF) == 0xE5)
                continue; // listing is unused

            char fs_name[13] = {0};
            if (listing->contents[i].attributes & 0x10)
            { // directory
                fat12_parse_filename(fs_name, listing->contents[i].name);
                printf("~%s ", fs_name);
            }
            else if (listing->contents[i].attributes & 0x0F)
            { // long file name, ignore for now
            }
            else
            { // normal file
                fat12_parse_filename(fs_name, listing->contents[i].name);
                printf("%s ", fs_name);
            }
        }
        printf("\n");
    }
    else
    {
        printf("ls: no such directory\n");
    }
}

void fs_cat(char *dir)
{
    if (strlen(dir) == 0)
        return;

    Path *final = path_get_final(state->path, dir);

    fat_directory_entry_standard *info;
    int exists = state->current_driver->fileInfo(state->current_driver, final, FileEntry, &info);

    uint16_t *data = state->current_driver->readFile(state->current_driver, final);
    if (data)
    {
        for (int i = 0; i < info->file_size_bytes; i++)
            printf("%c", ((uint8_t *)data)[i]);
        printf("\n");
    }
    else
    {
        printf("cat: no such file\n");
    }

    path_free(final);
}

void fs_cd(char *dir)
{
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
        fat_directory_entry_standard *info;
        int exists = state->current_driver->fileInfo(state->current_driver, final, DirectoryEntry, &info);
        if (info->first_cluster_number_low)
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

void fs_mkdir(char *dir)
{
    Path *final = path_get_final(state->path, dir);
    int result = state->current_driver->createDirectory(state->current_driver, final);
    if (!result)    
        printf("mkdir: failed!\n");
}

void fs_touch(char *dir)
{
    Path *final = path_get_final(state->path, dir);
    int result = state->current_driver->createFile(state->current_driver, final);
    if (!result)    
        printf("touch: failed!\n");
}

void fs_rm(char *dir)
{ // remove file

}

void fs_rmd(char *dir)
{ // remove dir

}