#ifndef DRIVERS_SHELL_H
#define DRIVERS_SHELL_H

#include <stdbool.h>
#include "../drivers/tty.h"

#define KEY_BUFFER_SIZE 128

void shell_init();
void shell_add_key(tty_interface *tty, char letter);
bool shell_backspace(tty_interface *tty);
void shell_enter(tty_interface *tty);
void shell_control(tty_interface *tty, char c, bool shift);

bool is_ctrlc();

#endif