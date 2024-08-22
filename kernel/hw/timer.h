#ifndef TIMER_H
#define TIMER_H

#define TIMER_HZ 1000

#include <stdint.h>
#include <stdbool.h>

void timer_init();
void timer_wait(int ms);
uint64_t timer_get_epoch();
uint64_t timer_get_tick();
bool timer_enabled();

#endif