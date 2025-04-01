#include "fat12.h"

#include "../../../tty.h"
#include "../../../../hw/timer.h"
#include "../../../../hw/mem.h"
#include "../../../../lib/string.h"

fat_bootsector *fat_read_bs(ide_device *ide)
{
    fat_bootsector *bs = (fat_bootsector *)calloc(sizeof(fat_bootsector));

    uint16_t *bootsector = ata_28bit_pio_read_sector(*ide, 0, 1);
    for (int i = 0; i < ide->sector_size; i++)
        bs->raw[i] = bootsector[i];
    mfree(bootsector);

    return bs;
}

uint16_t *fat12_read_table(ide_device *ide, fat_bootsector *bs)
{
    uint16_t *fat = (uint16_t *)calloc(sizeof(uint16_t) * bs->ebpb.logical_sectors_per_fat * (3 * bs->ebpb.bytes_per_logical_sector) / 2);

    uint16_t *fat1sector = ata_28bit_pio_read_sector(*ide, bs->ebpb.reserved_logical_sectors, bs->ebpb.logical_sectors_per_fat);
    // uint16_t *fat2sector = ata_28bit_pio_read_sector(*ide, bs->ebpb.reserved_logical_sectors + bs->ebpb.logical_sectors_per_fat, bs->ebpb.logical_sectors_per_fat);
    uint32_t entries_per_fat = bs->ebpb.logical_sectors_per_fat * (3 * bs->ebpb.bytes_per_logical_sector) / 2;
    for (int i = 0; i < entries_per_fat; i++)
    {
        uint16_t fat_pos = (12 * i) / 8;
        uint16_t table_v = *((uint16_t *)((uint8_t *)fat1sector + fat_pos));
        uint16_t mask = (i & 1) ? table_v >> 4 : table_v & 0xFFF;

        // printf("c %x n %x\n", i, mask);
        fat[i] = mask;
    }
    mfree(fat1sector);

    return fat;
}

uint16_t *fat12_read_root(ide_device *ide, fat_bootsector *bs)
{
    uint16_t root_directory_location = bs->ebpb.reserved_logical_sectors + (bs->ebpb.logical_sectors_per_fat * bs->ebpb.num_fat_tables);
    uint16_t root_directory_sectors = (bs->ebpb.max_root_directory_entries * 32) / bs->ebpb.bytes_per_logical_sector;

    uint16_t *read = ata_28bit_pio_read_sector(*ide, root_directory_location, root_directory_sectors);
    return read;
}

uint16_t *fat12_read_data(ide_device *ide, fat_bootsector *bs, uint16_t *fat, uint16_t offset_from_root)
{
    uint16_t root_directory_location = bs->ebpb.reserved_logical_sectors + (bs->ebpb.logical_sectors_per_fat * bs->ebpb.num_fat_tables);
    uint16_t root_directory_sectors = (bs->ebpb.max_root_directory_entries * 32) / bs->ebpb.bytes_per_logical_sector;
    uint16_t first_data_sector = root_directory_location + root_directory_sectors;

    uint8_t number_of_sectors = 0;
    uint16_t current_sector = offset_from_root;
    do
    {
        number_of_sectors++;
        current_sector = fat[current_sector];
    } while (current_sector != 0xFFF);
    current_sector = offset_from_root;

    uint16_t *data = (uint16_t *)calloc(bs->ebpb.bytes_per_logical_sector * number_of_sectors * 2);
    uint8_t sectors_read = 0;
    do
    {
        uint16_t *read_data = ata_28bit_pio_read_sector(*ide, current_sector - 2 + first_data_sector, 1);
        memcpy(data + (sectors_read * bs->ebpb.bytes_per_logical_sector / 2), read_data, bs->ebpb.bytes_per_logical_sector);
        mfree(read_data);
        current_sector = fat[current_sector];
        sectors_read++;
    } while (current_sector != 0xFFF);

    return data;
}

char *fat12_assemble_filename(fat_directory_entry_standard entry)
{
    uint16_t name_length = 0;
    char *name = entry.name;
    char *ext = entry.ext;
    while (*(name++) != 0x20)
        name_length++;
    while (*(ext++) != 0x20)
        name_length++;
    char *assembed = (char *)calloc(name_length + 2);

    uint16_t i = 0;
    name = entry.name;
    while (*name != 0x20 && i < 8)
    {
        assembed[i++] = *name;
        name++;
    }

    if (!(entry.attributes & 0x10))
    {
        assembed[i++] = '.';
        ext = entry.ext;
        while (*ext != 0x20)
        {
            assembed[i++] = *ext;
            ext++;
        }
    }

    return assembed;
}


fat_directory_listing *fat12_get_directory_contents(FSDriver *driver, Entry *entry)
{
    if (entry->type == DirectoryEntry)
    {
        Path *blank = (Path *)calloc(sizeof(Path));
        uint16_t *root_data = fat12_read_root(driver->ide, driver->fat_bs);
        fat_directory_listing *root = fat12_data_to_directory_contents(driver, blank, root_data);
        mfree(root_data);
        if (entry->path->num_components == 0)
            return root;
        mfree(blank);

        uint8_t depth = 0;
        for (int i = 0; i < 256; i++)
        {
            if (depth == root->contents[i].)
            {
                // printf("FOUND\n");
                return root;
            }
            // printf("%d %s %s\n", depth, path->components[depth], current->components[depth]);

            if (strcmp(path->components[depth], current->components[depth]) == 0 && current->type == DirectoryPath)
            {
                uint16_t *data = fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, ((fat_directory_entry_standard *)current->fs_specific_header)->first_cluster_number_low);
                mfree(root);
                root = fat12_generate_directory_listing(driver, current, data);
                mfree(data);

                current = root->first;
                depth += 1;
            }

            current = current->next;
        } 

        printf("[FS] Could not find path!\n");
        return 0;
    }
}

fat_directory_listing *fat12_find_directory_of_file(FSDriver *driver, Entry *entry)
{
    Path *dir = (Path *)calloc(sizeof(Path));
    memcpy(dir, entry->path, sizeof(Path));
    dir->num_components -= 1;

    // printf("path created\n");
    fat_directory_listing *containingDir = fat12_directory_listing(driver, dir);
    // printf("directory listing obtained\n");
    Path *current = containingDir->first;
    // printf("beginning loop\n");
    while (current)
    {
        if (path->num_components == current->num_components)
        {
            uint16_t sum = 0;
            for (int i = 0; i < path->num_components; i++)
            {
                sum += strcmp(path->components[i], current->components[i]);
            }
            if (sum == 0)
            {
                mfree(containingDir);
                mfree(dir);
                return current;
            }
        }
        current = current->next;
        // printf("in loop\n");
    }
    // printf("returning 0\n");
    return 0;
}

int fat12_entry_exists(char *desired, fat_directory_listing *listing, EntryType type)
{
    for (int i = 0; i < listing->num_contents; i++)
    {
        char *name = fat12_assemble_filename(listing->contents[i]);
        if (strcmp(desired, name) == 0)
        {
            if (type == DirectoryEntry && listing->contents[i].attributes & 0x10)
                return 1;
            else if (type == FileEntry)
                return 1;
        }
    }
    return 0;
}

Entry *fat12_read_file(FSDriver *driver, Path *path)
{
    Path *list = fat12_find_listing(driver, path);
    if (list == 0)
        return 0;

    Entry *file = (Entry *)calloc(sizeof(Entry));
    file->path = path;
    file->data = (uint8_t *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, ((fat_directory_entry_standard *) list->components)->first_cluster_number_low);
    mfree(list);

    return file;
}

void fat12_write_file(FSDriver *driver, Path *path, File *file)
{
    Path *list = fat12_find_listing(driver, path);
    uint16_t *directory = fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, ((fat_directory_entry_standard *)(list->fs_specific_header))->first_cluster_number_low);
   
    return;
}

fat_directory_listing *fat12_directory_content(FSDriver *driver, Path *path, uint16_t *data)
{
    fat_directory_listing *listing = (fat_directory_listing *)calloc(sizeof(fat_directory_listing));
    memcpy(listing->raw, data, 16384);
    return listing;

    PathListing *list = (PathListing *)calloc(sizeof(PathListing));
    for (int f = 0; f < driver->fat_bs->ebpb.max_root_directory_entries; f++)
    {
        // printf("reading line\n");
        uint8_t start = ((uint8_t *)data)[(f * 16 * 2) + 0];
        if (start == 0)
            break;
        else if (start == 0xE5)
            continue;

        uint8_t attribute = ((uint8_t *)data)[(f * 16 * 2) + 11];
        if (attribute != 0x0F) // ignore longnames
        {
            Path *new = (Path *)calloc(sizeof(Path));
            if (attribute & 0x10) // directory
            {
                fat_directory_entry_standard directory;
                memcpy(directory.raw, data + (f * 16), sizeof(fat_directory_entry_standard));
    
                memcpy(new->components, path->components, 256 * sizeof(char*));
                new->num_components = path->num_components;
                new->components[new->num_components++] = fat12_assemble_filename(directory);

                memcpy(new->fs_specific_header, directory.raw, 256);
                new->type = DirectoryPath;
            }
            else // file
            {
                fat_directory_entry_standard file;
                memcpy(file.raw, data + (f * 16), sizeof(fat_directory_entry_standard));
    
                memcpy(new->components, path->components, 256 * sizeof(char*));
                new->num_components = path->num_components;
                new->components[new->num_components++] = fat12_assemble_filename(file);
                
                memcpy(new->fs_specific_header, file.raw, 256);
                new->type = FilePath;
            }

            Path *current = list->first;
            if (!current)
                list->first = new;
            else
            {
                while (current->next)
                    current = current->next;
                current->next = new;
            }
        }
    }
    return list;
}

void fat12_free_path(Path* path)
{
    // for (int i = 0; i < path->num_components; i++)
    //     mfree(path->components[i]);
    mfree(path);
}

void fat12_free_listing(FSDriver *driver, PathListing *list)
{
    Path *current = list->first;
    Path *old;
    while (current)
    {
        old = current;
        current = current->next;
        fat12_free_path(old);
        // printf("%p\n", old);
    }
    mfree(list);
}

FSDriver *fat12_init_driver(ide_device *ide)
{
    FSDriver *driver = (FSDriver *)calloc(sizeof(FSDriver));

    driver->ide = ide;

    driver->readFile = fat12_read_file;
    driver->writeFile = fat12_write_file;

    driver->directoryListing = fat12_directory_listing;
    driver->freePathListing = fat12_free_listing;

    driver->entryExists = fat12_exists;

    driver->fat_bs = fat_read_bs(driver->ide);
    driver->fat_table = fat12_read_table(driver->ide, driver->fat_bs);

    return driver;
}