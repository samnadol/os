#pragma once

#include <stdbool.h>

void gui_init();
void gui_update();

void gui_keypress(char c);
void gui_control(char c, bool shift);
void gui_backspace();
void gui_enter();