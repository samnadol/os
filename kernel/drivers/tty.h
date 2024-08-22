#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>

typedef enum TTYColor
{
    TTYColor_WHITE,
    TTYColor_RED,
} TTYColor;

enum PrintDestinations
{
    TTY_VGA = (1 << 0),
    TTY_SERIAL = (1 << 1),
};

typedef enum TTYType
{
    TTYType_VGA,
    TTYType_SERIAL_COM,
    TTYType_SERIAL_PCI,
} TTYType;

typedef struct tty_interface
{
    TTYType type;
    int interface_no;
    char keybuffer[256];

    void (*tty_clear_screen)();
    void (*tty_del_char)();
    size_t (*tty_print)(TTYColor color, const char *s);
    void (*tty_kb_in)(struct tty_interface *tty, char c);
} tty_interface;

void tty_init();
tty_interface *tty_add_interface(TTYType type, void (*tty_clear_screen)(), void (*tty_del_char)(), size_t (*tty_print)(TTYColor color, const char *s), void (*tty_kb_in)(tty_interface *tty, char c), void *device);

void tty_clear_screen(tty_interface *interface);
size_t putc(tty_interface *interface, TTYColor color, char c);
void delc(tty_interface *interface);

void printf(const char *fmt, ...);
void dprintf(const char *fmt, ...);
void tprintf(tty_interface *tty, const char *fmt, ...);
void cprintf(TTYColor color, const char *fmt, ...);
void ctprintf(tty_interface *tty, TTYColor color, const char *fmt, ...);

#endif