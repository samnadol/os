#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../tty.h"

#define VGA_AC_INDEX 0x3C0
#define VGA_AC_WRITE 0x3C0
#define VGA_AC_READ 0x3C1
#define VGA_MISC_WRITE 0x3C2
#define VGA_SEQ_INDEX 0x3C4
#define VGA_SEQ_DATA 0x3C5
#define VGA_DAC_READ_INDEX 0x3C7
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA 0x3C9
#define VGA_MISC_READ 0x3CC
#define VGA_GC_INDEX 0x3CE
#define VGA_GC_DATA 0x3CF
#define VGA_CRTC_INDEX 0x3D4 /* 0x3B4 */
#define VGA_CRTC_DATA 0x3D5  /* 0x3B5 */
#define VGA_INSTAT_READ 0x3DA
#define VGA_NUM_SEQ_REGS 5
#define VGA_NUM_CRTC_REGS 25
#define VGA_NUM_GC_REGS 9
#define VGA_NUM_AC_REGS 21
#define VGA_NUM_REGS (1 + VGA_NUM_SEQ_REGS + VGA_NUM_CRTC_REGS + VGA_NUM_GC_REGS + VGA_NUM_AC_REGS)

enum VGAColor_256B
{
    VGA_256_BLACK = 0x0,
    VGA_256_BLUE = 0x1,
    VGA_256_GREEN = 0x2,
    VGA_256_TURQUOISE = 0x3,
    VGA_256_RED = 0x4,
    VGA_256_PURPLE = 0x5,
    VGA_256_GOLD = 0x6,
    VGA_256_GRAY = 0x7,
    VGA_256_DARKBLUE = 0x8,
    VGA_256_LIGHTBLUE = 0x9,
    VGA_256_WHITE = 0x3F,
};

enum VGAColor_4B
{
    VGA_4_BLACK = 0x0,
    VGA_4_BLUE = 0x1,
    VGA_4_GREEN = 0x2,
    VGA_4_CYAN = 0x3,
    VGA_4_RED = 0x4,
    VGA_4_WHITE = 0xF,
};

typedef enum VGAMode
{
    VGA_TEXT = 0x1,
    VGA_GUI = 0x2,
} VGAMode;

typedef struct VGAInfo
{
    uint32_t vga_width;
    uint32_t vga_height;
    uint32_t vga_bits_per_pixel;
    uint8_t *vga_address;
} VGAInfo;
static VGAInfo *vga_info;

void vga_init(VGAMode mode);
void vga_switch_mode(VGAMode mode);
VGAMode vga_get_mode();
void vga_clear_screen();
VGAInfo *vga_get_info();

void vga_write_registers(uint8_t *regs);
uint32_t vga_get_fb_address();

// text mode
size_t vga_write_string(TTYColor color, const char *s);
void vga_setc(char c, uint8_t x, uint8_t y, uint16_t color);
void vga_delc();

// gui mode
void vga_set_pixel(uint16_t x, uint16_t y, uint8_t color);

// keyboard
void vga_kb_in(tty_interface *tty, char c);
tty_interface *vga_get_tty();