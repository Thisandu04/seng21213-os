/* =============================================================================
 * SENG21213-OS :: File System
 * File   : kernel/fs.h
 * Lecture 12 §2-3 - superblock, inodes, flat directory, POSIX-style API
 * ============================================================================*/
#ifndef FS_H
#define FS_H

#include "../include/types.h"
#include "ramdisk.h"

#define MAX_FILENAME   28
#define DIRECT_BLOCKS  8
#define MAX_FILE_SIZE  (DIRECT_BLOCKS * BLOCK_SIZE)   /* 32 KB */

void fs_init(void);

int  fs_exists(const char *name);
int  fs_open(const char *name);                        /* auto-creates if new; returns fd or -1 */
int  fs_read(int fd, void *buf, uint32_t len);
int  fs_write(int fd, const void *buf, uint32_t len);   /* always appends */
void fs_close(int fd);
int  fs_unlink(const char *name);

int  fs_list(char names[][MAX_FILENAME], uint32_t sizes[], int max_entries);

#endif /* FS_H */
