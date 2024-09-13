#include "gui.h"

#include "../../drivers/vga/vga.h"
#include "../../drivers/tty.h"
#include "../../lib/string.h"
#include "../../hw/mem.h"
#include "../../hw/pci.h"
#include "../../hw/timer.h"
#include "../../hw/cpu/cpuid.h"
#include "font/font.h"

size_t sb_size;
uint8_t *sb;

uint32_t frames;
uint32_t fps;
uint64_t frame_start_tick;

struct mouse
{
    uint16_t mouse_x;
    uint16_t mouse_y;
};
struct mouse *mouse = 0;

struct pci_dev
{
    uint16_t vendor;
    uint16_t device;
    struct pci_dev *next;
};
struct pci_dev *pci_first = 0;

inline double clamp(double d, double min, double max)
{
    const double t = d < min ? min : d;
    return t > max ? max : t;
}

char *buf;
void gui_init()
{
    buf = (char *)calloc(64);
    vga_info = vga_get_info();
    sb_size = vga_info->vga_width * vga_info->vga_height * (vga_info->vga_bits_per_pixel / 256);

    sb = (uint8_t *)calloc(sb_size);
    if (!sb)
        dprintf(0, "[GUI] failed to calloc screenbuffer\n");

    mouse = (struct mouse *)calloc(sizeof(struct mouse));
    mouse->mouse_x = 0;
    mouse->mouse_y = 0;

    dprintf(0, "[GUI] init (%dx%d, %d bit, fb @ 0x%x, size %f)\n", vga_info->vga_width, vga_info->vga_height, vga_info->vga_bits_per_pixel, vga_info->vga_address, sb_size);

    struct pci_dev *current = pci_first;
    for (uint32_t bus = 0; bus <= 0xFF; bus++)
    {
        for (uint32_t slot = 0; slot < 32; slot++)
        {
            for (uint32_t function = 0; function < 8; function++)
            {
                uint16_t vendor = pci_get_vendor_id(bus, slot, function);
                if (vendor == 0xFFFF)
                    continue;

                struct pci_dev *new = (struct pci_dev *)calloc(sizeof(struct pci_dev));
                new->vendor = vendor;
                new->device = pci_get_device_id(bus, slot, function);
                new->next = 0;

                if (!pci_first)
                    pci_first = new;
                if (current)
                    current->next = new;
                current = new;
            }
        }
    }
}

inline static void gui_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (((y * vga_info->vga_width) + x) > sb_size)
    {
        printf("[GUI] tried to set pixel outside of screen buffer (address %x, sb_size %x)\n", ((y * vga_info->vga_width) + x), sb_size);
        return;
    }
    sb[(y * vga_info->vga_width) + x] = color;
}

inline static void gui_set_line(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    memset(sb + (y * vga_info->vga_width) + x, color, w);
}

inline static void gui_rectangle(uint16_t tlx, uint16_t tly, uint16_t w, uint16_t h, uint16_t color)
{
    for (size_t y = tly; y < tly + h; y++)
        gui_set_line(tlx, y, w, color);
}

inline static void gui_clear_screen(uint16_t color)
{
    memset(sb, color, sb_size);
}

inline static void gui_copy_sb()
{
    memcpy(vga_info->vga_address, sb, sb_size);
}

// returns width of letter
size_t gui_print_letter(uint8_t *fontchar, uint16_t x, uint16_t y, uint16_t color)
{
    size_t l_width = 0;
    while (fontchar[l_width] != 99)
        l_width++;
    l_width /= FONT_HEIGHT;

    // memcpy(sb + ((cy + y) * vga_info->vga_width) + x, fontchar + (cy * l_width), l_width);

    for (size_t cx = 0; cx < l_width; cx++)
        for (size_t cy = 0; cy < FONT_HEIGHT; cy++)
            if (fontchar[(cy * l_width) + cx])
                gui_set_pixel(x + cx, y + cy, color);

    return l_width + 1;
}

size_t gui_write(char *s, size_t len, uint16_t x, uint16_t y, uint16_t color)
{
    size_t width = 0;
    for (int cx = 0; cx < len; cx++)
    {
        uint8_t *fc = font_get_char(s[cx]);
        if (!fc)
        {
            dprintf(0, "[GUI] no font char for '%c' (ASCII %d)\n", s[cx], s[cx]);
            continue;
        }
        width += gui_print_letter(fc, x + width, y, color);
    }
    return width;
}

uint16_t bgcolor = VGA_256_DARKBLUE;
void gui_update()
{
    gui_clear_screen(bgcolor);

    mem_stats m = mem_get_stats();
    gui_write(buf, sprintf(buf, "os %f/%f, %dfps, uptime %t", m.used, m.total, fps, timer_get_epoch()), 5, 5, VGA_256_WHITE);
    gui_write(buf, sprintf(buf, "%s", cpuinfo.vendor), 5, 20, VGA_256_WHITE);
    gui_write(buf, sprintf(buf, "%s", cpuinfo.model), 5, 35, VGA_256_WHITE);
    gui_write("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26, 140, 55, VGA_256_WHITE);
    gui_write("abcdefghijklmnopqrstuvwxyz", 26, 140, 70, VGA_256_WHITE);
    gui_write("01234567890 ():@+-/.,?", 21, 140, 85, VGA_256_WHITE);

    gui_write("PCI Devices:", 12, 140, 115, VGA_256_WHITE);
    size_t cy = 130, cx = 140;
    struct pci_dev *current = pci_first;
    while (current)
    {
        gui_write(buf, sprintf(buf, "%x:%x", current->vendor, current->device), cx, cy, VGA_256_WHITE);
        if (cy > 160)
        {
            cy = 115;
            cx += 60;
        }
        cy += 15;
        current = current->next;
    }

    frames++;
    if (timer_get_tick() - frame_start_tick > 1000)
    {
        frame_start_tick = timer_get_tick();
        fps = frames;
        frames = 0;
    }

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 0x8; c++)
            gui_rectangle((c * 16) + 5, (r * 16) + 55, 15, 15, c + (r * 8));

    gui_rectangle(mouse->mouse_x, mouse->mouse_y, 2, 2, VGA_256_GOLD);

    gui_copy_sb();
}

void mouse_lc()
{
    printf("mouse lc at %d %d\n", mouse->mouse_x, mouse->mouse_y);

    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 0x8; c++)
            if (mouse->mouse_x > (c * 16) + 5 && (c * 16) + 15 + 5 > mouse->mouse_x)
                if (mouse->mouse_y > (r * 16) + 55 && (r * 16) + 15 + 55 > mouse->mouse_y)
                    bgcolor = c + (r * 8);
}
void mouse_rc()
{
    printf("mouse rc at %d %d\n", mouse->mouse_x, mouse->mouse_y);
}
void mouse_mc()
{
    printf("mouse mc at %d %d\n", mouse->mouse_x, mouse->mouse_y);
}

void gui_keypress(char c)
{
}
void gui_control(char c, bool shift)
{
}
void gui_backspace()
{
}
void gui_enter()
{
}

void gui_mouse(uint8_t data, int8_t dx, int8_t dy)
{
    if (mouse)
    {
        if (data & 0x1)
            mouse_lc();
        if (data & 0x2)
            mouse_rc();
        if (data & 0x4)
            mouse_mc();

        mouse->mouse_x = clamp(mouse->mouse_x + dx, 0, vga_info->vga_width);
        mouse->mouse_y = clamp(mouse->mouse_y - dy, 0, vga_info->vga_height);
    }
}