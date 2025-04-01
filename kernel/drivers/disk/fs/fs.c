#include "fs.h"

#include "../../../hw/mem.h"
#include "../../../lib/string.h"
#include "types/fat12.h"

FSState *state;
char fs_buf[256];

char *fs_get_wd()
{
    int i;
    for (i = 0; i < 256; i++)
        fs_buf[i] = 0;

    i = 0;
    fs_buf[i++] = '/';
    for (int j = 0; j < state->wd->num_components; j++)
    {
        char *comp = state->wd->components[j];
        while (*comp)
        {
            fs_buf[i++] = *comp++;
        }
        fs_buf[i++] = '/';
    }

    return fs_buf;
}

void fs_read(FSDriver *driver, Path *path)
{
    Entry *file = driver->readFile(driver, path);
    printf("%s\n", file->data);

    mfree(file->data);
    mfree(file);
}

void fs_init(ide_device *ide)
{
    if (!state)
    {
        state = (FSState *)calloc(sizeof(FSState));
        state->current_driver = fat12_init_driver(ide);

        Path *path = (Path *)calloc(sizeof(Path));
        path->num_components = 0;
        state->wd = path;
    }
}

void fs_ls(char *dir)
{
    Entry *list = state->current_driver->directoryListing(state->current_driver, state->wd);
    if (list <= 0)
    {
        printf("[FS] An error occurred!\n");
    }
    else
    {
        Path *current = list;
        while (current)
        {
            printf("%c%s ", ' ', current->components[current->num_components - 1]);
            current = current + sizeof(Path);
        }
        printf("\n");
    }
}

void fs_cat(char *dir)
{
    char *new_component = (char *)calloc(strlen(dir) + 1);
    memcpy(new_component, dir, strlen(dir) + 1);

    strupper(new_component);
    state->wd->components[state->wd->num_components] = new_component;
    state->wd->num_components++;

    if (!state->current_driver->entryExists(state->current_driver, state->wd, FileEntry))
    {
        printf("%s: no such file\n", dir);
    }
    else
    {
        Entry *file = state->current_driver->readFile(state->current_driver, state->wd);
        size_t len = strlen((char*)file->data);
        for (int i = 0; i < (len + 128) / 128; i++)
        {
            for (int j = 0; j < 128; j++)
            {
                printf("%c", file->data[(i*128)+j]);
            }
            printf("\n");
        }
    }

    mfree(new_component);
    state->wd->num_components--;
}

void fs_cd(char *dir)
{
    if (!strcmp(dir, "."))
        return;

    if (!strcmp(dir, ".."))
    {
        if (state->wd->num_components > 0)
        {
            state->wd->num_components--;
        }
    }
    else
    {
        char *new_component = (char *)calloc(strlen(dir) + 1);
        memcpy(new_component, dir, strlen(dir) + 1);

        strupper(new_component);
        state->wd->components[state->wd->num_components] = new_component;
        state->wd->num_components++;

        if (!state->current_driver->entryExists(state->current_driver, state->wd, DirectoryEntry))
        {
            printf("%s: no such directory\n", dir);
            mfree(new_component);
            state->wd->num_components--;
        }
    }
}

void fs_mkdir(char *dir)
{

}

void fs_write(char *dir)
{
    Path *p = (Path *)calloc(sizeof(Path));
    p->num_components = 1;
    p->components[0] = "written.txt";

    File *f = (File *)calloc(sizeof(File));
    f->path = p;
    f->data = (uint8_t *)"this was written from the os";

    state->current_driver->writeFile(state->current_driver, p, f);
}