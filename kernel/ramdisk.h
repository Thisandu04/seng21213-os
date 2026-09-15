/* =============================================================================
 * SENG21213-OS :: RAM Disk
 * File   : kernel/ramdisk.h
 * Lecture 12 §1 - fixed-size byte array acting as a block device
 * ============================================================================*/
#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

#define BLOCK_SIZE    4096u
#define RAMDISK_SIZE  (1024u * 1024u)              /* 1 MB */
#define TOTAL_BLOCKS  (RAMDISK_SIZE / BLOCK_SIZE)  /* 256 */

void  ramdisk_init(void);
void  ramdisk_read_block(uint32_t block_num, void *buf);
void  ramdisk_write_block(uint32_t block_num, const void *buf);
void *ramdisk_block_ptr(uint32_t block_num);  /* direct pointer - for metadata only */

#endif /* RAMDISK_H */
