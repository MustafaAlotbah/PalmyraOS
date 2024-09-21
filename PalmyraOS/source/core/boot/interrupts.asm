BITS 32

; Once an interrupt fires, all other interrupts stop, unless iret or sti

; Define global functions for handling interrupts
global enable_interrupts:function
global disable_interrupts:function
global flush_idt_table:function
global enable_interrupts:function
global disable_interrupts:function
global _default_isr_handler:function

; from CPP
extern primary_isr_handler

; Enables interrupts and returns to the caller. (After IDT is set up)
enable_interrupts:
    sti         ; Set Interrupts
    nop         ; No operation (just wait a bit)
    ret         ; return

; Disables interrupts and returns to the caller.
disable_interrupts:
    cli         ; Clear Interrupts
    ret         ; return


; Load the pointer to the IDT table to the CPU
flush_idt_table:

    mov dword eax, [esp+4]      ; Load the first argument passed to the function
                                ; here (address of IDT) from the stack to the EAX register.

    lidt [eax]                  ; Load the IDT pointer to its register

    ret                         ; Return to the called

; --------------------- BEGIN: ISR MACROS ---------------------

%macro push_data_segment 0
    push ds
    push es
    push fs
    push gs
%endmacro

%macro pop_data_segment 0
    pop gs
    pop fs
    pop es
    pop ds
%endmacro


; Macro for Interrupt Stubs that already push an Error Code (8, 10, ..., 14)
%macro InterruptServiceRoutine_PushesErrorCode 1
global InterruptServiceRoutine_%1       ; expose the function
InterruptServiceRoutine_%1:
    cli                                 ; clear the interrupts TODO fetch kernel directory if available
                                        ; error code was pushed automatically
    push dword %1                       ; push interrupt number
    jmp _primary_isr_handler
%endmacro

; Macro for Interrupt Stubs that don't push an Error code
%macro InterruptServiceRoutine_NoErrorCode 1
global InterruptServiceRoutine_%1
InterruptServiceRoutine_%1:
    cli                                  ; Clear interrupts to prevent nesting
    push dword 0                         ; Push dummy error code to standardize stack frame
    push dword %1                        ; Push the interrupt number onto the stack
    jmp _primary_isr_handler
%endmacro
; ---------------------- END: ISR MACROS ----------------------


; Actual ISR handler functions


_primary_isr_handler:
    pusha                       ; preserve general purpose registers        [eax, ecx, edx, ebx, esp, ebp, esi, edi]
    push_data_segment           ; preserve data segment registers (GDT)     [ds, es, fs, gs]

    ; Load kernel data segments (ss and cs are pushed automatically by TSS if prev CPL=3)
    mov ax, 16
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, cr3                ; Push cr3 register value
    push eax

    push esp                    ; Push the stack pointer onto the stack, effectively passing a pointer
                                ; to the current stack (where all registers and possibly the error code are saved)
                                ; to the primary_isr_handler function as a parameter.



    call primary_isr_handler    ; Call the C++ primary handler. This function is expected to handle the interrupt
                                ; with all necessary context provided by the stack pointer argument.

    ; Switch stack pointer
    mov esp, eax

    ; pop esp                   ; Clean up the stack pointer from the stack after the call returns.
    add esp, 4                  ; This restores the original ESP value which pointed to the interrupt context.

    pop eax                     ; Pop cr3
    mov ebx, cr3

    cmp eax, ebx                ; Compare old cr3 with current cr3
    je SkipCR3Write             ; If no change, avoid the overhead of rewriting to CR3

    mov cr3, eax                ; Update CR3 Register (Paging Directory Pointer)

SkipCR3Write:
    pop_data_segment            ; Restore data segment registers
    popa                        ; Restore general purpose registers

    add esp, 8                  ; Clean up the `error code` and `interrupt number` from the stack

    iret                        ; Interrupt return

_default_isr_handler:
    iret

; Pre-prelimary interrupts registration
InterruptServiceRoutine_NoErrorCode 0x00 ; Division Error (Fault)
InterruptServiceRoutine_NoErrorCode 0x01 ; Debug (Fault/Trap)
InterruptServiceRoutine_NoErrorCode 0x02 ; Non-maskable Interrupt (Interrupt)
InterruptServiceRoutine_NoErrorCode 0x03 ; Breakpoint (Trap)
InterruptServiceRoutine_NoErrorCode 0x04 ; Overflow (Trap)
InterruptServiceRoutine_NoErrorCode 0x05 ; Bound Range Exceeded (Fault)
InterruptServiceRoutine_NoErrorCode 0x06 ; Invalid Opcode (Fault)
InterruptServiceRoutine_NoErrorCode 0x07 ; Device Not Available	(Fault)
InterruptServiceRoutine_NoErrorCode 0x09 ; Coprocessor Segment Overrun (Fault)
InterruptServiceRoutine_PushesErrorCode 0x08 ; Double Fault (Abort)
InterruptServiceRoutine_PushesErrorCode 0x0A ; Invalid TSS (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0B ; Segment Not Present (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0C ; Stack-Segment Fault (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0D ; General Protection Fault (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0E ; Page Fault (Fault)
; reserved ...
InterruptServiceRoutine_NoErrorCode 0x20        ; IRQ0 timer
InterruptServiceRoutine_NoErrorCode 0x21        ; IRQ1 keyboard
InterruptServiceRoutine_NoErrorCode 0x22        ; IRQ2 cascade for IRQs 8-15
InterruptServiceRoutine_NoErrorCode 0x23        ; IRQ3 serial port 2
InterruptServiceRoutine_NoErrorCode 0x24        ; IRQ4 serial port 1
InterruptServiceRoutine_NoErrorCode 0x25        ; IRQ5 parallel port 2 / sound card
InterruptServiceRoutine_NoErrorCode 0x26        ; IRQ6 floppy disk
InterruptServiceRoutine_NoErrorCode 0x27        ; IRQ7 parallel port 1 / sound card
InterruptServiceRoutine_NoErrorCode 0x28        ; IRQ8 real-time clock
InterruptServiceRoutine_NoErrorCode 0x29        ; IRQ9 free for peripherals / re-purposed
InterruptServiceRoutine_NoErrorCode 0x2A        ; IRQ10 free for peripherals / re-purposed
InterruptServiceRoutine_NoErrorCode 0x2B        ; IRQ11 free for peripherals / re-purposed
InterruptServiceRoutine_NoErrorCode 0x2C        ; IRQ12 Mouse
InterruptServiceRoutine_NoErrorCode 0x2D        ; IRQ13 FPU / coprocessor / inter-processor
InterruptServiceRoutine_NoErrorCode 0x2E        ; IRQ14 primary ATA channel
InterruptServiceRoutine_NoErrorCode 0x2F        ; IRQ15 secondary ATA channel

InterruptServiceRoutine_NoErrorCode 0x80        ; System call (trap)