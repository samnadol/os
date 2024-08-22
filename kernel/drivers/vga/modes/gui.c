#include "gui.h"

#include "../vga.h"

void vga_gui_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (x < vga_info->vga_width && y < vga_info->vga_height)
        vga_info->vga_address[(y * vga_info->vga_width) + x] = color;
}

void vga_gui_clear_screen(uint8_t color)
{
    for (uint32_t y = 0; y < vga_info->vga_height; y++)
    {
        for (uint32_t x = 0; x < vga_info->vga_width; x++)
        {
            vga_info->vga_address[vga_info->vga_width * y + x] = color;
        }
    }
}

void vga_gui_init()
{
    vga_write_registers(mode_graphics_320_200_256);

    vga_info = vga_get_info();
    vga_info->vga_width = 320;
    vga_info->vga_height = 200;
    vga_info->vga_bits_per_pixel = 256;
    vga_info->vga_address = (uint8_t *)vga_get_fb_address();

    vga_gui_clear_screen(0x0);
}