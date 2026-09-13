/* =============================================================================
 * SENG21213-OS :: Mutex
 * File   : kernel/mutex.h
 * Lecture 10 §2 – mutual exclusion
 * ============================================================================*/
#ifndef MUTEX_H
#define MUTEX_H

typedef struct {
    volatile int locked;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */
