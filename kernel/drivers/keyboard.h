#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "../hw/port.h"
#include "../hw/cpu/irq.h"

void keyboard_init();
void keyboard_callback(registers_t *regs);

#endif