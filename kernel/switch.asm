; =============================================================================
; SENG21213-OS :: Context Switch + Timer IRQ handler
; File   : kernel/switch.asm
; =============================================================================
[BITS 32]

global irq0_entry
global scheduler_start
extern current_process
extern scheduler_next_pcb
extern pic_send_eoi

; The CPU auto-pushes EIP, CS, EFLAGS before landing here on IRQ0.
irq0_entry:
    pushad
    mov eax, [current_process]
    mov [eax], esp             ; pcb->esp = outgoing stack pointer (offset 0)

    call pic_send_eoi
    call scheduler_next_pcb    ; picks next READY pcb, updates current_process

    mov eax, [current_process]
    mov esp, [eax]              ; load incoming process's stack pointer

    popad
    iretd

; Called ONCE from kernel_main to "become" the first process. Never returns.
scheduler_start:
    mov eax, [current_process]
    mov esp, [eax]
    popad
    iretd


    global thread_trampoline
extern thread_exit

; Reads the real entry fn (ESI) and its argument (EBX) out of the registers
; POPAD just restored, makes a normal cdecl call, then terminates cleanly.
thread_trampoline:
    push ebx          ; arg
    call esi          ; call the real thread entry function
    add esp, 4         ; caller cleans up (cdecl)
    call thread_exit   ; mark TERMINATED, halt forever
.thread_hang:
    hlt
    jmp .thread_hang
