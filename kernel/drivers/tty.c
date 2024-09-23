#include "tty.h"

#include <stdarg.h>
#include <stdint.h>

#include "serial.h"
#include "vga/vga.h"
#include "vga/modes/text.h"
#include "../user/status.h"
#include "../lib/string.h"
#include "../hw/mem.h"

size_t num_interfaces = 0;

void tty_init()
{
    vga_init(VGA_TEXT);
    serial_init();
}

void delc(tty_interface *tty)
{
    tty->tty_del_char();
}

void tty_int_enable()
{
    serial_int_enable();
}

void tty_clear_screen(tty_interface *tty)
{
    if (tty->tty_clear_screen)
        tty->tty_clear_screen();
}

tty_interface *tty_add_interface(TTYType type, void (*tty_clear_screen)(), void (*tty_del_char)(), size_t (*tty_print)(TTYColor color, const char *s), void (*tty_kb_in)(tty_interface *tty, char c), void *device)
{
    tty_interface *interface = (tty_interface *)calloc(sizeof(tty_interface));
    interface->type = type;
    interface->interface_no = num_interfaces++;
    interface->tty_del_char = tty_del_char;
    interface->tty_kb_in = tty_kb_in;
    interface->tty_print = tty_print;
    interface->tty_clear_screen = tty_clear_screen;
    return interface;
}

size_t puts(tty_interface *interface, TTYColor color, const char *s)
{
    interface->tty_print(color, s);
    return 0;
}

size_t putc(tty_interface *interface, TTYColor color, char c)
{
    char s[2];
    s[0] = c;
    s[1] = '\0';

    return puts(interface, color, s);
}

size_t switch_char(tty_interface *tty, TTYColor color, char c)
{
    size_t char_printed = 0;
    switch (c)
    {
    case '\t':
        puts(tty, color, "    ");
        char_printed += 4;
        break;
    case '\r':
        break;
    default:
        putc(tty, color, c);
        char_printed += 1;
    }
    return char_printed;
}

void kprintf(tty_interface *tty, TTYColor color, const char *fmt, va_list ap)
{
    char buf[32];
    size_t char_printed = 0;
    for (int i = 0; i < strlen(fmt); i++)
    {
        if (fmt[i] == '%')
        {
            i++;
            switch (fmt[i])
            {
            case 'd':
                int32_t a = va_arg(ap, int32_t);
                if (a < 0)
                {
                    putc(tty, color, '-');
                    a *= -1;
                    char_printed += 1;
                }
                itoa(a, buf, 10);
                char_printed += puts(tty, color, buf);
                break;
            case 'u':
                itoa(va_arg(ap, uint32_t), buf, 10);
                char_printed += puts(tty, color, buf);
                break;
            case 'o':
                itoa(va_arg(ap, uint32_t), buf, 8);
                char_printed += puts(tty, color, buf);
                break;
            case 'x':
                itoa(va_arg(ap, uint32_t), buf, 16);
                char_printed += puts(tty, color, buf);
                break;
            case 'b':
                itoa(va_arg(ap, uint32_t), buf, 2);
                char_printed += puts(tty, color, buf);
                break;
            case 'c':
                char_printed += switch_char(tty, color, va_arg(ap, int));
                break;
            case 's':
                char_printed += puts(tty, color, va_arg(ap, char *));
                break;
            case 'p':
                itoa((uintptr_t)va_arg(ap, void *), buf, 16);
                char_printed += puts(tty, color, buf);
                break;
            case 'f':
                human_readable_size(va_arg(ap, uint32_t), buf, 32);
                char_printed += puts(tty, color, buf);
                break;
            case 'n':
                *va_arg(ap, int32_t *) = char_printed;
                break;
            case '%':
                putc(tty, color, '%');
                char_printed += 1;
                break;
            case 'm':
                uint8_t *mac = va_arg(ap, uint8_t *);
                itoa(mac[0], buf, 16);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, ':');
                itoa(mac[1], buf, 16);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, ':');
                itoa(mac[2], buf, 16);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, ':');
                itoa(mac[3], buf, 16);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, ':');
                itoa(mac[4], buf, 16);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, ':');
                itoa(mac[5], buf, 16);
                char_printed += puts(tty, color, buf);
                break;
            case 'i':
                uint32_t ip = va_arg(ap, uint32_t);
                itoa((ip >> 24) & 0xFF, buf, 10);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, '.');
                itoa((ip >> 16) & 0xFF, buf, 10);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, '.');
                itoa((ip >> 8) & 0xFF, buf, 10);
                char_printed += puts(tty, color, buf) + 1;
                putc(tty, color, '.');
                itoa((ip >> 0) & 0xFF, buf, 10);
                char_printed += puts(tty, color, buf);
                break;
            default:
                va_arg(ap, void *);
                puts(tty, color, "(?)");
                char_printed += 3;
            }
        }
        else
        {
            switch_char(tty, color, fmt[i]);
        }
    }
}

void printf(const char *fmt, ...)
{
    va_list a1, a2;
    va_copy(a2, a1);

    if (vga_get_mode() == VGA_TEXT)
    {
        va_start(a1, fmt);
        kprintf(vga_get_tty(), TTYColor_WHITE, fmt, a1);
        va_end(a1);
    }

    va_start(a2, fmt);
    kprintf(serial_get_tty(), TTYColor_WHITE, fmt, a2);
    va_end(a2);
}

void dprintf(uint8_t debuglevel, const char *fmt, ...)
{
    if (!serial_get_tty())
        return;

    if (debuglevel > 1)
        return;
        
    va_list a1, a2;
    va_copy(a2, a1);

    // va_start(a1, fmt);
    // kprintf(vga_get_tty(), TTYColor_WHITE, fmt, a1);
    // va_end(a1);

    va_start(a2, fmt);
    kprintf(serial_get_tty(), TTYColor_WHITE, fmt, a2);
    va_end(a2);
}

void tprintf(tty_interface *tty, const char *fmt, ...)
{
    va_list a1;
    va_start(a1, fmt);
    kprintf(tty, TTYColor_WHITE, fmt, a1);
    va_end(a1);
}

void ctprintf(tty_interface *tty, TTYColor color, const char *fmt, ...)
{
    va_list a1;
    va_start(a1, fmt);
    kprintf(tty, color, fmt, a1);
    va_end(a1);
}

void cprintf(TTYColor color, const char *fmt, ...)
{
    va_list a1, a2;
    va_copy(a2, a1);

    va_start(a1, fmt);
    kprintf(vga_get_tty(), color, fmt, a1);
    va_end(a1);

    va_start(a2, fmt);
    kprintf(serial_get_tty(), color, fmt, a2);
    va_end(a2);
}