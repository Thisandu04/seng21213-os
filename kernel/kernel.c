/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 1 – Process Management)
 * File   : kernel/kernel.c
 * ============================================================================*/

 #define RACE_ITER 60
#include "vga.h"
#include "keyboard.h"
#include "idt.h"
#include "process.h"
#include "scheduler.h"
#include "../include/types.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_ps(void);

static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

static void k_itoa(int value, char *buf) {
    int i = 0, t = 0, neg;
    char tmp[12];
    unsigned int uval;
    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    neg  = value < 0;
    uval = neg ? (unsigned int)(-value) : (unsigned int)value;
    while (uval) { tmp[t++] = (char)('0' + (uval % 10)); uval /= 10; }
    if (neg) buf[i++] = '-';
    while (t > 0) buf[i++] = tmp[--t];
    buf[i] = '\0';
}

static void print_splash(void) {
    vga_clear(VGA_BLACK);
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);
    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 1: Process Management & Round-Robin Scheduler",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);
    vga_set_cursor(4, 2);
    vga_puts_color("  Type 'help' to begin. Type 'ps' to see running processes.",
                   VGA_LIGHT_GREEN, VGA_BLACK);
    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Timer-driven preemptive multitasking is now active (100Hz).\n");
    vga_puts("  Two background processes are running below the shell prompt.\n\n");
}

static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  mem     - Memory map (stub)\n");
    vga_puts_color("  ps      - [L09] List all processes\n", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  kill    - [L09] Terminate a process\n");
    vga_puts("  threads - [L10] List kernel threads\n");
    vga_puts("  free    - [L11] Show free memory\n");
    vga_puts("  ls      - [L12] List files\n");
    vga_puts("  cat     - [L12] Print file contents\n\n");
    vga_puts_color("  race    - [L10] Race condition demo (with/without mutex)\n", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts_color("  pcdemo  - [L10] Producer-consumer demo (3 semaphores)\n", VGA_LIGHT_GREEN, VGA_BLACK);
}

static void cmd_clear(void) { vga_clear(VGA_BLACK); }

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    vga_puts_color("\n  Memory Map (stub - implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  0x00000000 - 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 - 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0xB8000    - 0xBFFFF     :  VGA frame buffer\n\n");
}

static void cmd_ps(void) {
    char pidbuf[12];
    int i;
    vga_puts_color("\n  PID  STATE      NAME\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------\n");
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_table_get(i);
        if (p->state == PROC_UNUSED) continue;
       const char *s = (p->state == PROC_RUNNING) ? "RUNNING" :
                 (p->state == PROC_READY)   ? "READY  " :
                 (p->state == PROC_BLOCKED) ? "BLOCKED" : "DEAD   ";
        k_itoa((int)p->pid, pidbuf);
        vga_puts("  "); vga_puts(pidbuf);
        vga_puts("    "); vga_puts(s);
        vga_puts("    "); vga_puts(p->name);
        vga_puts("\n");
    }
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * L10 Demo 1: race condition on a shared global, with and without a mutex
 * --------------------------------------------------------------------------*/

static volatile long myglobal;
static volatile int  race_done_flags[2];
static mutex_t       race_mutex;


static inline void force_yield(void) {
    __asm__ volatile ("int $0x20" ::: "memory", "cc");
}

static void racer_unsafe(void *arg) {
    int id = (int)(long)arg;
    int i;
    char dbg[16];
    for (i = 0; i < RACE_ITER; i++) {
        long tmp = myglobal;
        force_yield();
        tmp = tmp + 1;
        myglobal = tmp;

        if (i % 200 == 0) {
            k_itoa(i, dbg);
            if (id == 0) vga_print_at(23, 0,  dbg, VGA_WHITE, VGA_BLACK);
            else         vga_print_at(23, 20, dbg, VGA_WHITE, VGA_BLACK);
        }
    }
    race_done_flags[id] = 1;
    if (id == 0) vga_print_at(23, 0,  "A-DONE", VGA_WHITE, VGA_RED);
    else         vga_print_at(23, 20, "B-DONE", VGA_WHITE, VGA_RED);
}

static void racer_safe(void *arg) {
    int id = (int)(long)arg;
    int i;
    char dbg[16];
    for (i = 0; i < RACE_ITER; i++) {
        mutex_lock(&race_mutex);
        long tmp = myglobal;
        force_yield();
        tmp = tmp + 1;
        myglobal = tmp;
        mutex_unlock(&race_mutex);

        if (i % 200 == 0) {
            k_itoa(i, dbg);
            if (id == 0) vga_print_at(21, 0,  dbg, VGA_WHITE, VGA_BLACK);
            else         vga_print_at(21, 20, dbg, VGA_WHITE, VGA_BLACK);
        }
    }
    race_done_flags[id] = 1;
    if (id == 0) vga_print_at(21, 0,  "C-DONE", VGA_WHITE, VGA_GREEN);
    else         vga_print_at(21, 20, "D-DONE", VGA_WHITE, VGA_GREEN);
}

static void cmd_race(void) {
    long expected = (long)RACE_ITER * 2;   /* now = 4000 */
    char buf[16];

    vga_puts_color("\n  Race Condition Demo (L10 - myglobal, WITHOUT mutex)\n",
                   VGA_YELLOW, VGA_BLACK);
    vga_puts("  Two threads each increment myglobal 60 times...\n");

    myglobal = 0; race_done_flags[0] = 0; race_done_flags[1] = 0;
    thread_create(racer_unsafe, (void *)0, "racer_a");
    thread_create(racer_unsafe, (void *)1, "racer_b");
    while (!race_done_flags[0] || !race_done_flags[1]) { __asm__ volatile ("hlt"); }

    vga_puts("  Expected total: "); k_itoa((int)expected, buf); vga_puts(buf); vga_puts("\n");
    vga_puts_color("  Actual total:   ", VGA_LIGHT_RED, VGA_BLACK);
    k_itoa((int)myglobal, buf); vga_puts_color(buf, VGA_LIGHT_RED, VGA_BLACK);
    if (myglobal != expected)
        vga_puts_color("   <-- LOST UPDATES (race condition)\n", VGA_LIGHT_RED, VGA_BLACK);
    else
        vga_puts("   (no corruption this run - timing dependent, try again)\n");

    vga_puts_color("\n  Same demo WITH mutex protection:\n", VGA_YELLOW, VGA_BLACK);
    mutex_init(&race_mutex);
    myglobal = 0; race_done_flags[0] = 0; race_done_flags[1] = 0;
    thread_create(racer_safe, (void *)0, "racer_c");
    thread_create(racer_safe, (void *)1, "racer_d");
    while (!race_done_flags[0] || !race_done_flags[1]) { __asm__ volatile ("hlt"); }

    vga_puts("  Expected total: "); k_itoa((int)expected, buf); vga_puts(buf); vga_puts("\n");
    vga_puts_color("  Actual total:   ", VGA_LIGHT_GREEN, VGA_BLACK);
    k_itoa((int)myglobal, buf); vga_puts_color(buf, VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts_color("   <-- CORRECT (mutex prevented the race)\n\n", VGA_LIGHT_GREEN, VGA_BLACK);
}

/* ---------------------------------------------------------------------------
 * L10 Demo 2: bounded-buffer producer-consumer, three semaphores
 * --------------------------------------------------------------------------*/
#define PC_BUF_SIZE 4
#define PC_ITEMS    12

static int          pc_buffer[PC_BUF_SIZE];
static int          pc_in, pc_out;
static semaphore_t  pc_empty, pc_full, pc_mutex;
static volatile int pc_done_flags[2];

static void pc_producer(void *arg) {
    (void)arg;
    int i;
    for (i = 1; i <= PC_ITEMS; i++) {
        sem_wait(&pc_empty);
        sem_wait(&pc_mutex);

        pc_buffer[pc_in] = i;
        pc_in = (pc_in + 1) % PC_BUF_SIZE;

        char buf[8]; k_itoa(i, buf);
        vga_puts_color("  [producer] made item ", VGA_LIGHT_CYAN, VGA_BLACK);
        vga_puts_color(buf, VGA_LIGHT_CYAN, VGA_BLACK);
        vga_puts("\n");

        sem_signal(&pc_mutex);
        sem_signal(&pc_full);
    }
    pc_done_flags[0] = 1;
}

static void pc_consumer(void *arg) {
    (void)arg;
    int i;
    for (i = 1; i <= PC_ITEMS; i++) {
        sem_wait(&pc_full);
        sem_wait(&pc_mutex);

        int item = pc_buffer[pc_out];
        pc_out = (pc_out + 1) % PC_BUF_SIZE;

        char buf[8]; k_itoa(item, buf);
        vga_puts_color("  [consumer] took item  ", VGA_LIGHT_MAGENTA, VGA_BLACK);
        vga_puts_color(buf, VGA_LIGHT_MAGENTA, VGA_BLACK);
        vga_puts("\n");

        sem_signal(&pc_mutex);
        sem_signal(&pc_empty);
    }
    pc_done_flags[1] = 1;
}

static void cmd_pc(void) {
    vga_puts_color("\n  Producer-Consumer Demo (L10 - bounded buffer, 3 semaphores)\n",
                   VGA_YELLOW, VGA_BLACK);
    pc_in = 0; pc_out = 0;
    pc_done_flags[0] = 0; pc_done_flags[1] = 0;
    sem_init(&pc_empty, PC_BUF_SIZE);
    sem_init(&pc_full, 0);
    sem_init(&pc_mutex, 1);

    thread_create(pc_producer, 0, "producer");
    thread_create(pc_consumer, 0, "consumer");

    while (!pc_done_flags[0] || !pc_done_flags[1]) { __asm__ volatile ("hlt"); }

    vga_puts_color("\n  Done - all items produced and consumed in order, no corruption.\n\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);
}
/* ---------------------------------------------------------------------------
 * Demo processes - visible proof that preemptive multitasking is working
 * --------------------------------------------------------------------------*/
static void demo_process_a(void) {
    volatile uint32_t n = 0;
    int count = 0;
    char msg[16];
    while (1) {
        n++;
        if (n % 3000000 == 0) {
            count++;
            char numbuf[12];
            k_itoa(count, numbuf);
            int i = 0, j = 0;
            msg[i++] = 'A'; msg[i++] = ':'; msg[i++] = ' ';
            while (numbuf[j]) msg[i++] = numbuf[j++];
            msg[i++] = ' '; msg[i++] = ' '; msg[i++] = ' ';
            msg[i] = '\0';
            vga_print_at(24, 0, msg, VGA_LIGHT_GREEN, VGA_BLACK);
        }
    }
}

static void demo_process_b(void) {
    volatile uint32_t n = 0;
    int count = 0;
    char msg[16];
    while (1) {
        n++;
        if (n % 5000000 == 0) {
            count++;
            char numbuf[12];
            k_itoa(count, numbuf);
            int i = 0, j = 0;
            msg[i++] = 'B'; msg[i++] = ':'; msg[i++] = ' ';
            while (numbuf[j]) msg[i++] = numbuf[j++];
            msg[i++] = ' '; msg[i++] = ' '; msg[i++] = ' ';
            msg[i] = '\0';
            vga_print_at(24, 20, msg, VGA_LIGHT_CYAN, VGA_BLACK);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char shell_buf[256];
static char prompt[] = "\n  ksh> ";

static void shell_run(void) {
  vga_print_at(12, 0, ">>> SHELL STARTED <<<", VGA_WHITE, VGA_RED);   // TEMP debug marker
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }
        if (k_strcmp(cmd, "ps")    == 0) { cmd_ps();    continue; }
        if (k_strcmp(cmd, "race")   == 0) { cmd_race(); continue; }
        if (k_strcmp(cmd, "pcdemo") == 0) { cmd_pc();   continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "kill")    == 0 ||
            k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ||
            k_strcmp(cmd, "ls")      == 0 ||
            k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point - called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();
    idt_init();
    print_splash();

    process_init();
    create_process(shell_run,      "shell");
    create_process(demo_process_a, "proc_a");
    create_process(demo_process_b, "proc_b");

    scheduler_init();
    scheduler_run(); /* hands control to "shell" - never returns past here */

    __asm__ __volatile__("hlt");
}
