/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * File   : kernel/scheduler.h
 * ============================================================================*/
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

extern pcb_t *current_process;   /* NEW */

void scheduler_init(void);
void scheduler_run(void);

#endif
