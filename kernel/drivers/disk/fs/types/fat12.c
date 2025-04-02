#include "fat12.h"

#include "../../../tty.h"
#include "../../../../hw/timer.h"
#include "../../../../hw/mem.h"
#include "../../../../lib/string.h"

fat_bootsector *fat12_read_bs(ide_device *ide)
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

void fat12_parse_filename(char *dest, const char *fat_name)
{
    int name_len = 8;
    int ext_len = 3;

    while (name_len > 0 && fat_name[name_len - 1] == ' ')
        name_len--;
    while (ext_len > 0 && fat_name[8 + ext_len - 1] == ' ')
        ext_len--;

    memcpy((void *)dest, (void *)fat_name, name_len);

    if (ext_len > 0)
    {
        dest[name_len] = '.';
        memcpy((void *)(dest + name_len + 1), (void *)(fat_name + 8), ext_len);
        dest[name_len + 1 + ext_len] = '\0';
    }
    else
    {
        dest[name_len] = '\0';
    }

    strlower(dest);
}

uint16_t *fat12_read_file(FSDriver *driver, Path *path)
{
    uint16_t sector_location = fat12_get_data_sector(driver, path, FileEntry);
    if (sector_location)
    {
        uint16_t *data = fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, sector_location);
        return data;
    }
    return 0;
}

void fat12_write_file(FSDriver *driver, Path *path, uint16_t *data)
{
}

fat_directory_listing *fat12_list_directory(FSDriver *driver, Path *path)
{
    fat_directory_listing *root_data = (fat_directory_listing *)fat12_read_root(driver->ide, driver->fat_bs);
    if (path->count == 0)
    {
        return root_data;
    }
    else
    {
        Path *search = (Path *)calloc(sizeof(Path));
        path_init(search);

        int depth = 0;
        for (int i = 0; i < driver->fat_bs->ebpb.max_root_directory_entries; i++)
        {
            if ((root_data->contents[i].raw[0] & 0xFF) == 0x00)
                break; // no more listings in this sector
            if ((root_data->contents[i].raw[0] & 0xFF) == 0xE5)
                continue; // listing is unused

            if (root_data->contents[i].attributes & 0x10)
            { // directory
                char fs_name[13] = {0};
                char in_name[13] = {0};

                fat12_parse_filename(fs_name, root_data->contents[i].name);
                memcpy(in_name, path->components[depth], strlen(path->components[depth]));
                strlower(in_name);

                // printf("%s %s\n", fs_name, in_name);
                if (strcmp(fs_name, in_name) == 0)
                {
                    size_t sector_location = root_data->contents[i].first_cluster_number_low;
                    fat12_free_listing(root_data);
                    root_data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, sector_location);
                    depth++;
                    i = 0;

                    path_add(search, in_name);
                    char *search_string = path_join(search);
                    char *final_string = path_join(path);
                    strlower(search_string);
                    strlower(final_string);
                    if (strcmp(search_string, final_string) == 0)
                    {
                        mfree(search_string);
                        mfree(final_string);

                        return root_data;
                    }
                }
            }
        }
    }
    return 0;
}

void fat12_free_listing(fat_directory_listing *toFree)
{

}

uint16_t fat12_get_data_sector(FSDriver *driver, Path *path, EntryType type)
{
    Path *parent = path_get_parent(path);
    fat_directory_listing *parent_dir = fat12_list_directory(driver, parent);

    for (int i = 0; i < 2048; i++)
    {
        if ((parent_dir->contents[i].raw[0] & 0xFF) == 0x00)
            break; // no more listings in this sector
        if ((parent_dir->contents[i].raw[0] & 0xFF) == 0xE5)
            continue; // listing is unused

        if (type == DirectoryEntry ? (parent_dir->contents[i].attributes & 0x10) : !(parent_dir->contents[i].attributes & 0x10))
        { // ensure we're looking at the right type
            char fs_name[13] = {0};
            char in_name[13] = {0};

            fat12_parse_filename(fs_name, parent_dir->contents[i].name);
            memcpy(in_name, path->components[path->count - 1], strlen(path->components[path->count - 1]));
            strlower(in_name);

            if (strcmp(fs_name, in_name) == 0)
            {
                fat12_free_listing(parent_dir);
                path_free(parent);
                return parent_dir->contents[i].first_cluster_number_low;
            }
        }
    }

    fat12_free_listing(parent_dir);
    path_free(parent);
    return 0;
}

FSDriver *fat12_init_driver(ide_device *ide)
{
    FSDriver *driver = (FSDriver *)calloc(sizeof(FSDriver));

    driver->ide = ide;

    driver->readFile = fat12_read_file;
    driver->writeFile = fat12_write_file;

    driver->directoryListing = fat12_list_directory;
    driver->entryExists = fat12_get_data_sector;

    // initialize bootsector and fat tables
    driver->fat_bs = fat12_read_bs(driver->ide);
    driver->fat_table = fat12_read_table(driver->ide, driver->fat_bs);

    return driver;
}