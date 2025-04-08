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

int fat12_write_table(ide_device *ide, fat_bootsector *bs, uint16_t *fat_write)
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
    ata_28bit_pio_write_sector(*ide, bs->ebpb.reserved_logical_sectors + bs->ebpb.logical_sectors_per_fat, bs->ebpb.logical_sectors_per_fat, data, size_of_fat);

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

    if (start_cluster == -1)
    { // writing in root dir
        ata_28bit_pio_write_sector(*ide, root_directory_location, root_directory_sectors, data, size);
        return -1;
    }
    else
    {
        int i = 0;
        int current_cluster;
        int clusters_found = 0;
        int *clusters = (int *)calloc(sizeof(int) * clusters_needed);
        if (start_cluster != 0)
        {
            current_cluster = start_cluster;
            if (clusters_needed > 0)
            {
                do
                {
                    clusters[clusters_found++] = current_cluster;
                    current_cluster = fat[current_cluster];
                } while ((current_cluster < 0xFF8) && (clusters_found < clusters_needed));
            }

            if (current_cluster < 0xFF8)
            {
                int next_cluster;
                do
                {
                    next_cluster = fat[current_cluster];
                    fat[current_cluster] = 0x000;
                    current_cluster = next_cluster;
                } while (current_cluster < 0xFF8);
            }

            printf("found %d clusters, %d needed\n", clusters_found, clusters_needed);

            while (clusters_found < clusters_needed)
            {
                while (fat[i] != 0)
                    i++;
                clusters[clusters_found++] = i;
                current_cluster = i++;
            }
        }
        else
        {
            while (clusters_found < clusters_needed)
            {
                while (fat[i] != 0)
                    i++;
                clusters[clusters_found++] = i;
                current_cluster = i++;
            }
        }

        for (int cluster = 0; cluster < clusters_needed - 1; cluster++)
        {
            fat[clusters[cluster]] = clusters[cluster + 1];
        }
        fat[clusters[clusters_needed - 1]] = 0xFFF;

        for (int i = 0; i < clusters_needed; i++)
        {
            // printf("writing cluster %d %d %s\n", first_data_sector - 2 + clusters[i], size, data + ((bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector / 2) * i));
            ata_28bit_pio_write_sector(
                *ide,
                first_data_sector - 2 + clusters[i],
                bs->ebpb.logical_sectors_per_cluster,
                data + ((bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector / 2) * i),
                size > bs->ebpb.bytes_per_logical_sector ? (bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector) : size);
            size -= (bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector);
        }

        fat12_write_table(ide, bs, fat);
        return clusters[0];
    }
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
    int exists = fat12_get_fat_file_info(driver, path, FileEntry, &info);
    if (exists && info->first_cluster_number_low)
    {
        uint16_t *data = fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, info->first_cluster_number_low);
        return data;
    }
    return 0;
}

fat_directory_listing *fat12_get_directory_sector(FSDriver *driver, Path *path)
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

void fat12_write_directory(FSDriver *driver, fat_bootsector *bs, uint16_t *fat, fat_directory_listing *data, int cluster)
{

    int i = 0;
    while (data->contents[i].name[0] != 0)
    {
        printf("%x\n", data->contents[i].name[0]);
        i += 1;
    }
    printf("%x\n", data->contents[i].name[0]);

    size_t table_size = i * sizeof(fat_directory_entry_standard);
    printf("writing %d bytes\n", table_size);

    fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, data->raw, table_size, cluster);
}

void fat12_free_listing(fat_directory_listing *toFree)
{
}

int fat12_create_directory(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    int file_exists = fat12_get_fat_file_info(driver, path, DirectoryEntry, &file_info);
    if (file_exists)
        return 0;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    int parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

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

    fat12_write_directory(driver, driver->fat_bs, driver->fat_table, data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    return 1;
}

int fat12_create_file(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    FileInfoType file_exists = fat12_get_fat_file_info(driver, path, FileEntry, &file_info);
    if (file_exists)
        return 0;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

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

    int num_repeats = 30;
    char *text = "this was written from the os";
    uint16_t *text_data = (uint16_t *)calloc(sizeof(uint16_t) * strlen(text) * num_repeats);
    for (int i = 0; i < num_repeats; i++)
        memcpy((uint8_t *)(text_data + (i * strlen(text) / 2)), text, strlen(text));
    int data_sector = fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, text_data, strlen(text) * num_repeats, 0);

    // create and write descriptor to entry
    memcpy(data->contents[entry_loc].name, "        ", 8);
    memcpy(data->contents[entry_loc].name, path->components[path->count - 1], strlen(path->components[path->count - 1]));
    memcpy(data->contents[entry_loc].ext, "TXT", 3);
    data->contents[entry_loc].attributes |= 0x00;
    data->contents[entry_loc].first_cluster_number_high = 0;
    data->contents[entry_loc].first_cluster_number_low = data_sector;
    data->contents[entry_loc].file_size_bytes = strlen(text) * num_repeats;

    fat12_write_directory(driver, driver->fat_bs, driver->fat_table, data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);
    return 1;
}

void fat12_write_file(FSDriver *driver, Path *path, uint16_t *data, size_t size)
{
    fat_directory_entry_standard *file_info;
    FileInfoType file_exists = fat12_get_fat_file_info(driver, path, FileEntry, &file_info);
    if (!file_exists)
        return;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info);
    if (parent_exists == 0)
        return;
    fat_directory_listing *parent_dir_data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    // printf();
    fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, data, size, file_info->first_cluster_number_low);
    // parent_dir_data->contents update size in parent dir

    fat12_write_directory(driver, driver->fat_bs, driver->fat_table, parent_dir_data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);
}

FileInfoType fat12_get_fat_file_info(FSDriver *driver, Path *path, EntryType type, fat_directory_entry_standard **output)
{
    char *path_string = path_join(path);
    if (strcmp(path_string, "/") == 0)
    {
        mfree(path_string);
        return FileInfoType_RootDir;
    }
    mfree(path_string);

    Path *parent = path_get_parent(path);
    fat_directory_listing *parent_dir = fat12_get_directory_sector(driver, parent);

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
                return FileInfoType_Standard;
            }
        }
    }

    fat12_free_listing(parent_dir);
    path_free(parent);
    return FileInfoType_Nonexistent;
}

int fat12_remove_directory(FSDriver *driver, Path *path)
{
    return 1;
}

int fat12_remove_file(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    FileInfoType file_exists = fat12_get_fat_file_info(driver, path, FileEntry, &file_info);
    if (!file_exists)
        return 0;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    for (int i = 0; i < 2048; i++)
    {
        if ((data->contents[i].raw[0] & 0xFF) == 0x00)
            break; // no more listings in this sector
        if ((data->contents[i].raw[0] & 0xFF) == 0xE5)
            continue; // listing is unused

        char fs_name[13] = {0};
        char in_name[13] = {0};

        fat12_parse_filename(fs_name, data->contents[i].name);
        memcpy(in_name, path->components[path->count - 1], strlen(path->components[path->count - 1]));
        strlower(in_name);

        if (strcmp(fs_name, in_name) == 0)
        {
            fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, 0, 0, data->contents[i].first_cluster_number_low);
            data->contents[i].raw[0] = 0xE5;
            break;
        }
    }

    fat12_write_directory(driver, driver->fat_bs, driver->fat_table, data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    return 1;
}

DirectoryListing *fat12_list_directory(FSDriver *driver, Path *path)
{
    fat_directory_listing *data = fat12_get_directory_sector(driver, path);

    if (data)
    {
        DirectoryListing *prev = NULL;
        DirectoryListing *first = NULL;

        int i = 0;
        while (data->contents[i].raw[0] != 0)
        {
            if ((data->contents[i].raw[0] & 0xFF) == 0xE5)
            {
                i++;
                continue;
            }

            // printf("item\n");

            DirectoryListing *new = (DirectoryListing *)calloc(sizeof(DirectoryListing));
            new->info.name = (char *)calloc(sizeof(char) * 11);
            fat12_parse_filename(new->info.name, data->contents[i].name);
            new->info.ext = data->contents[i].ext;
            new->info.type = data->contents[i].attributes & 0x10 ? DirectoryEntry : FileEntry;
            new->info.size = data->contents[i].file_size_bytes;

            if (!first)
                first = new;
            if (prev)
                prev->next = new;

            prev = new;
            i++;
        }

        return first;
    }
    else
    {
        // printf("nothing\n");
        return 0;
    }
}

FileInfoType fat12_get_file_info(FSDriver *driver, Path *path, EntryType type, FileInfo **output)
{
    fat_directory_entry_standard *entry;
    FileInfoType fileinfo_type = fat12_get_fat_file_info(driver, path, type, &entry);

    if (output)
    {
        if (fileinfo_type == FileInfoType_Nonexistent)
            return FileInfoType_Nonexistent;

        (*output)->name = entry->name;
        (*output)->ext = entry->ext;
        (*output)->type = (fileinfo_type == FileInfoType_RootDir) ? DirectoryEntry : (entry->attributes & 0x10 ? DirectoryEntry : FileEntry);
        (*output)->size = entry->file_size_bytes;
    }

    return fileinfo_type;
}

FSDriver *fat12_init_driver(ide_device *ide)
{
    FSDriver *driver = (FSDriver *)calloc(sizeof(FSDriver));

    driver->ide = ide;

    driver->readFile = fat12_read_file;
    driver->writeFile = fat12_write_file;

    driver->createDirectory = fat12_create_directory;
    driver->createFile = fat12_create_file;

    driver->removeDirectory = fat12_remove_directory;
    driver->removeFile = fat12_remove_file;

    driver->directoryListing = fat12_list_directory;
    driver->fileInfo = fat12_get_file_info;

    // initialize bootsector and fat tables
    driver->fat_bs = fat12_read_bs(driver->ide);
    driver->fat_table = fat12_read_table(driver->ide, driver->fat_bs);

    return driver;
}