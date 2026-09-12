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
    int slot = -1;
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_UNUSED) { slot = i; break; }
    }
    if (slot == -1) return -1;

    pcb_t *p = &process_table[slot];
    p->pid   = next_pid++;
    p->entry = (uint32_t)entry_fn;
    p_strncpy(p->name, name, sizeof(p->name));

    uint32_t *sp = (uint32_t *)(p->stack + STACK_SIZE);

    *(--sp) = 0x202;               /* EFLAGS — IF=1, interrupts stay enabled */
    *(--sp) = 0x08;                /* CS — kernel code selector (VERIFY vs boot.asm GDT) */
    *(--sp) = (uint32_t)entry_fn;  /* EIP — where execution begins */
    *(--sp) = 0; /* EAX */
    *(--sp) = 0; /* ECX */
    *(--sp) = 0; /* EDX */
    *(--sp) = 0; /* EBX */
    *(--sp) = 0; /* ESP — dummy, POPAD discards this slot */
    *(--sp) = 0; /* EBP */
    *(--sp) = 0; /* ESI */
    *(--sp) = 0; /* EDI */

    p->esp   = (uint32_t)sp;
    p->state = PROC_READY;
    return (int)p->pid;
}

pcb_t *process_table_get(int index) {
    if (index < 0 || index >= MAX_PROCESSES) return 0;
    return &process_table[index];
}
