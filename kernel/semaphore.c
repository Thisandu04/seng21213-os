/* =============================================================================
 * SENG21213-OS :: Counting Semaphore (blocking)
 * File   : kernel/semaphore.c
 * ============================================================================*/
#include "semaphore.h"
#include "process.h"
#include "scheduler.h"

void sem_init(semaphore_t *s, int initial) {
    s->count = initial;
}

void sem_wait(semaphore_t *s) {
    while (1) {
        __asm__ volatile ("cli");
        if (s->count > 0) {
            s->count--;
            __asm__ volatile ("sti");
            return;
        }
        current_process->state = PROC_BLOCKED;
        __asm__ volatile ("sti");
        __asm__ volatile ("hlt");
    }
}

void sem_signal(semaphore_t *s) {
    int i;
    __asm__ volatile ("cli");
    s->count++;
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_table_get(i);
        if (p->state == PROC_BLOCKED) p->state = PROC_READY;
    }
    __asm__ volatile ("sti");
}
