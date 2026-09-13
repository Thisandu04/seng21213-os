/* =============================================================================
 * SENG21213-OS :: Process Table
 * File   : kernel/process.c
 * Lecture 09 §2 – process creation
 * ============================================================================*/
#include "process.h"

static pcb_t    process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;

static void p_strncpy(char *dst, const char *src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

void process_init(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++)
        process_table[i].state = PROC_UNUSED;
}

/* Hand-builds a fake interrupt-frame stack so the first context switch into
 * this process looks identical to switching into an already-running one.
 * Layout must exactly match what PUSHAD + a hardware interrupt produce,
 * since switch.asm resumes execution via POPAD + IRETD. */
int create_process(void (*entry_fn)(void), const char *name) {
    pcb_t *p = process_alloc(name);
    if (!p) return -1;

    p->entry = (uint32_t)entry_fn;
    uint32_t *sp = (uint32_t *)(p->stack + STACK_SIZE);

    *(--sp) = 0x202; *(--sp) = 0x08; *(--sp) = (uint32_t)entry_fn;
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; *(--sp) = 0;

    p->esp   = (uint32_t)sp;
    p->state = PROC_READY;
    return (int)p->pid;
}

pcb_t *process_table_get(int index) {
    if (index < 0 || index >= MAX_PROCESSES) return 0;
    return &process_table[index];
}

/* Finds a free slot and reserves it (pid + name only — caller must build
 * the stack frame and set state = PROC_READY once ready to run).
 * TERMINATED slots are reclaimable too: once a thread hits thread_exit(),
 * it will NEVER run again on our single CPU, so it's safe for a different
 * currently-running thread to recycle its slot. */
pcb_t *process_alloc(const char *name) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_UNUSED ||
            process_table[i].state == PROC_TERMINATED) {
            pcb_t *p = &process_table[i];
            p->pid = next_pid++;
            p_strncpy(p->name, name, sizeof(p->name));
            return p;
        }
    }
    return 0;
}
