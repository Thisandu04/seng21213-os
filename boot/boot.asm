; =============================================================================
; SENG21213-OS :: Bootloader (Stage 0 boot/GDT + Stage 3 E820 memory detection)
; File   : boot/boot.asm
; =============================================================================

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    sti

    mov  [boot_drive], dl

    mov  si, msg_banner
    call print_rm
    mov  si, msg_load
    call print_rm

load_kernel:
    mov  bx, 0x1000
    mov  es, bx
    xor  bx, bx

    mov  ah, 0x02
    mov  al, 64
    mov  ch, 0
    mov  cl, 2
    mov  dh, 0
    mov  dl, [boot_drive]
    int  0x13
    jc   disk_error

    mov  si, msg_ok
    call print_rm

; ---------------------------------------------------------------------------
; Lecture 11: Detect the BIOS memory map (E820) BEFORE entering protected
; mode. Results land at physical 0x8000 for the kernel's pmm_init() to read.
; Runs silently (no status text) to keep this file under 512 bytes.
; ---------------------------------------------------------------------------
    xor  bx, bx
    mov  es, bx
    call detect_memory

enter_pm:
    cli
    lgdt [gdt_descriptor]

    mov  eax, cr0
    or   eax, 0x1
    mov  cr0, eax

    jmp  CODE_SEG:init_pm32

[BITS 32]
init_pm32:
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    mov  ebp, 0x90000
    mov  esp, ebp

    call 0x10000
    hlt

[BITS 16]
disk_error:
    mov  si, msg_err
    call print_rm
    mov  si, msg_halt
    call print_rm
    jmp  $

print_rm:
    lodsb
    or   al, al
    jz   .done
    mov  ah, 0x0E
    xor  bh, bh
    int  0x10
    jmp  print_rm
.done:
    ret

; ---------------------------------------------------------------------------
; detect_memory - BIOS INT 0x15, EAX=0xE820 memory map query.
;
; Buffer layout starting at physical 0x8000:
;   word  [0x8000]       = number of entries found
;   entries @ 0x8004, 24 bytes each:
;       uint64 base | uint64 length | uint32 type (1=usable) | uint32 acpi_ext
; ---------------------------------------------------------------------------
MAX_E820_ENTRIES equ 64

detect_memory:
    pusha
    mov   di, 0x8004
    xor   ebx, ebx
    xor   bp, bp

.query_entry:
    mov   dword [es:di + 20], 1
    mov   eax, 0xE820
    mov   edx, 0x534D4150
    mov   ecx, 24
    int   0x15
    jc    .list_done

    cmp   eax, 0x534D4150
    jne   .list_done

    cmp   bp, MAX_E820_ENTRIES
    jae   .list_done

    cmp   cl, 20
    jbe   .store_entry
    test  byte [es:di + 20], 1
    je    .skip_entry

.store_entry:
    mov   ecx, [es:di + 8]
    or    ecx, [es:di + 12]
    jz    .skip_entry

    inc   bp
    add   di, 24

.skip_entry:
    test  ebx, ebx
    jz    .list_done
    jmp   .query_entry

.list_done:
    mov   [0x8000], bp
    popa
    ret

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
boot_drive  db 0

msg_banner  db 13, 10, '  SENG21213-OS - Stage 3 - Booting...', 13, 10, 0
msg_load    db '  [BOOT] Loading kernel...', 13, 10, 0
msg_ok      db '  [BOOT] Kernel loaded OK ', 13, 10, 0
msg_err     db '  [BOOT] DISK ERROR!       ', 13, 10, 0
msg_halt    db '  System halted.           ', 13, 10, 0

; ---------------------------------------------------------------------------
; GDT
; ---------------------------------------------------------------------------
gdt_start:
gdt_null:
    dd 0x00000000
    dd 0x00000000

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
