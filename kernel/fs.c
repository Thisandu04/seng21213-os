/* =============================================================================
 * SENG21213-OS :: File System
 * File   : kernel/fs.c
 * ============================================================================*/
#include "fs.h"

#define FS_MAGIC 0x53454E47u   /* "SENG" */

#define SUPERBLOCK_NUM      0
#define DIRECTORY_BLOCK     1
#define BLOCK_BITMAP_BLOCK  2
#define INODE_BITMAP_BLOCK  3
#define INODE_TABLE_START   4
#define INODE_TABLE_BLOCKS  1                              /* 64 * 40B fits in 1 block */
#define DATA_BLOCKS_START   (INODE_TABLE_START + INODE_TABLE_BLOCKS)  /* block 5 */

#define MAX_INODES      64
#define MAX_DIR_ENTRIES 100     /* 100 * 36B = 3600B, fits in one 4KB block */
#define MAX_OPEN_FILES  8

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
} superblock_t;

typedef struct {
    char     name[MAX_FILENAME];
    uint32_t inode;
    uint32_t used;
} dir_entry_t;

typedef struct {
    uint32_t used;
    uint32_t size;
    uint32_t blocks[DIRECT_BLOCKS];
} inode_t;

typedef struct {
    int      used;
    uint32_t inode;
    uint32_t pos;
} file_t;

static superblock_t *sb;
static dir_entry_t   *dir;
static uint8_t        *block_bitmap;
static uint8_t        *inode_bitmap;
static inode_t        *inodes;
static file_t          open_files[MAX_OPEN_FILES];

/* ---------------------------------------------------------------------------
 * Tiny local string helpers (no shared string.c in this project)
 * --------------------------------------------------------------------------*/
static uint32_t fs_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}
static int fs_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
static void fs_strncpy(char *dst, const char *src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* ---------------------------------------------------------------------------
 * Bitmap helpers
 * --------------------------------------------------------------------------*/
static inline void bm_set(uint8_t *bm, uint32_t i)   { bm[i / 8] |= (uint8_t)(1 << (i % 8)); }
static inline void bm_clear(uint8_t *bm, uint32_t i) { bm[i / 8] &= (uint8_t)~(1 << (i % 8)); }
static inline int  bm_test(uint8_t *bm, uint32_t i)  { return bm[i / 8] & (1 << (i % 8)); }

static int32_t alloc_block(void) {
    uint32_t i;
    for (i = DATA_BLOCKS_START; i < TOTAL_BLOCKS; i++) {
        if (!bm_test(block_bitmap, i)) { bm_set(block_bitmap, i); return (int32_t)i; }
    }
    return -1;
}
static void free_block(uint32_t b) { bm_clear(block_bitmap, b); }

static int32_t alloc_inode(void) {
    uint32_t i; int j;
    for (i = 0; i < MAX_INODES; i++) {
        if (!bm_test(inode_bitmap, i)) {
            bm_set(inode_bitmap, i);
            inodes[i].used = 1;
            inodes[i].size = 0;
            for (j = 0; j < DIRECT_BLOCKS; j++) inodes[i].blocks[j] = 0;
            return (int32_t)i;
        }
    }
    return -1;
}

static dir_entry_t *dir_find(const char *name) {
    int i;
    for (i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir[i].used && fs_strcmp(dir[i].name, name) == 0) return &dir[i];
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------*/
void fs_init(void) {
    int i;
    ramdisk_init();

    sb           = (superblock_t *)ramdisk_block_ptr(SUPERBLOCK_NUM);
    dir          = (dir_entry_t *)ramdisk_block_ptr(DIRECTORY_BLOCK);
    block_bitmap = (uint8_t *)ramdisk_block_ptr(BLOCK_BITMAP_BLOCK);
    inode_bitmap = (uint8_t *)ramdisk_block_ptr(INODE_BITMAP_BLOCK);
    inodes       = (inode_t *)ramdisk_block_ptr(INODE_TABLE_START);

    sb->magic        = FS_MAGIC;
    sb->total_blocks = TOTAL_BLOCKS;
    sb->total_inodes = MAX_INODES;
    sb->free_blocks  = TOTAL_BLOCKS - DATA_BLOCKS_START;
    sb->free_inodes  = MAX_INODES;

    for (i = 0; i < MAX_DIR_ENTRIES; i++) dir[i].used = 0;
    for (i = 0; i < MAX_INODES; i++)      { inodes[i].used = 0; inodes[i].size = 0; }

    for (i = 0; i < DATA_BLOCKS_START; i++) bm_set(block_bitmap, (uint32_t)i);
    for (i = DATA_BLOCKS_START; i < (int)TOTAL_BLOCKS; i++) bm_clear(block_bitmap, (uint32_t)i);
    for (i = 0; i < MAX_INODES; i++) bm_clear(inode_bitmap, (uint32_t)i);

    for (i = 0; i < MAX_OPEN_FILES; i++) open_files[i].used = 0;
}

int fs_exists(const char *name) {
    return dir_find(name) != 0;
}

int fs_open(const char *name) {
    int i;
    dir_entry_t *entry = dir_find(name);

    if (!entry) {
        int32_t inode_idx;
        if (fs_strlen(name) >= MAX_FILENAME) return -1;

        inode_idx = alloc_inode();
        if (inode_idx < 0) return -1;

        for (i = 0; i < MAX_DIR_ENTRIES; i++) {
            if (!dir[i].used) {
                dir[i].used  = 1;
                fs_strncpy(dir[i].name, name, MAX_FILENAME);
                dir[i].inode = (uint32_t)inode_idx;
                entry = &dir[i];
                break;
            }
        }
        if (!entry) return -1; /* directory full */
    }

    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].used) {
            open_files[i].used  = 1;
            open_files[i].inode = entry->inode;
            open_files[i].pos   = 0;
            return i;
        }
    }
    return -1; /* too many open files */
}

int fs_read(int fd, void *buf, uint32_t len) {
    static uint8_t block_buf[BLOCK_SIZE];   /* static: keeps thread stacks small */
    uint8_t *out = (uint8_t *)buf;
    file_t  *f;
    inode_t *ino;
    uint32_t total = 0;

    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) return -1;
    f   = &open_files[fd];
    ino = &inodes[f->inode];

    while (total < len && f->pos < ino->size) {
        uint32_t block_idx  = f->pos / BLOCK_SIZE;
        uint32_t block_off  = f->pos % BLOCK_SIZE;
        uint32_t remain_blk = BLOCK_SIZE - block_off;
        uint32_t remain_fil = ino->size - f->pos;
        uint32_t chunk      = len - total;
        uint32_t k;

        if (chunk > remain_blk) chunk = remain_blk;
        if (chunk > remain_fil) chunk = remain_fil;
        if (block_idx >= DIRECT_BLOCKS || ino->blocks[block_idx] == 0) break;

        ramdisk_read_block(ino->blocks[block_idx], block_buf);
        for (k = 0; k < chunk; k++) out[total + k] = block_buf[block_off + k];

        total  += chunk;
        f->pos += chunk;
    }
    return (int)total;
}

int fs_write(int fd, const void *buf, uint32_t len) {
    static uint8_t block_buf[BLOCK_SIZE];
    const uint8_t *in = (const uint8_t *)buf;
    file_t  *f;
    inode_t *ino;
    uint32_t total = 0;

    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) return -1;
    f   = &open_files[fd];
    ino = &inodes[f->inode];

    if (ino->size >= MAX_FILE_SIZE) return 0;
    if (ino->size + len > MAX_FILE_SIZE) len = MAX_FILE_SIZE - ino->size;

    while (total < len) {
        uint32_t pos       = ino->size;
        uint32_t block_idx = pos / BLOCK_SIZE;
        uint32_t block_off = pos % BLOCK_SIZE;
        uint32_t remain_blk = BLOCK_SIZE - block_off;
        uint32_t chunk = len - total;
        uint32_t k;
        if (chunk > remain_blk) chunk = remain_blk;
        if (block_idx >= DIRECT_BLOCKS) break;

        if (ino->blocks[block_idx] == 0) {
            int32_t nb = alloc_block();
            if (nb < 0) break;
            ino->blocks[block_idx] = (uint32_t)nb;
            for (k = 0; k < BLOCK_SIZE; k++) block_buf[k] = 0;
        } else {
            ramdisk_read_block(ino->blocks[block_idx], block_buf);
        }

        for (k = 0; k < chunk; k++) block_buf[block_off + k] = in[total + k];
        ramdisk_write_block(ino->blocks[block_idx], block_buf);

        total     += chunk;
        ino->size += chunk;
    }
    f->pos = ino->size;
    return (int)total;
}

void fs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return;
    open_files[fd].used = 0;
}

int fs_unlink(const char *name) {
    int j;
    dir_entry_t *entry = dir_find(name);
    inode_t *ino;
    if (!entry) return -1;

    ino = &inodes[entry->inode];
    for (j = 0; j < DIRECT_BLOCKS; j++) {
        if (ino->blocks[j] != 0) { free_block(ino->blocks[j]); ino->blocks[j] = 0; }
    }
    ino->used = 0;
    ino->size = 0;
    bm_clear(inode_bitmap, entry->inode);
    entry->used = 0;
    return 0;
}

int fs_list(char names[][MAX_FILENAME], uint32_t sizes[], int max_entries) {
    int i, count = 0;
    for (i = 0; i < MAX_DIR_ENTRIES && count < max_entries; i++) {
        if (dir[i].used) {
            fs_strncpy(names[count], dir[i].name, MAX_FILENAME);
            sizes[count] = inodes[dir[i].inode].size;
            count++;
        }
    }
    return count;
}
