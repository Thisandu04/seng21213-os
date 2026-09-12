/* =============================================================================
 * SENG21213-OS :: Process Control Block
 * File   : kernel/process.h
 * Lecture 09 §1 – PCB structure
 * ============================================================================*/
#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES 8
#define STACK_SIZE    4096

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_TERMINATED
} proc_state_t;

typedef struct pcb {
    uint32_t     esp;      /* MUST be first field — switch.asm reads offset 0 */
    uint32_t     pid;
    proc_state_t state;
    uint32_t     entry;
    char         name[16];
    uint8_t      stack[STACK_SIZE];
} pcb_t;

void   process_init(void);
int    create_process(void (*entry_fn)(void), const char *name);
pcb_t *process_table_get(int index);

#endif /* PROCESS_H */
