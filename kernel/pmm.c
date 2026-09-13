/* =============================================================================
 * SENG21213-OS :: Physical Memory Manager
 * File   : kernel/pmm.c
 * Lecture 11 §2 - parses the E820 map left by boot.asm, builds a bitmap
 * ============================================================================*/
#include "pmm.h"

#define E820_COUNT_ADDR    0x8000
#define E820_ENTRIES_ADDR  0x8004

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;      /* 1 = usable RAM */
    uint32_t acpi_ext;
} e820_entry_t;

/* Sized to match this project's fixed QEMU config: -m 32M (see Makefile) */
#define PHYS_MEM_SIZE  (32u * 1024u * 1024u)
#define MAX_FRAMES     (PHYS_MEM_SIZE / FRAME_SIZE)   /* 8192 */
#define BITMAP_SIZE    (MAX_FRAMES / 8)                /* 1024 bytes */

static uint8_t  bitmap[BITMAP_SIZE];
static uint32_t used_frame_count;

extern uint32_t kernel_end; /* linker symbol - address just past kernel image */

static inline void bitmap_set(uint32_t f)   { bitmap[f / 8] |= (uint8_t)(1 << (f % 8)); }
static inline void bitmap_clear(uint32_t f) { bitmap[f / 8] &= (uint8_t)~(1 << (f % 8)); }
static inline int  bitmap_test(uint32_t f)  { return bitmap[f / 8] & (1 << (f % 8)); }

void pmm_init(void) {
    uint32_t i, f;

    /* Step 1: assume EVERYTHING is used/reserved until proven otherwise. */
    for (i = 0; i < BITMAP_SIZE; i++) bitmap[i] = 0xFF;
    used_frame_count = MAX_FRAMES;

    /* Step 2: walk the E820 map the bootloader stored, freeing usable RAM. */
    uint16_t count = *(volatile uint16_t *)E820_COUNT_ADDR;
    e820_entry_t *entries = (e820_entry_t *)E820_ENTRIES_ADDR;

    for (i = 0; i < count; i++) {
        e820_entry_t *e = &entries[i];
        if (e->type != 1) continue; /* only usable RAM regions */

        uint64_t start = e->base;
        uint64_t end   = e->base + e->length;

        if (start < 0x100000) start = 0x100000;      /* never touch below 1MB */
        if (end > PHYS_MEM_SIZE) end = PHYS_MEM_SIZE;
        if (start >= end) continue;

        uint32_t first = (uint32_t)(start / FRAME_SIZE);
        uint32_t last  = (uint32_t)(end   / FRAME_SIZE);

        for (f = first; f < last; f++) {
            if (bitmap_test(f)) {
                bitmap_clear(f);
                used_frame_count--;
            }
        }
    }

    /* Step 3: belt-and-braces - explicitly reserve the kernel's own frames,
     * in case it ever grows past 1MB in a future extension. */
    uint32_t kstart = 0x10000 / FRAME_SIZE;
    uint32_t kend   = ((uint32_t)&kernel_end + FRAME_SIZE - 1) / FRAME_SIZE;
    for (f = kstart; f < kend; f++) {
        if (!bitmap_test(f)) { bitmap_set(f); used_frame_count++; }
    }
}

uint32_t pmm_alloc_frame(void) {
    uint32_t i;
    for (i = 0; i < MAX_FRAMES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frame_count++;
            return i * FRAME_SIZE;
        }
    }
    return 0; /* out of memory */
}

void pmm_free_frame(uint32_t paddr) {
    uint32_t frame = paddr / FRAME_SIZE;
    if (frame >= MAX_FRAMES) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frame_count--;
    }
}

uint32_t pmm_total_frames(void) { return MAX_FRAMES; }
uint32_t pmm_used_frames(void)  { return used_frame_count; }
uint32_t pmm_free_frames(void)  { return MAX_FRAMES - used_frame_count; }
