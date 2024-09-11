#ifndef DRIVERS_SHELL_H
#define DRIVERS_SHELL_H

#include <stdbool.h>
#include "../drivers/tty.h"

#define KEY_BUFFER_SIZE 128

enum CommandHashes
{
    COMMAND_CPU = 0xB8866ED,
    COMMAND_MEM = 0xB889004,
    COMMAND_PCI = 0xB889C81,
    COMMAND_IRQ = 0xB8880B1,
    COMMAND_ARP = 0xB885EA8,
    COMMAND_ICMP = 0x7C9856EE,
    COMMAND_IP = 0x59783E,
    COMMAND_DNS = 0xB886AEA,
    COMMAND_ECHO = 0x7C9624C4,
    COMMAND_DNSREQ = 0xF92A6D12,
    COMMAND_HTTP = 0x7C9813C5,
    COMMAND_GUI = 0xB88788A,
    COMMAND_DHCP = 0x7C95AD04,
    COMMAND_HELP = 0x7C97D2EE,
};

void shell_init();
void shell_add_key(tty_interface *tty, char letter);
bool shell_backspace(tty_interface *tty);
void shell_enter(tty_interface *tty);
void shell_control(tty_interface *tty, char c, bool shift);

bool is_ctrlc();

#endif