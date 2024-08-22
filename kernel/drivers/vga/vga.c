#include "vga.h"

#include "modes/text.h"
#include "modes/gui.h"
#include "../../hw/port.h"
#include "../../hw/mem.h"

static VGAMode vga_mode;

void vga_init(VGAMode mode)
{
    vga_info = (VGAInfo *)calloc(sizeof(VGAInfo));
    vga_mode = mode;
    if (vga_mode == VGA_TEXT)
        vga_text_init();
    if (vga_mode == VGA_GUI)
        vga_gui_init();
}

void vga_switch_mode(VGAMode mode)
{
    vga_mode = mode;

    if (mode == VGA_TEXT)
    {
        vga_text_init();
        // vga_gui_deinit();
    }
    if (mode == VGA_GUI)
    {
        vga_gui_init();
        vga_text_deinit();
    }
}

void vga_write_registers(uint8_t *regs)
{
    /* write MISCELLANEOUS reg */
    outb(VGA_MISC_WRITE, *regs);
    regs++;

    /* write SEQUENCER regs */
    for (uint32_t i = 0; i < VGA_NUM_SEQ_REGS; i++)
    {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, *regs);
        regs++;
    }

    /* unlock CRTC registers */
    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);

    /* make sure they remain unlocked */
    regs[0x03] |= 0x80;
    regs[0x11] &= ~0x80;

    /* write CRTC regs */
    for (uint32_t i = 0; i < VGA_NUM_CRTC_REGS; i++)
    {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, *regs);
        regs++;
    }

    /* write GRAPHICS CONTROLLER regs */
    for (uint32_t i = 0; i < VGA_NUM_GC_REGS; i++)
    {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, *regs);
        regs++;
    }

    /* write ATTRIBUTE CONTROLLER regs */
    for (uint32_t i = 0; i < VGA_NUM_AC_REGS; i++)
    {
        inb(VGA_INSTAT_READ);
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, *regs);
        regs++;
    }

    /* lock 16-color palette and unblank display */
    inb(VGA_INSTAT_READ);
    outb(VGA_AC_INDEX, 0x20);
}

uint32_t vga_get_fb_address()
{
    outb(VGA_GC_INDEX, 6);
    uint32_t seg = (inb(VGA_GC_DATA) >> 2) & 3;
    switch (seg)
    {
    case 0:
    case 1:
        return 0xA0000;
    case 2:
        return 0xB0000;
    case 3:
        return 0xB8000;
    }
    return 0;
}

size_t vga_write_string(TTYColor color, const char *s)
{
    if (vga_mode == VGA_TEXT)
        return vga_text_write(color, s);
    return 0;
}

void vga_setc(char c, uint8_t x, uint8_t y, uint16_t color)
{
    if (vga_mode == VGA_TEXT)
        vga_text_setc(c, vga_text_get_offset(x, y), color);
}

void vga_delc()
{
    if (vga_mode == VGA_TEXT)
        vga_text_delc();
}

void vga_clear_screen()
{
    if (vga_mode == VGA_TEXT)
        vga_text_clear_screen();
    if (vga_mode == VGA_GUI)
        vga_gui_clear_screen(VGA_256_BLACK);
}

void vga_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (vga_mode == VGA_GUI)
        vga_gui_set_pixel(x, y, color);
}

VGAMode vga_get_mode()
{
    return vga_mode;
}

VGAInfo *vga_get_info()
{
    return vga_info;
}

void vga_in(tty_interface *tty, char c)
{
    if (c == 0x7F || c == 0x08)
        vga_text_backspace();
    else if (c == 0xD)
        vga_text_enter();
    else if (c == 0x3)
        vga_text_control('c', false);
    else
        vga_text_add_key(c);
}

tty_interface *vga_get_tty()
{
    if (vga_mode == VGA_TEXT)
        return vga_text_get_tty();
    return NULL;
}