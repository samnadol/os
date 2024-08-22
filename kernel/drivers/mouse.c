#include "mouse.h"

#include "../hw/port.h"
#include "../hw/cpu/irq.h"
#include "tty.h"
#include "../user/gui/gui.h"

uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];

void mouse_handler(registers_t *r)
{
    switch (mouse_cycle)
    {
    case 0:
        mouse_byte[0] = inb(PORT_PS2_DATA);
        mouse_cycle++;
        break;
    case 1:
        mouse_byte[1] = inb(PORT_PS2_DATA);
        mouse_cycle++;
        break;
    case 2:
        mouse_byte[2] = inb(PORT_PS2_DATA);
        gui_mouse(mouse_byte[0], mouse_byte[1], mouse_byte[2]);
        mouse_cycle = 0;
        break;
    }
}

inline void mouse_wait(uint8_t a_type) // a_type: 0 = data, 1 = signal
{
    uint32_t timeout = 100000;
    while (timeout > 0)
    {
        if ((inb(PORT_PS2_CTRL) & (a_type == 0 ? 0x1 : 0x2)) == (a_type == 0 ? 1 : 0))
        {
            return;
        }
        timeout--;
    }
    return;
}

inline void mouse_write(uint8_t a_write) // unsigned char
{
    // Wait to be able to send a command
    mouse_wait(1);
    // Tell the mouse we are sending a command
    outb(PORT_PS2_CTRL, 0xD4);
    // Wait for the final part
    mouse_wait(1);
    // Finally write
    outb(PORT_PS2_DATA, a_write);
}

uint8_t mouse_read()
{
    // Get's response from mouse
    mouse_wait(0);
    return inb(PORT_PS2_DATA);
}

void mouse_init()
{
    dprintf("[MSE] Initializing\n");

    uint8_t status;

    // enable mouse device
    mouse_wait(1);
    outb(PORT_PS2_CTRL, 0xA8);

    // enable interrupts
    mouse_wait(1);
    outb(PORT_PS2_CTRL, 0x20);

    mouse_wait(0);
    status = (inb(PORT_PS2_DATA) | 2);
    mouse_wait(1);
    outb(PORT_PS2_CTRL, 0x60);
    mouse_wait(1);
    outb(PORT_PS2_DATA, status);

    // Tell the mouse to use default settings
    mouse_write(0xF6);
    mouse_read(); // Acknowledge

    // Enable the mouse
    mouse_write(0xF4);
    mouse_read(); // Acknowledge

    irq_register(IRQ12, mouse_handler);
}