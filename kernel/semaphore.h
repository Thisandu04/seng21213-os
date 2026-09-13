/* =============================================================================
 * SENG21213-OS :: Counting Semaphore
 * File   : kernel/semaphore.h
 * Lecture 10 §3 – bounded-buffer producer/consumer
 * ============================================================================*/
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

typedef struct {
    volatile int count;
} semaphore_t;

void sem_init(semaphore_t *s, int initial);
void sem_wait(semaphore_t *s);
void sem_signal(semaphore_t *s);

#endif /* SEMAPHORE_H */
