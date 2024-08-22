#ifndef SCHEDULER_H
#define SCHEDULER_H

void scheduler_init();
void scheduler_tick();
void scheduler_add_task(void (*func)(), void *data);

#endif