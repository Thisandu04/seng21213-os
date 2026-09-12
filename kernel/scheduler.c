/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * File   : kernel/scheduler.c
 * Lecture 09 §3 – PIT programming, IRQ0 handler, round-robin dispatch
 * ============================================================================*/
#include "process.h"
#include "idt.h"
#include "scheduler.h"
#include "../include/io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIT_CH0   0x40
#define PIT_CMD   0x43
#define PIT_FREQ  1193182

extern void irq0_entry(void);
extern void scheduler_start(void);

pcb_t *current_process;
static int current_index = -1;

static void pic_remap(void) {
    outb(PIC1_CMD, 0x11); outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 0x20);   /* master: IRQ0-7  -> INT 0x20-0x27 */
    outb(PIC2_DATA, 0x28);   /* slave:  IRQ8-15 -> INT 0x28-0x2F */
    outb(PIC1_DATA, 0x04); outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01); outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, 0xFE);   /* unmask ONLY IRQ0 (timer) */
    outb(PIC2_DATA, 0xFF);   /* mask all slave IRQs — no handlers exist yet */
}

void pic_send_eoi(void) {
    outb(PIC1_CMD, 0x20);
}

static void pit_init(uint32_t hz) {
    uint16_t divisor = (uint16_t)(PIT_FREQ / hz);
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);
}

void scheduler_next_pcb(void) {
    if (current_process->state == PROC_RUNNING)
        current_process->state = PROC_READY;

    int start = current_index;
    do {
        current_index = (current_index + 1) % MAX_PROCESSES;
        pcb_t *p = process_table_get(current_index);
        if (p->state == PROC_READY) {
            current_process = p;
            p->state = PROC_RUNNING;
            return;
        }
    } while (current_index != start);

    current_process->state = PROC_RUNNING; /* nothing else ready */
}

void scheduler_init(void) {
    int i;
    pic_remap();
    idt_set_gate(0x20, (uint32_t)irq0_entry, 0x08, 0x8E);
    pit_init(100); /* 10ms tick */

    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_table_get(i);
        if (p->state == PROC_READY) {
            current_index = i;
            current_process = p;
            p->state = PROC_RUNNING;
            break;
        }
    }
}

void scheduler_run(void) {

    scheduler_start(); /* never returns */
}
