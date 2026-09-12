/* =============================================================================
 * SENG21213-OS :: Interrupt Descriptor Table
 * File   : kernel/idt.h
 * Lecture 09 §1 – interrupt-driven execution
 * ============================================================================*/
#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags);

#endif /* IDT_H */
