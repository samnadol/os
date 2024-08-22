#include "text.h"

#include "../../../hw/mem.h"
#include "../../../lib/string.h"
#include "../../../hw/port.h"
#include "../../../user/shell.h"

tty_interface *tty;

void vga_text_setc(char c, uint32_t offset, uint16_t color)
{
	uint8_t *vidmem = (uint8_t *)vga_info->vga_address;
	vidmem[offset] = c;
	vidmem[offset + 1] = color;
}

uint32_t vga_text_get_offset(uint8_t col, uint8_t row)
{
	return 2 * (row * vga_info->vga_width + col);
}

uint8_t vga_text_get_row_from_offset(uint32_t offset)
{
	return offset / (2 * vga_info->vga_width);
}

uint32_t vga_text_move_offset_to_new_line(uint32_t offset)
{
	return vga_text_get_offset(0, vga_text_get_row_from_offset(offset) + 1);
}

void vga_text_set_cursor(uint32_t offset)
{
	offset /= 2;
	outb(PORT_VGA_CTRL, VGA_OFFSET_HIGH);
	outb(PORT_VGA_DATA, (uint8_t)(offset >> 8));
	outb(PORT_VGA_CTRL, VGA_OFFSET_LOW);
	outb(PORT_VGA_DATA, (uint8_t)(offset & 0xff));
}

uint16_t vga_text_get_cursor()
{
	outb(PORT_VGA_CTRL, VGA_OFFSET_HIGH);
	uint16_t offset = inb(PORT_VGA_DATA) << 8;

	outb(PORT_VGA_CTRL, VGA_OFFSET_LOW);
	offset += inb(PORT_VGA_DATA);

	return offset * 2;
}

void vga_text_clear_screen()
{
	for (int i = 0; i < vga_info->vga_width * vga_info->vga_height; ++i)
		vga_text_setc(' ', i * 2, VGA_TEXT_WHITE_ON_BLACK);
	vga_text_set_cursor(vga_text_get_offset(0, 0));
}

uint32_t vga_text_scroll_ln(uint32_t offset)
{
	memcpy((char *)(vga_text_get_offset(0, 0) + vga_info->vga_address), (char *)(vga_text_get_offset(0, 1) + vga_info->vga_address), vga_info->vga_width * (vga_info->vga_height - 1) * 2);
	for (int col = 0; col < vga_info->vga_width; col++)
		vga_text_setc(' ', vga_text_get_offset(col, vga_info->vga_height - 1), VGA_TEXT_WHITE_ON_BLACK);
	return offset - 2 * vga_info->vga_width;
}

void vga_text_delc()
{
	uint16_t newCursor = vga_text_get_cursor() - 2;
	vga_text_setc(' ', newCursor, VGA_TEXT_WHITE_ON_BLACK);
	vga_text_set_cursor(newCursor);
}

size_t vga_text_write(TTYColor color, const char *s)
{
	uint32_t offset = vga_text_get_cursor();

	size_t i = 0;
	while (s[i] != 0)
	{
		if (s[i] == '\n')
			offset = vga_text_move_offset_to_new_line(offset);
		else
		{
			vga_text_setc(s[i], offset, vgatty_colors[color]);
			offset += 2;
		}

		if (offset >= vga_info->vga_height * vga_info->vga_width * 2)
			offset = vga_text_scroll_ln(offset);

		i++;
	}
	vga_text_set_cursor(offset);

	return i;
}

void vga_text_init()
{
	tty = tty_add_interface(TTYType_VGA, vga_clear_screen, vga_delc, vga_write_string, vga_in, NULL);

	vga_write_registers(mode_text_80x25);

	vga_info = vga_get_info();
	vga_info->vga_width = 80;
	vga_info->vga_height = 25;
	vga_info->vga_bits_per_pixel = 16;
	vga_info->vga_address = (uint8_t *)vga_get_fb_address();

	vga_text_clear_screen();
}

void vga_text_deinit()
{
	tty = NULL;
}

tty_interface *vga_text_get_tty()
{
	return tty;
}

// keyboard
void vga_text_add_key(char c)
{
	shell_add_key(tty, c);
}
bool vga_text_backspace()
{
	return shell_backspace(tty);
}
void vga_text_enter()
{
	shell_enter(tty);
}
void vga_text_control(char c, bool shift)
{
	shell_control(tty, c, shift);
}