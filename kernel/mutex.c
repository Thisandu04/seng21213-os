/* =============================================================================
 * SENG21213-OS :: Mutex (blocking)
 * File   : kernel/mutex.c
 * ============================================================================*/
#include "mutex.h"
#include "process.h"
#include "scheduler.h"

void mutex_init(mutex_t *m) {
    m->locked = 0;
}

void mutex_lock(mutex_t *m) {
    while (1) {
        __asm__ volatile ("cli");
        if (!m->locked) {
            m->locked = 1;
            __asm__ volatile ("sti");
            return;
        }
        /* Someone else holds it: block, then re-check once woken. */
        current_process->state = PROC_BLOCKED;
        __asm__ volatile ("sti");
        __asm__ volatile ("hlt");
    }
}

void mutex_unlock(mutex_t *m) {
    int i;
    __asm__ volatile ("cli");
    m->locked = 0;
    /* Wake every blocked process. Each independently re-checks its own
     * condition and re-blocks if it lost the race (Mesa-style broadcast —
     * simple and correct for a small teaching kernel). */
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_table_get(i);
        if (p->state == PROC_BLOCKED) p->state = PROC_READY;
    }
    __asm__ volatile ("sti");
}
