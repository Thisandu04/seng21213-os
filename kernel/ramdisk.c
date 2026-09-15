/* =============================================================================
 * SENG21213-OS :: RAM Disk
 * File   : kernel/ramdisk.c
 * ============================================================================*/
#include "ramdisk.h"

/* Uninitialised -> lives in .bss, adds ZERO bytes to the actual kernel.bin
 * file on disk (BSS is NOBITS - reserved space only, not stored data).
 * We zero it explicitly at runtime below regardless. */
static uint8_t ramdisk[RAMDISK_SIZE];

void ramdisk_init(void) {
    uint32_t i;
    for (i = 0; i < RAMDISK_SIZE; i++) ramdisk[i] = 0;
}

void ramdisk_read_block(uint32_t block_num, void *buf) {
    uint8_t *dst = (uint8_t *)buf;
    uint8_t *src = ramdisk + ((uint32_t)block_num * BLOCK_SIZE);
    uint32_t i;
    if (block_num >= TOTAL_BLOCKS) return;
    for (i = 0; i < BLOCK_SIZE; i++) dst[i] = src[i];
}

void ramdisk_write_block(uint32_t block_num, const void *buf) {
    uint8_t *dst = ramdisk + ((uint32_t)block_num * BLOCK_SIZE);
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t i;
    if (block_num >= TOTAL_BLOCKS) return;
    for (i = 0; i < BLOCK_SIZE; i++) dst[i] = src[i];
}

void *ramdisk_block_ptr(uint32_t block_num) {
    if (block_num >= TOTAL_BLOCKS) return 0;
    return (void *)(ramdisk + ((uint32_t)block_num * BLOCK_SIZE));
}
