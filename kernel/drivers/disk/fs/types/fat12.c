#include "fat12.h"

#include "../../../tty.h"
#include "../../../../hw/timer.h"
#include "../../../../hw/mem.h"
#include "../../../../lib/string.h"

#include "../../../../lib/time.h"
#include "../../../../drivers/net/l3/time.h"

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
        printf("reading %d RD sectors from %x\n", root_directory_sectors, root_directory_location);
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

        uint16_t *data = (uint16_t *)calloc(bs->ebpb.bytes_per_logical_sector * number_of_clusters * bs->ebpb.logical_sectors_per_cluster * 2);
        // for (int i = 0; i < 10; i++)
        // {
        //     for (int j = 0; j < 16; j++)
        //         printf("%x ", data[(i * 16) + j]);
        //     printf("\n");
        // }

        int sectors_read = 0;
        do
        {
            uint16_t *read_data = ata_28bit_pio_read_sector(*ide, current_cluster - 2 + first_data_sector, bs->ebpb.logical_sectors_per_cluster);
            if (read_data == 0)
            {
                mfree(data);
                return 0;
            }
            memcpy(data + (sectors_read * bs->ebpb.bytes_per_logical_sector / 2), read_data, bs->ebpb.bytes_per_logical_sector);
            mfree(read_data);
            current_cluster = fat[current_cluster];
            sectors_read++;
        } while (current_cluster < 0xFF8);
        printf("reading %d sectors from %x\n", sectors_read, current_cluster - 2 + first_data_sector);

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
        printf("writing %d RD sectors at %x\n", root_directory_sectors, root_directory_location);
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

            // printf("found %d clusters, %d needed\n", clusters_found, clusters_needed);

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

        printf("writing %d sectors at %x\n", clusters_needed, first_data_sector - 2 + clusters[0]);
        for (int i = 0; i < clusters_needed; i++)
        {
            // printf("writing cluster %d %d %s\n", first_data_sector - 2 + clusters[i], size, data + ((bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector / 2) * i));
            if (!ata_28bit_pio_write_sector(
                    *ide,
                    first_data_sector - 2 + clusters[i],
                    bs->ebpb.logical_sectors_per_cluster,
                    data + ((bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector / 2) * i),
                    size > bs->ebpb.bytes_per_logical_sector ? (bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector) : size))
            {
                return 0;
            }
            size -= (bs->ebpb.logical_sectors_per_cluster * bs->ebpb.bytes_per_logical_sector);
        }

        fat12_write_table(ide, bs, fat);
        return clusters[0];
    }
}

void fat12_assemble_lfn(char *dest, fat_directory_entry_longname **lfns, size_t num_lfns)
{
    size_t loc = 0;
    for (int i = 0; i < num_lfns; i++)
    {
        size_t j;
        for (j = 0; j < 5; j++)
            dest[loc++] = lfns[i]->first5[j];
        for (j = 0; j < 6; j++)
            dest[loc++] = lfns[i]->next6[j];
        for (j = 0; j < 2; j++)
            dest[loc++] = lfns[i]->final2[j];
    }
    dest[loc] = 0;

    strlower(dest);
}

void fat12_parse_filename(char *dest, const char *name, const char *ext)
{
    int name_len = 0;
    for (int i = 0; i < 8; i++)
        if (name[i] != 0 && name[i] != ' ')
            name_len++;

    int ext_len = 0;
    for (int i = 0; i < 3; i++)
        if (ext[i] != 0 && ext[i] != ' ')
            ext_len++;

    // dest = (char *)malloc((name_len + ext_len + 1) * sizeof(char));

    for (int i = 0; i < name_len; i++)
    {
        dest[i] = name[i];
    }

    if (ext_len > 0)
    {
        dest[name_len] = '.';
        for (int i = 0; i < ext_len; i++)
        {
            dest[name_len + i + 1] = ext[i];
        }
    }

    dest[name_len + ext_len + (ext_len > 0 ? 1 : 0)] = '\0';

    strlower(dest);
}

uint16_t *fat12_read_file(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *info = 0;
    int exists = fat12_get_fat_file_info(driver, path, FileEntry, &info, 0);
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

        size_t num_lfns = 0;
        fat_directory_entry_longname *lfns[LFN_INDEX_ARR_SIZE];

        int depth = 0;
        for (int i = 0; i < driver->fat_bs->ebpb.max_root_directory_entries; i++)
        {
            if ((root_data->contents[i].raw[0] & 0xFF) == 0x00)
                break; // no more listings in this sector
            if ((root_data->contents[i].raw[0] & 0xFF) == 0xE5)
                continue; // listing is unused

            if (root_data->contents[i].attributes == 0x0F)
            {
                fat_directory_entry_longname *lfn = (fat_directory_entry_longname *)&root_data->contents[i];
                lfns[(lfn->order & ~0x40) - 1] = lfn;
                num_lfns++;
            }
            else
            {
                if (root_data->contents[i].attributes & 0x10)
                { // directory
                    char fs_name[255] = {0};
                    char in_name[255] = {0};

                    if (num_lfns > 0)
                        fat12_assemble_lfn(fs_name, lfns, num_lfns);
                    else
                        fat12_parse_filename(fs_name, root_data->contents[i].name, root_data->contents[i].ext);

                    memcpy(in_name, path->components[depth], strlen(path->components[depth]) + 1);
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

                        // printf("%s %s\n", search_string, final_string);

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
                num_lfns = 0;
            }
        }
    }
    return 0;
}

int fat12_write_directory(FSDriver *driver, fat_bootsector *bs, uint16_t *fat, fat_directory_listing *data, int cluster)
{
    int i = 0;
    while (data->contents[i].name[0] != 0)
    {
        i += 1;
    }
    size_t table_size = i * sizeof(fat_directory_entry_standard);

    int result_cluster = fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, data->raw, table_size, cluster);
    if (!result_cluster)
        return 0;
    return 1;
}

void fat12_free_listing(fat_directory_listing *toFree)
{
}

int fat12_create_directory(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    int file_exists = fat12_get_fat_file_info(driver, path, DirectoryEntry, &file_info, 0);
    if (file_exists)
        return 0;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    int parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info, 0);
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
    fat_directory_entry_standard *file_info = 0;
    FileInfoType file_exists = fat12_get_fat_file_info(driver, path, FileEntry, &file_info, 0);
    if (file_exists)
    {
        printf("file exists\n");
        return 0;
    }

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info = 0;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info, 0);
    if (parent_exists == 0)
    {
        printf("could not get parent dir\n");
        return 0;
    }
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    int entry_loc = 0;
    while (data->contents[entry_loc].raw[0] != 0 && entry_loc < driver->fat_bs->ebpb.max_root_directory_entries)
    {
        if ((data->contents[entry_loc].raw[0] & 0xFF) == 0x00)
            break;
        if ((data->contents[entry_loc].raw[0] & 0xFF) == 0xE5)
            break;
        entry_loc++;
    }

    uint16_t *text = (uint16_t *)calloc(1);
    int data_sector = fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, text, 1, 0);
    mfree(text);

    // create and write descriptor to entry
    memcpy(data->contents[entry_loc].name, "        ", 8);
    memcpy(data->contents[entry_loc].name, path->components[path->count - 1], strlen(path->components[path->count - 1]));
    memcpy(data->contents[entry_loc].ext, "   ", 3);
    data->contents[entry_loc].attributes = 0x00;
    data->contents[entry_loc].first_cluster_number_high = 0;
    data->contents[entry_loc].first_cluster_number_low = data_sector;
    data->contents[entry_loc].file_size_bytes = 1;

    calendar_t time = time_calendar(time_unix_epoch());
    data->contents[entry_loc].creation_date.year = time.year - 1980;
    data->contents[entry_loc].creation_date.month = time.month;
    data->contents[entry_loc].creation_date.day = time.day;
    data->contents[entry_loc].creation_time.hour = time.hour;
    data->contents[entry_loc].creation_time.minute = time.minute;
    data->contents[entry_loc].creation_time.second = time.second;
    data->contents[entry_loc].last_modify_date.year = time.year - 1980;
    data->contents[entry_loc].last_modify_date.month = time.month;
    data->contents[entry_loc].last_modify_date.day = time.day;
    data->contents[entry_loc].last_modify_time.hour = time.hour;
    data->contents[entry_loc].last_modify_time.minute = time.minute;
    data->contents[entry_loc].last_modify_time.second = time.second;

    fat12_write_directory(driver, driver->fat_bs, driver->fat_table, data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);
    return 1;
}

void fat12_write_file(FSDriver *driver, Path *path, uint16_t *data, size_t size)
{
    fat_directory_entry_standard *file_info;
    uint16_t entry_num;
    FileInfoType file_exists = fat12_get_fat_file_info(driver, path, FileEntry, &file_info, &entry_num);
    if (file_exists != FileInfoType_Standard)
        return;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info, 0);
    if (parent_exists == 0)
        return;
    fat_directory_listing *parent_dir_data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    // printf("%x %x %x\n", data[0], data[1], data[2]);
    fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, data, size, file_info->first_cluster_number_low);

    parent_dir_data->contents[entry_num].file_size_bytes = size;
    // printf("%s %d\n", parent_dir_data->contents[entry_num].name, size);
    fat12_write_directory(driver, driver->fat_bs, driver->fat_table, parent_dir_data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);
}

fat_entry_search_result fat12_find_entry(FSDriver *driver, fat_directory_listing *data, char *search)
{
    fat_entry_search_result res;

    size_t num_lfns = 0;
    fat_directory_entry_longname *lfns[LFN_INDEX_ARR_SIZE];
    size_t lfn_index[LFN_INDEX_ARR_SIZE];

    int i = 0;
    while (data->contents[i].raw[0] != 0 && i < driver->fat_bs->ebpb.max_root_directory_entries)
    {
        if ((data->contents[i].raw[0] & 0xFF) == 0x00)
            break; // no more listings in this sector
        if ((data->contents[i].raw[0] & 0xFF) != 0xE5)
        {
            if (data->contents[i].attributes == 0x0F)
            {
                fat_directory_entry_longname *lfn = (fat_directory_entry_longname *)&data->contents[i];
                lfns[(lfn->order & ~0x40) - 1] = lfn;
                lfn_index[(lfn->order & ~0x40) - 1] = i;
                num_lfns++;
            }
            else
            {
                char fs_name[255] = {0};
                char in_name[255] = {0};

                if (num_lfns > 0)
                    fat12_assemble_lfn(fs_name, lfns, num_lfns);
                else
                    fat12_parse_filename(fs_name, data->contents[i].name, data->contents[i].ext);

                memcpy(in_name, search, strlen(search));
                strlower(in_name);

                if (strcmp(fs_name, in_name) == 0)
                {
                    res.found = true;

                    res.entry = &data->contents[i];
                    res.entry_index = i;

                    res.num_lfns = num_lfns;
                    memcpy(res.lfn_index, lfn_index, LFN_INDEX_ARR_SIZE * sizeof(size_t));

                    return res;
                }

                num_lfns = 0;
            }
        }
        i++;
    }

    res.found = false;
    return res;
}

FileInfoType fat12_get_fat_file_info(FSDriver *driver, Path *path, EntryType type, fat_directory_entry_standard **output, uint16_t *entry_num)
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

    fat_entry_search_result result = fat12_find_entry(driver, parent_dir, path->components[path->count - 1]);
    if (result.found)
    {
        fat12_free_listing(parent_dir);
        path_free(parent);
        *output = &parent_dir->contents[result.entry_index];

        if (entry_num)
            *entry_num = result.entry_index;

        return FileInfoType_Standard;
    }
    else
    {
        fat12_free_listing(parent_dir);
        path_free(parent);
        return FileInfoType_Nonexistent;
    }
}

int fat12_remove_file(FSDriver *driver, Path *path)
{
    fat_directory_entry_standard *file_info;
    FileInfoType file_exists = fat12_get_fat_file_info(driver, path, FileEntry, &file_info, 0);
    if (!file_exists)
        return 0;

    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info, 0);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    if (data == 0)
        printf("fuck\n");

    fat_entry_search_result result = fat12_find_entry(driver, data, path->components[path->count - 1]);
    if (result.found && !(result.entry->attributes & 0x10))
    {
        if (!fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, 0, 0, result.entry->first_cluster_number_low))
            printf("write (f) failed\n");

        ((uint8_t *)data->contents[result.entry_index].raw)[0] = 0xE5;
        for (int j = 0; j < result.num_lfns; j++)
            ((uint8_t *)data->contents[result.lfn_index[j]].raw)[0] = 0xE5;

        if (!fat12_write_directory(driver, driver->fat_bs, driver->fat_table, data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low))
            printf("write (d) failed\n");

        return 1;
    }
    else
    {
        return 0;
    }
}

int fat12_remove_directory(FSDriver *driver, Path *path)
{
    // remove directory listing
    Path *parent = path_get_parent(path);
    fat_directory_entry_standard *parent_info;
    FileInfoType parent_exists = fat12_get_fat_file_info(driver, parent, DirectoryEntry, &parent_info, 0);
    if (parent_exists == 0)
        return 0;
    fat_directory_listing *parent_data = (fat_directory_listing *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

    fat_entry_search_result result = fat12_find_entry(driver, parent_data, path->components[path->count - 1]);
    if (result.found && result.entry->attributes & 0x10)
    {
        fat_directory_listing *data = fat12_get_directory_sector(driver, path);
        if (data)
        {
            char buf[256];

            int i = 0;
            while (data->contents[i].raw[0] != 0)
            {
                if ((data->contents[i].raw[0] & 0xFF) != 0xE5)
                {
                    if (data->contents[i].attributes != 0x0F)
                    {
                        fat12_parse_filename(buf, data->contents[i].name, data->contents[i].ext);
                        if (data->contents[i].attributes & 0x10)
                        {
                            if (strcmp(buf, ".") != 0 && strcmp(buf, "..") != 0)
                            {
                                Path *new = path_duplicate(path);
                                path_add_string(buf, new);
                                fat12_remove_directory(driver, new);
                                path_free(new);
                            }
                        }
                        else
                        {
                            Path *new = path_duplicate(path);
                            path_add_string(buf, new);
                            fat12_remove_file(driver, new);
                            path_free(new);
                        }
                    }
                }
                i++;
            }
        }

        fat12_write_data(driver->ide, driver->fat_bs, driver->fat_table, 0, 0, result.entry->first_cluster_number_low);

        ((uint8_t *)parent_data->contents[result.entry_index].raw)[0] = 0xE5;
        for (int j = 0; j < result.num_lfns; j++)
            ((uint8_t *)parent_data->contents[result.lfn_index[j]].raw)[0] = 0xE5;

        fat12_write_directory(driver, driver->fat_bs, driver->fat_table, parent_data, parent_exists == FileInfoType_RootDir ? -1 : parent_info->first_cluster_number_low);

        return 1;
    }
    else
    {
        return 0;
    }
}

DirectoryListing *fat12_list_directory(FSDriver *driver, Path *path)
{
    fat_directory_listing *data = fat12_get_directory_sector(driver, path);
    // printf("sector %c\n", data[0]);

    if (data)
    {
        // printf("data exists\n");

        DirectoryListing *prev = NULL;
        DirectoryListing *first = NULL;

        size_t num_lfns = 0;
        fat_directory_entry_longname *lfns[LFN_INDEX_ARR_SIZE];

        int i = 0;
        while (data->contents[i].raw[0] != 0)
        {
            if ((data->contents[i].raw[0] & 0xFF) != 0xE5)
            {
                if (data->contents[i].attributes == 0x0F)
                {
                    fat_directory_entry_longname *lfn = (fat_directory_entry_longname *)&data->contents[i];
                    lfns[(lfn->order & ~0x40) - 1] = lfn;
                    num_lfns++;
                }
                else
                {
                    DirectoryListing *new = (DirectoryListing *)calloc(sizeof(DirectoryListing));

                    if (num_lfns > 0)
                    {
                        new->info.name = (char *)calloc((13 * sizeof(char) * num_lfns) + 1);
                        fat12_assemble_lfn(new->info.name, lfns, num_lfns);
                    }
                    else
                    {
                        new->info.name = (char *)calloc((11 * sizeof(char)) + 1);
                        fat12_parse_filename(new->info.name, data->contents[i].name, data->contents[i].ext);
                    }

                    new->info.type = data->contents[i].attributes & 0x10 ? DirectoryEntry : FileEntry;
                    new->info.size = data->contents[i].file_size_bytes;
                    new->info.creation_date = data->contents[i].creation_date;
                    new->info.creation_time = data->contents[i].creation_time;

                    if (!first)
                        first = new;
                    if (prev)
                        prev->next = new;

                    prev = new;

                    num_lfns = 0;
                }
            }
            i++;
        }

        if (!first)
        {
            first = (DirectoryListing *)calloc(sizeof(DirectoryListing));
            first->info.name = ".";
            first->info.type = DirectoryEntry;
        }

        return first;
    }
    return 0;
}

FileInfoType fat12_get_file_info(FSDriver *driver, Path *path, EntryType type, FileInfo **output)
{
    fat_directory_entry_standard *entry = 0;
    FileInfoType fileinfo_type = fat12_get_fat_file_info(driver, path, type, &entry, 0);

    if (output)
    {
        if (fileinfo_type == FileInfoType_Nonexistent)
            return FileInfoType_Nonexistent;

        (*output)->name = entry->name;
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