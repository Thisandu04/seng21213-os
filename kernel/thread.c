/* =============================================================================
 * SENG21213-OS :: Kernel Threads
 * File   : kernel/thread.c
 * ============================================================================*/
#include "thread.h"
#include "process.h"
#include "scheduler.h"   /* for current_process */

extern void thread_trampoline(void); /* defined in switch.asm */

/* A thread reuses the exact same pcb_t/scheduling machinery as a process —
 * the only real difference is that its entry function takes an argument.
 * Since our context switch resumes via POPAD+IRETD (not a normal CALL),
 * we can't pass an argument the usual way. Instead we stash:
 *   ESI = real entry function address
 *   EBX = the argument
 * into the fake initial stack frame, and point EIP at a small assembly
 * trampoline that reads them out of the registers POPAD just restored,
 * then makes a normal cdecl call into the real function. */
int thread_create(void (*entry_fn)(void *arg), void *arg, const char *name) {
    pcb_t *p = process_alloc(name);
    if (!p) return -1;

    p->entry = (uint32_t)entry_fn;

    uint32_t *sp = (uint32_t *)(p->stack + STACK_SIZE);
    *(--sp) = 0x202;                        /* EFLAGS */
    *(--sp) = 0x08;                         /* CS */
    *(--sp) = (uint32_t)thread_trampoline;  /* EIP -> trampoline, NOT entry_fn directly */
    *(--sp) = 0;                            /* EAX */
    *(--sp) = 0;                            /* ECX */
    *(--sp) = 0;                            /* EDX */
    *(--sp) = (uint32_t)arg;                /* EBX -> becomes the thread's argument */
    *(--sp) = 0;                            /* ESP dummy */
    *(--sp) = 0;                            /* EBP */
    *(--sp) = (uint32_t)entry_fn;           /* ESI -> real function the trampoline calls */
    *(--sp) = 0;                            /* EDI */

    p->esp   = (uint32_t)sp;
    p->state = PROC_READY;
    return (int)p->pid;
}

/* Called automatically once a thread's entry function returns. Marks the
 * slot TERMINATED (permanently skipped by the scheduler) and halts forever.
 * Safe to reclaim this slot later from ANY other running thread — once
 * TERMINATED, this pcb will never execute again on our single CPU. */
void thread_exit(void) {
    __asm__ volatile ("cli");
    current_process->state = PROC_TERMINATED;
    __asm__ volatile ("sti");
    for (;;) { __asm__ volatile ("hlt"); }
}
