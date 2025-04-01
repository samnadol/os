#include "fat12.h"

#include "../../tty.h"
#include "../../../hw/timer.h"
#include "../../../hw/mem.h"
#include "../../../lib/string.h"

FAT_BS fat_read_bs(ide_device *ide)
{
    FAT_BS bs;

    uint16_t *bootsector = ata_28bit_pio_read_sector(*ide, 0, 1);
    for (int i = 0; i < ide->sector_size; i++)
        bs.raw[i] = bootsector[i];
    mfree(bootsector);

    return bs;
}

void fat_read_fat(ide_device *ide, FAT_BS bs, uint16_t *fat)
{
    uint16_t *fat1sector = ata_28bit_pio_read_sector(*ide, bs.ebpb.reserved_logical_sectors, bs.ebpb.logical_sectors_per_fat);
    // uint16_t *fat2sector = ata_28bit_pio_read_sector(*ide, bs.ebpb.reserved_logical_sectors + bs.ebpb.logical_sectors_per_fat, bs.ebpb.logical_sectors_per_fat);
    
    uint32_t entries_per_fat = bs.ebpb.logical_sectors_per_fat * (3 * bs.ebpb.bytes_per_logical_sector) / 2;
    for (int i = 0; i < entries_per_fat; i++)
    {
        uint16_t fat_pos = (12 * i) / 8;
        uint16_t table_v = *((uint16_t *)((uint8_t *)fat1sector + fat_pos));
        uint16_t mask = (i & 1) ? table_v >> 4 : table_v & 0xFFF;

        // printf("c %x n %x\n", i, mask);
        fat[i] = mask;
    }
    mfree(fat1sector);
}

uint16_t *fat_read_file(ide_device *ide, FAT_BS bs, FAT_DIRECTORY_ENTRY directory, uint16_t *fat, uint16_t first_data_sector)
{
    uint16_t data_sectors = 0;
    uint16_t cluster_num = directory.first_cluster_number_low;
    do
    {
        data_sectors++;
        cluster_num = fat[cluster_num];
    } 
    while (cluster_num != 0xFFF);

    uint16_t *data = (uint16_t *)calloc(bs.ebpb.bytes_per_logical_sector * data_sectors);

    int i = 0;
    cluster_num = directory.first_cluster_number_low;
    do
    {
        // printf("reading sector %d\n", cluster_num - 2 + first_data_sector);
        uint16_t *read = ata_28bit_pio_read_sector(*ide, cluster_num - 2 + first_data_sector, 1);
        memcpy(data + (i * bs.ebpb.bytes_per_logical_sector), read, bs.ebpb.bytes_per_logical_sector);
        mfree(read);

        cluster_num = fat[cluster_num];
        i++;
    } 
    while (cluster_num != 0xFFF);

    return data;
}

char *fat_assemble_filename(FAT_DIRECTORY_ENTRY directory)
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
    assembed[i++] = '.';
    ext = directory.ext;
    while (*ext != 0x20)
    {
        assembed[i++] = *ext;
        ext++;
    }

    return assembed;
}

void fat_test(ide_device *ide)
{
    // dprintf(0, "[FAT] Testing FAT (sector size: %d words, %d bytes)\n", ide->sector_size, ide->sector_size * 2);

    FAT_BS bs = fat_read_bs(ide);
    if (bs.signature != 0xAA55)
    {
        printf("FAT signature does not match (%x)!\n", bs.signature);
        return;
    }

    uint16_t fat[bs.ebpb.logical_sectors_per_fat * (3 * bs.ebpb.bytes_per_logical_sector) / 2];
    fat_read_fat(ide, bs, fat);
    // printf("[FAT] %d bytes per sector, %d reserved sector(s), %d tables, %d sectors per cluster, %d sectors per fat\n", bs.ebpb.bytes_per_logical_sector, bs.ebpb.reserved_logical_sectors, bs.ebpb.num_fat_tables, bs.ebpb.logical_sectors_per_cluster, bs.ebpb.logical_sectors_per_fat);
    
    uint16_t root_directory_location = bs.ebpb.reserved_logical_sectors + (bs.ebpb.logical_sectors_per_fat * bs.ebpb.num_fat_tables);
    uint16_t root_directory_sectors = ((bs.ebpb.max_root_directory_entries * 32) + (bs.ebpb.bytes_per_logical_sector - 1)) / bs.ebpb.bytes_per_logical_sector;
    uint16_t *root_directory_sector = ata_28bit_pio_read_sector(*ide, root_directory_location, root_directory_sectors);
    uint16_t first_data_sector = root_directory_location + root_directory_sectors;
    // printf("[FAT] %d root directory sectors, %d first data sector\n", root_directory_sectors, root_directory_location + root_directory_sectors);

    printf("\n");
    for (int f = 0; f < 128; f++)
    {
        uint8_t attribute = ((uint8_t *)root_directory_sector)[(f * 16 * 2) + 11];
        if (attribute == 0x0F)
        {
            // FAT_LONGNAME_ENTRY longname;
            // for (int i = 0; i < ide->sector_size; i++)
            //     longname.raw[i] = root_directory_sector[(f * 16) + i];

            // int i = 0;
            // char name[14];
            // for (; i < 5; i++)
            //     name[i] = longname.first5[i];
            // for (; i < 11; i++)
            //     name[i] = longname.next6[i - 5];
            // for (; i < 13; i++)
            //     name[i] = longname.final2[i - 11];
            // name[13] = 0;

            // printf("longname %x %x %s\n", (longname.order & 0xF0) >> 4, longname.order & 0x0F, name);

            // printf("\n");
        }
        else
        {
            FAT_DIRECTORY_ENTRY directory;
            for (int i = 0; i < ide->sector_size; i++)
                directory.raw[i] = root_directory_sector[(f * 16) + i];

            if (directory.raw[0] == 0)
                continue;

            char *name = fat_assemble_filename(directory);
            uint16_t *data = fat_read_file(ide, bs, directory, fat, first_data_sector);

            printf("file %s\n", name);
            printf("created %d/%d/%d %d:%d:%d\n", directory.creation_date.month, directory.creation_date.day, directory.creation_date.year + 1980, directory.creation_time.hour, directory.creation_time.minute, directory.creation_time.second * 2);
            printf("modified %d/%d/%d %d:%d:%d\n", directory.last_modify_date.month, directory.last_modify_date.day, directory.last_modify_date.year + 1980, directory.last_modify_time.hour, directory.last_modify_time.minute, directory.last_modify_time.second * 2);
            printf("accessed %d/%d/%d\n", directory.last_access_date.month, directory.last_access_date.day, directory.last_access_date.year + 1980);
            printf("size %f\n", directory.file_size_bytes);
            printf("data %s", data);

            // for (int i = 0; i < (bs.ebpb.bytes_per_logical_sector * data_sectors) / 32; i++)
            // {
            //     printf("[%xx] ", i);
            //     for (int j = 0; j < 32; j++)
            //     {
            //         printf("%c ", ((uint8_t *)data)[(i * 32) + j]);
            //     }
            //     printf("\n");
            // }
            printf("\n");

            mfree(name);
            mfree(data);
        }
    }
    mfree(root_directory_sector);
}