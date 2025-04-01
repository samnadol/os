#include "fat12.h"

#include "../../../tty.h"
#include "../../../../hw/timer.h"
#include "../../../../hw/mem.h"
#include "../../../../lib/string.h"

FAT_BS *fat_read_bs(ide_device *ide)
{
    FAT_BS *bs = (FAT_BS *)calloc(sizeof(FAT_BS));

    uint16_t *bootsector = ata_28bit_pio_read_sector(*ide, 0, 1);
    for (int i = 0; i < ide->sector_size; i++)
        bs->raw[i] = bootsector[i];
    mfree(bootsector);

    return bs;
}

uint16_t *fat12_read_table(ide_device *ide, FAT_BS *bs)
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

uint16_t *fat12_read_root(ide_device *ide, FAT_BS *bs)
{
    uint16_t root_directory_location = bs->ebpb.reserved_logical_sectors + (bs->ebpb.logical_sectors_per_fat * bs->ebpb.num_fat_tables);
    uint16_t root_directory_sectors = (bs->ebpb.max_root_directory_entries * 32) / bs->ebpb.bytes_per_logical_sector;

    uint16_t *read = ata_28bit_pio_read_sector(*ide, root_directory_location, root_directory_sectors);
    return read;
}

uint16_t *fat12_read_data(ide_device *ide, FAT_BS *bs, uint16_t *fat, uint16_t offset_from_root)
{
    uint16_t root_directory_location = bs->ebpb.reserved_logical_sectors + (bs->ebpb.logical_sectors_per_fat * bs->ebpb.num_fat_tables);
    uint16_t root_directory_sectors = (bs->ebpb.max_root_directory_entries * 32) / bs->ebpb.bytes_per_logical_sector;
    uint16_t first_data_sector = root_directory_location + root_directory_sectors;

    // find number of sectors that contain this data in total
    uint8_t number_of_sectors = 0;
    uint16_t current_sector = offset_from_root;
    do
    {
        number_of_sectors++;
        current_sector = fat[current_sector];
    } while (current_sector != 0xFFF);
    current_sector = offset_from_root;

    // printf("reading %d sectors\n", number_of_sectors);

    // allocate memory to store data to be read
    uint16_t *data = (uint16_t *)calloc(bs->ebpb.bytes_per_logical_sector * number_of_sectors * 2);

    // read data from sectors, following FAT to find order
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

char *fat12_assemble_filename(FAT_DIRECTORY_ENTRY directory)
{
    uint16_t name_length = 0;
    char *name = directory.name;
    char *ext = directory.ext;
    while (*(name++) != 0x20)
        name_length++;
    while (*(ext++) != 0x20)
        name_length++;
    char *assembed = (char *)calloc(name_length + 2);

    uint16_t i = 0;
    name = directory.name;
    while (*name != 0x20 && i < 8)
    {
        assembed[i++] = *name;
        name++;
    }

    if (!(directory.attributes & 0x10))
    {
        assembed[i++] = '.';
        ext = directory.ext;
        while (*ext != 0x20)
        {
            assembed[i++] = *ext;
            ext++;
        }
    }

    return assembed;
}

// void fat12_read_directory(ide_device *ide, FAT_BS *bs, uint16_t *fat, uint16_t *data, int level)
// {
//     for (int f = 0; f < bs->ebpb.max_root_directory_entries; f++)
//     {
//         uint8_t start = ((uint8_t *)data)[(f * 16 * 2) + 0];
//         if (start == 0)
//             break;
//         else if (start == 0xE5)
//             continue;

//         uint8_t attribute = ((uint8_t *)data)[(f * 16 * 2) + 11];
//         if (attribute == 0x0F) // longname entry
//         {
//             // FAT_LONGNAME_ENTRY longname;
//             // memcpy(longname.raw, data + (f * 16), bs->ebpb.bytes_per_logical_sector / 2);

//             // int i = 0;
//             // char name[14];
//             // for (; i < 5; i++)
//             //     name[i] = longname.first5[i];
//             // for (; i < 11; i++)
//             //     name[i] = longname.next6[i - 5];
//             // for (; i < 13; i++)
//             //     name[i] = longname.final2[i - 11];
//             // name[13] = 0;

//             // printf("longname %s %x %s\n", (longname.order & 0xF0) >> 4 ? "FINAL" : "", longname.order & 0x0F, name);
//         }
//         else if (attribute & 0x10) // directory
//         {
//             FAT_DIRECTORY_ENTRY directory;
//             memcpy(directory.raw, data + (f * 16), bs->ebpb.bytes_per_logical_sector / 2);

//             char *name = fat12_assemble_filename(directory);
//             printf("\ndirectory %s\n", name);

//             if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
//             {
//                 uint16_t *read_dir = fat12_read_data(ide, bs, fat, directory.first_cluster_number_low);
//                 fat12_read_directory(ide, bs, fat, read_dir, level + 1);
//                 mfree(read_dir);
//             }

//             mfree(name);
//             // printf("\n");
//         }
//         else // file
//         {
//             FAT_DIRECTORY_ENTRY file;
//             memcpy(file.raw, data + (f * 16), bs->ebpb.bytes_per_logical_sector / 2);

//             if (file.raw[0] == 0)
//                 continue;

//             char *name = fat12_assemble_filename(file);
//             printf("file %s\n", name);
//             // printf("created %d/%d/%d %d:%d:%d\n", file.creation_date.month, file.creation_date.day, file.creation_date.year + 1980, file.creation_time.hour, file.creation_time.minute, file.creation_time.second * 2);
//             // printf("modified %d/%d/%d %d:%d:%d\n", file.last_modify_date.month, file.last_modify_date.day, file.last_modify_date.year + 1980, file.last_modify_time.hour, file.last_modify_time.minute, file.last_modify_time.second * 2);
//             // printf("accessed %d/%d/%d\n", file.last_access_date.month, file.last_access_date.day, file.last_access_date.year + 1980);
//             // printf("size %f\n", file.file_size_bytes);

//             uint16_t *read_file = fat12_read_data(ide, bs, fat, file.first_cluster_number_low);
//             // printf("data %s\n", read_file);
//             mfree(read_file);

//             mfree(name);
//             // printf("\n");
//         }
//     }
// }

// void fat_test(ide_device *ide)
// {
//     FAT_BS *bs = fat_read_bs(ide);
//     if (bs->signature != 0xAA55)
//     {
//         printf("FAT signature does not match (%x)!\n", bs->signature);
//         return;
//     }

//     printf("got here\n");

//     uint16_t *fat = fat12_read_table(ide, bs);
//     uint16_t *root_directory_data = fat12_read_root(ide, bs);

//     fat12_read_directory(ide, bs, fat, root_directory_data, 0);
//     mfree(root_directory_data);
// }

Path *fat12_find_listing(FSDriver *driver, Path *path)
{
    Path *dir = (Path *)calloc(sizeof(Path));
    memcpy(dir, path, sizeof(Path));
    dir->num_components -= 1;
    dir->type = DirectoryPath;

    // printf("path created\n");
    PathListing *containingDir = fat12_directory_listing(driver, dir);
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

int fat12_file_exists(FSDriver *driver, Path *path)
{
    Path *list = fat12_find_listing(driver, path);
    if (list == 0)
        return 0;

    if (list->type != FilePath)
        return 0;

    return 1;
}

int fat12_directory_exists(FSDriver *driver, Path *path)
{
    Path *list = fat12_find_listing(driver, path);
    if (list == 0)
        return 0;

    if (list->type != DirectoryPath)
        return 0;

    return 1;
}

File *fat12_read_file(FSDriver *driver, Path *path)
{
    Path *list = fat12_find_listing(driver, path);
    if (list == 0)
        return 0;

    File *file = (File *)calloc(sizeof(File));
    file->path = path;
    file->data = (uint8_t *)fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, ((FAT_DIRECTORY_ENTRY *) list->fs_specific_header)->first_cluster_number_low);
    mfree(list);

    return file;
}

void fat12_write_file(FSDriver *driver, Path *path, File *file)
{
    return;
}

PathListing *fat12_generate_directory_listing(FSDriver *driver, Path *path, uint16_t *data)
{
    PathListing *list = (PathListing *)calloc(sizeof(PathListing));

    // driver->fat_bs->ebpb.max_root_directory_entries
    for (int f = 0; f < 128; f++)
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
                FAT_DIRECTORY_ENTRY directory;
                memcpy(directory.raw, data + (f * 16), driver->fat_bs->ebpb.bytes_per_logical_sector / 2);
    
                memcpy(new->components, path->components, 256 * sizeof(char*));
                new->num_components = path->num_components;
                new->components[new->num_components++] = fat12_assemble_filename(directory);

                memcpy(new->fs_specific_header, directory.raw, 256);
                new->type = DirectoryPath;
            }
            else // file
            {
                FAT_DIRECTORY_ENTRY file;
                memcpy(file.raw, data + (f * 16), driver->fat_bs->ebpb.bytes_per_logical_sector / 2);
    
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

PathListing *fat12_directory_listing(FSDriver *driver, Path *path)
{
    if (path->type == FilePath)
    {
        printf("[FS] Cannot ls a file!\n");
        return 0;
    }

    Path *blank = (Path *)calloc(sizeof(Path));
    blank->type = DirectoryPath;
    // printf("reading root\n");
    uint16_t *root_data = fat12_read_root(driver->ide, driver->fat_bs);
    // printf("generating root listing\n");
    PathListing *root = fat12_generate_directory_listing(driver, blank, root_data);
    mfree(root_data);
    if (path->num_components == 0)
        return root;
    mfree(blank);

    // printf("beginning search\n");
    uint8_t depth = 0;
    Path *current = root->first;
    while (current)
    {
        if (depth == path->num_components)
        {
            // printf("FOUND\n");
            return root;
        }
        // printf("%d %s %s\n", depth, path->components[depth], current->components[depth]);

        if (strcmp(path->components[depth], current->components[depth]) == 0 && current->type == DirectoryPath)
        {
            uint16_t *data = fat12_read_data(driver->ide, driver->fat_bs, driver->fat_table, ((FAT_DIRECTORY_ENTRY *)current->fs_specific_header)->first_cluster_number_low);
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

    driver->fileExists = fat12_file_exists;
    driver->directoryExists = fat12_directory_exists;

    driver->fat_bs = fat_read_bs(driver->ide);
    driver->fat_table = fat12_read_table(driver->ide, driver->fat_bs);

    return driver;
}