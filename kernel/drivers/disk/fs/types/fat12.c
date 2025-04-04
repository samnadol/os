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
    uint32_t entries_per_fat = bs->ebpb.logical_sectors_per_fat * (3 * bs->ebpb.bytes_per_logical_sector) / 2;
    uint16_t *fat = (uint16_t *)calloc(sizeof(uint16_t) * entries_per_fat);

    uint16_t *fat1sector = ata_28bit_pio_read_sector(*ide, bs->ebpb.reserved_logical_sectors, bs->ebpb.logical_sectors_per_fat);
    // uint16_t *fat2sector = ata_28bit_pio_read_sector(*ide, bs->ebpb.reserved_logical_sectors + bs->ebpb.logical_sectors_per_fat, bs->ebpb.logical_sectors_per_fat);

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

int fat12_write_table(ide_device *ide, fat_bootsector *bs, uint16_t* fat_write)
{
    uint32_t size_of_fat = sizeof(uint16_t) * bs->ebpb.logical_sectors_per_fat * bs->ebpb.bytes_per_logical_sector;
    uint16_t *data = (uint16_t *)calloc(size_of_fat);

    for (int i = 0; i < (sizeof(uint16_t) * bs->ebpb.logical_sectors_per_fat * bs->ebpb.bytes_per_logical_sector); i += 3)
    { // iterates through individual bytes of disk data
        uint16_t fat_pos = (i * 8) / 12;
        ((uint8_t *)data)[i] = fat_write[fat_pos] & 0xFF;
        ((uint8_t *)data)[i + 1] = ((fat_write[fat_pos] & 0xF00) >> 8) | ((fat_write[fat_pos + 1] & 0xF) << 4);
        ((uint8_t *)data)[i + 2] = fat_write[fat_pos + 1] >> 4;
    }

    ata_28bit_pio_write_sector(*ide, bs->ebpb.reserved_logical_sectors, bs->ebpb.logical_sectors_per_fat, data, size_of_fat);
    // ata_28bit_pio_write_sector(*ide, bs->ebpb.reserved_logical_sectors + bs->ebpb.logical_sectors_per_fat, bs->ebpb.logical_sectors_per_fat, data, size_of_fat);

    return 1;
}

// returns data read from first cluster location, looking up subsequent clusters in the FAT
uint16_t *fat12_read_data(ide_device *ide, fat_bootsector *bs, uint16_t *fat, int first_cluster)
{
    int root_directory_location = bs->ebpb.reserved_logical_sectors + (bs->ebpb.logical_sectors_per_fat * bs->ebpb.num_fat_tables);
    int root_directory_sectors = (bs->ebpb.max_root_directory_entries * 32) / bs->ebpb.bytes_per_logical_sector;

    if (first_cluster == -1)
    {
        uint16_t *read = ata_28bit_pio_read_sector(*ide, root_directory_location, root_directory_sectors);
        return read;
    }
    else
    {
        int first_data_sector = root_directory_location + root_directory_sectors;
        int number_of_clusters = 0;
        int current_cluster = first_cluster;

        do
        {
            number_of_clusters++;
            current_cluster = fat[current_cluster];
        } while (current_cluster < 0xFF8);
        current_cluster = first_cluster;

        // for (int i = 0; i < 10; i++)
        // {
        //     for (int j = 0; j < 16; j++)
        //         printf("%x ", data[(i * 16) + j]);
        //     printf("\n");
        // }

        int sectors_read = 0;
        uint16_t *data = (uint16_t *)calloc(bs->ebpb.bytes_per_logical_sector * number_of_clusters * bs->ebpb.logical_sectors_per_cluster * 2);
        do
        {
            uint16_t *read_data = ata_28bit_pio_read_sector(*ide, current_cluster - 2 + first_data_sector, bs->ebpb.logical_sectors_per_cluster);
            memcpy(data + (sectors_read * bs->ebpb.bytes_per_logical_sector / 2), read_data, bs->ebpb.bytes_per_logical_sector);
            mfree(read_data);
            current_cluster = fat[current_cluster];
            sectors_read++;
        } while (current_cluster < 0xFF8);

        return data;
    }
}

// returns first cluster location of data written
int fat12_write_data(ide_device *ide, fat_bootsector *bs, uint16_t *fat, uint16_t *data, size_t size, int start_cluster)
{
    int bytes_per_cluster = bs->ebpb.bytes_per_logical_sector * bs->ebpb.logical_sectors_per_cluster;
    int root_directory_sectors = (bs->ebpb.max_root_directory_entries * 32) / bs->ebpb.bytes_per_logical_sector;
    int root_directory_location = bs->ebpb.reserved_logical_sectors + (bs->ebpb.logical_sectors_per_fat * bs->ebpb.num_fat_tables);
    uint16_t first_data_sector = root_directory_location + root_directory_sectors;

    int clusters_needed = (size + (bytes_per_cluster - 1)) / bytes_per_cluster;
    int entries_per_fat = (bs->ebpb.logical_sectors_per_fat * (3 * bs->ebpb.bytes_per_logical_sector / 12));

    if (start_cluster == 0)
    { // find and allocate start cluster
        for (size_t i = 0; i < entries_per_fat; i++)
        {
            if (fat[i] == 0)
            {
                start_cluster = i;
                break;
            }
        }
        
        // printf("[fat] writing data to cluster %d of size %d\n", start_cluster, size);

        int last_cluster;
        int current_cluster = start_cluster;
        for (int i = 0; i < clusters_needed; i++)
        {
            // printf("(new) writing at cluster %d\n", current_cluster);

            ata_28bit_pio_write_sector(*ide, first_data_sector - 2 + current_cluster, bs->ebpb.logical_sectors_per_cluster, &data[(bs->ebpb.bytes_per_logical_sector) * i], size > bs->ebpb.bytes_per_logical_sector ? bs->ebpb.bytes_per_logical_sector : size);
            size -= bs->ebpb.bytes_per_logical_sector;
            
            last_cluster = current_cluster;
            for (size_t i = 0; i < entries_per_fat; i++)
            {
                if (fat[i] == 0 && i != last_cluster)
                {
                    current_cluster = i;
                    break;
                }
            }
            fat[last_cluster] = current_cluster;
        }
        fat[current_cluster] = 0xFFF;
    }
    else if (start_cluster == -1)
    { // writing in root dir
        // printf("[fat] writing data to cluster %d of size %d\n", root_directory_location, size);
        ata_28bit_pio_write_sector(*ide, root_directory_location, root_directory_sectors, data, size);
        return -1;
    }
    else
    { // overwriting something already existing
        // printf("[fat] writing data to cluster %d of size %d\n", start_cluster, size);

        int last_cluster;
        int current_cluster = start_cluster;
        for (int i = 0; i < clusters_needed; i++)
        {
            // printf("(overwrite) writing at cluster %d\n", current_cluster);

            ata_28bit_pio_write_sector(*ide, first_data_sector - 2 + current_cluster, bs->ebpb.logical_sectors_per_cluster, &data[(bs->ebpb.bytes_per_logical_sector) * i], size > bs->ebpb.bytes_per_logical_sector ? bs->ebpb.bytes_per_logical_sector : size);
            size -= bs->ebpb.bytes_per_logical_sector;
            
            last_cluster = current_cluster;
            if (fat[i] < 0xFF8 && fat[i] != 0x000)
            {
                current_cluster = fat[i];
            }
            else
            {
                for (size_t i = 0; i < entries_per_fat; i++)
                {
                    if (fat[i] == 0 && i != last_cluster)
                    {
                        current_cluster = i;
                        break;
                    }
                }
            }
            fat[last_cluster] = current_cluster;
        }
        last_cluster = fat[current_cluster];
        fat[current_cluster] = 0xFFF;

        // free rest of FAT chain if used less than needed
        while (last_cluster < 0xFF8 && last_cluster != 0x000)
        {
            last_cluster = fat[last_cluster];
            fat[last_cluster] = 0x000;
        }
    }

    fat12_write_table(ide, bs, fat);

    return start_cluster;
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
    fat_directory_entry_standard *info;
    int exists = fat12_get_file_info(driver, path, FileEntry, &info);
    if (exists && info->first_cluster_number_low)
    {
        uint16_t *data = fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, info->first_cluster_number_low);
        return data;
    }
    return 0;
}

void fat12_write_file(FSDriver *driver, Path *path, uint16_t *data)
{
}

fat_directory_listing *fat12_list_directory(FSDriver *driver, Path *path)
{
    fat_directory_listing *root_data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, -1);
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

int fat12_create_directory(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    int file_exists = fat12_get_file_info(driver, path, DirectoryEntry, &file_info);
    if (file_exists)
        return 0;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    int parent_exists = fat12_get_file_info(driver, parent, DirectoryEntry, &parent_info);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_info->first_cluster_number_low);

    int entry_loc;
    for (entry_loc = 0; entry_loc < 2048; entry_loc++)
    {
        if ((data->contents[entry_loc].raw[0] & 0xFF) == 0x00)
        { // no more listings in this sector
            break;
        }
        if ((data->contents[entry_loc].raw[0] & 0xFF) == 0xE5)
        { // listing is unused
            break;
        }
    }
    // entry_loc--;
    
    // write data to sectors
    fat_directory_listing *new_data = (fat_directory_listing *)calloc(sizeof(fat_directory_listing));
    memcpy(new_data->contents[0].name, ".          ", 11);
    memcpy(new_data->contents[1].name, "..         ", 11);
    new_data->contents[0].attributes |= 0x10;
    new_data->contents[1].attributes |= 0x10;
    int data_sector = fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, new_data->raw, sizeof(fat_directory_entry_standard) * 2, 0);

    // create and write descriptor to entry
    memcpy(data->contents[entry_loc].name, "        ", 8);
    memcpy(data->contents[entry_loc].name, path->components[path->count - 1], strlen(path->components[path->count - 1]));
    memcpy(data->contents[entry_loc].ext, "   ", 3);
    data->contents[entry_loc].attributes |= 0x10;
    data->contents[entry_loc].first_cluster_number_low = data_sector;
    data->contents[entry_loc].first_cluster_number_high = 0;

    int i = 0;
    while (data->raw[i] != 0)
    {
        i += sizeof(fat_directory_entry_standard);
    }

    fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, data->raw, i, parent_info->first_cluster_number_low);

    return 1;
}

int fat12_create_file(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    int file_exists = fat12_get_file_info(driver, path, FileEntry, &file_info);
    if (file_exists)
        return 0;
        
    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    int parent_exists = fat12_get_file_info(driver, parent, DirectoryEntry, &parent_info);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_info->first_cluster_number_low);

    int entry_loc;
    for (entry_loc = 0; entry_loc < 2048; entry_loc++)
    {
        if ((data->contents[entry_loc].raw[0] & 0xFF) == 0x00)
        { // no more listings in this sector
            break;
        }
        if ((data->contents[entry_loc].raw[0] & 0xFF) == 0xE5)
        { // listing is unused
            break;
        }
    }

    char *text = "this was written from the os";
    uint16_t *text_data = (uint16_t *)calloc(sizeof(uint16_t) * strlen(text));
    memcpy((uint8_t *)text_data, text, strlen(text));

    int data_sector = fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, text_data, strlen(text), 0);

    // create and write descriptor to entry
    memcpy(data->contents[entry_loc].name, "        ", 8);
    memcpy(data->contents[entry_loc].name, path->components[path->count - 1], strlen(path->components[path->count - 1]));
    memcpy(data->contents[entry_loc].ext, "TXT", 3);
    data->contents[entry_loc].attributes |= 0x00;
    data->contents[entry_loc].first_cluster_number_high = 0;
    data->contents[entry_loc].first_cluster_number_low = data_sector;
    data->contents[entry_loc].file_size_bytes = strlen(text);

    int i = 0;
    while (data->raw[i] != 0)
    {
        i += sizeof(fat_directory_entry_standard);
    }

    fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, data->raw, i, parent_info->first_cluster_number_low);
    return 1;
}

int fat12_get_file_info(FSDriver *driver, Path *path, EntryType type, fat_directory_entry_standard **output)
{
    char *path_string = path_join(path);
    if (strcmp(path_string, "/") == 0)
    {
        mfree(path_string);
        return 0;
    }
    mfree(path_string);

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
                *output = &parent_dir->contents[i];
                return 1;
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

    driver->createDirectory = fat12_create_directory;
    driver->createFile = fat12_create_file;

    driver->directoryListing = fat12_list_directory;
    driver->fileInfo = fat12_get_file_info;

    // initialize bootsector and fat tables
    driver->fat_bs = fat12_read_bs(driver->ide);
    driver->fat_table = fat12_read_table(driver->ide, driver->fat_bs);

    return driver;
}