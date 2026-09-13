/* =============================================================================
 * SENG21213-OS :: Kernel Threads
 * File   : kernel/thread.h
 * Lecture 10 §1 – threads sharing the process's address space
 * ============================================================================*/
#ifndef THREAD_H
#define THREAD_H

int  thread_create(void (*entry_fn)(void *arg), void *arg, const char *name);
void thread_exit(void);

#endif /* THREAD_H */
