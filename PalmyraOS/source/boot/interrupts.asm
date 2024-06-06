BITS 32


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


; load the pointer to the IDT table to the CPU
flush_idt_table:

    mov dword eax, [esp+4]      ; Load the first argument passed to the function
                                ; here (address of IDT) from the stack to the EAX registe.

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


; For Interrupt Stubs that already push an Error Code (8, 10, ..., 14)
%macro InterruptServiceRoutine_PushesErrorCode 1
global InterruptServiceRoutine_%1       ; expose the function
InterruptServiceRoutine_%1:
    cli                                 ; clear the interrupts
                                        ; error code was pushed automatically
    push dword %1                        ; push interrupt number
    jmp _primary_isr_handler
%endmacro

; For Interrupt Stubs that don't push an Error code
%macro InterruptServiceRoutine_NoErrorCode 1
global InterruptServiceRoutine_%1
InterruptServiceRoutine_%1:
    cli                                 ; Clear interrupts to prevent nesting
    push dword 0                         ; Push dummy error code to standardize stack frame
    push dword %1                        ; Push the interrupt number onto the stack
    jmp _primary_isr_handler
%endmacro
; ---------------------- END: ISR MACROS ----------------------


; Actual functions


_primary_isr_handler:
    pusha                       ; preserve general purpose registers        [eax, ecx, edx, ebx, esp, ebp, esi, edi]
    push_data_segment           ; preserve data segment registers (GDT)     [ds, es, fs, gs]

    ; ss and cs are pushed automatically by TSS (if prev CPL=3)
    ; load kernel data segments
    mov ax, 16
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                    ; Push the stack pointer onto the stack, effectively passing a pointer
                                ; to the current stack (where all registers and possibly the error code are saved)
                                ; to the primary_isr_handler function as a parameter.


    call primary_isr_handler    ; Call the C++ primary handler. This function is expected to handle the interrupt
                                ; with all necessary context provided by the stack pointer argument.

    ; pop esp                     ; Clean up the stack pointer from the stack after the call returns.
    add esp, 4                            ; This restores the original ESP value which pointed to the interrupt context.

    pop_data_segment            ; restore data segment registers
    popa                        ; restore general purpose registers

    add esp, 8                  ; Clean up the `error code` and `interrupt number` from the stack

    iret                        ; interrupt return

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
; Coprocessor Segment Overrun
InterruptServiceRoutine_PushesErrorCode 0x08 ; Double Fault (Abort)
InterruptServiceRoutine_PushesErrorCode 0x0A ; Invalid TSS (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0B ; Segment Not Present (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0C ; Stack-Segment Fault (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0D ; General Protection Fault (Fault)
InterruptServiceRoutine_PushesErrorCode 0x0E ; Page Fault (Fault)
; reserved ...
InterruptServiceRoutine_NoErrorCode 0x20        ; IRQ0 timer
InterruptServiceRoutine_NoErrorCode 0x21        ; IRQ1 keyboard
InterruptServiceRoutine_NoErrorCode 0x22
InterruptServiceRoutine_NoErrorCode 0x23
InterruptServiceRoutine_NoErrorCode 0x24
InterruptServiceRoutine_NoErrorCode 0x25
InterruptServiceRoutine_NoErrorCode 0x26
InterruptServiceRoutine_NoErrorCode 0x27
InterruptServiceRoutine_NoErrorCode 0x28
InterruptServiceRoutine_NoErrorCode 0x29
InterruptServiceRoutine_NoErrorCode 0x2A
InterruptServiceRoutine_NoErrorCode 0x2B
InterruptServiceRoutine_NoErrorCode 0x2C        ; IRQ12 Mouse
InterruptServiceRoutine_NoErrorCode 0x2D
InterruptServiceRoutine_NoErrorCode 0x2E
InterruptServiceRoutine_NoErrorCode 0x2F

InterruptServiceRoutine_NoErrorCode 0x80