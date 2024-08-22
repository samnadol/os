#ifndef DRIVER_VGA_H
#define DRIVER_VGA_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../tty.h"
#include "../vga.h"

#define VGA_OFFSET_LOW 0x0f
#define VGA_OFFSET_HIGH 0x0e

#define VGA_TEXT_WHITE_ON_BLACK (VGA_4_BLACK << 4 | VGA_4_WHITE)
#define VGA_TEXT_WHITE_ON_GREEN (VGA_4_GREEN << 4 | VGA_4_WHITE)

static uint8_t vgatty_colors[] = {
    (VGA_4_BLACK << 4 | VGA_4_WHITE),
    (VGA_4_BLACK << 4 | VGA_4_RED),
};

static uint8_t mode_text_80x25[] = {
    /* MISC */
    0x67,
    /* SEQ */
    0x03, 0x00, 0x03, 0x00, 0x02,
    /* CRTC */
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
    0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF,
    /* GC */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
    0xFF,
    /* AC */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x0C, 0x00, 0x0F, 0x08, 0x00,
};

// initializes the screen
void vga_text_init();
void vga_text_deinit();
tty_interface *vga_text_get_tty();

void vga_text_clear_screen();

// deletes the character to the immediate left of the cursor
void vga_text_delc();

// writes given string to screen. returns number of chars written
size_t vga_text_write(TTYColor color, const char *s);

void vga_text_setc(char c, uint32_t offset, uint16_t color);
uint32_t vga_text_get_offset(uint8_t col, uint8_t row);

// keyboard
void vga_text_add_key(char letter);
bool vga_text_backspace();
void vga_text_enter();
void vga_text_control(char c, bool shift);

#endif