#pragma once

#include <stdbool.h>
#include <stdint.h>

void gui_init();
void gui_update();

// keyboard
void gui_keypress(char c);
void gui_control(char c, bool shift);
void gui_backspace();
void gui_enter();

// mouse
void gui_mouse(uint8_t data, int8_t dx, int8_t dy);
