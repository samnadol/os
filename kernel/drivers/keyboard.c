#include "keyboard.h"

#include "../user/shell.h"
#include "./tty.h"
#include "../lib/string.h"
#include "vga/vga.h"
#include "../user/gui/gui.h"

#define SC_MAX 57
#define BACKSPACE 0x0E
#define ENTER 0x1C

const char *scancode_human[] = {"ERROR", "Esc", "1", "2", "3", "4", "5", "6",
                                "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E",
                                "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl",
                                "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`",
                                "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".",
                                "/", "RShift", "Keypad *", "LAlt", "Spacebar"};

const char scancode_ascii[] = {'?', '?', '1', '2', '3', '4', '5', '6',
                               '7', '8', '9', '0', '-', '=', '?', '?', 'q', 'w', 'e', 'r', 't', 'y',
                               'u', 'i', 'o', 'p', '[', ']', '?', '?', 'a', 's', 'd', 'f', 'g',
                               'h', 'j', 'k', 'l', ';', '\'', '`', '?', '\\', 'z', 'x', 'c', 'v',
                               'b', 'n', 'm', ',', '.', '/', '?', '?', '?', ' '};

bool shift = false;
bool ctrl = false;

void keyboard_handler(registers_t *regs)
{    
    uint8_t scancode = inb(PORT_PS2_DATA);
    tty_interface *vgatty = vga_get_tty();

    // ignore rctrl identifier
    if (scancode == 0xE0)
        return;

    if (scancode == 0x1D)
        ctrl = true;
    else if (scancode == 0x9D)
        ctrl = false;
    else if (scancode == 0x2A || scancode == 0x36)
        shift = true;
    else if (scancode == 0xAA || scancode == 0xB6)
        shift = false;
    else if (ctrl && scancode == 0x2e)
        vgatty->tty_kb_in(vgatty, 0x3);
    else
    {
        switch (scancode)
        {
        case BACKSPACE:
            vgatty->tty_kb_in(vgatty, 0x8);
            break;
        case ENTER:
            vgatty->tty_kb_in(vgatty, 0xD);
            break;
        default:
            if (scancode <= 57)
                vgatty->tty_kb_in(vgatty, scancode_ascii[(int)scancode] - ((shift && char_is_small_letter(scancode_ascii[(int)scancode]) ? 32 : 0)));
        }
    }
}

void keyboard_init()
{
    dprintf("[KB] Initializing\n");
    irq_register(IRQ1, keyboard_handler);
}