
BITS 32

GLOBAL flush_gdt_table:function
GLOBAL flush_tss:function


; Function to set up the GDT
flush_gdt_table:
    cli         ; Disable Interrupts (make sure)
    ; Load the first argument passed to the function (address of GDT pointer) into the EAX register.
    mov dword eax, [esp+4]

    ; Load the GDT pointer from the address in EAX. This sets the GDTR register
    ; This effectively updates the GDT that the CPU uses.
    lgdt [eax]

    ; Load the value 0x10 into the AX register (Kernel Data Segment).
    ; This value is the selector for the data segment in the GDT after setting up protected mode.
    mov ax, 0x10

    ; Load the following register with the value in AX (0x10).
    mov ds, ax  ; DS is now pointing to the data segment described by the GDT entry with selector 0x10.
    mov es, ax  ; Similarly, update the ES (Extra Segment) register.
    mov fs, ax  ; Update the FS (File Segment) register.
    mov gs, ax  ; Update the GS (General Segment) register.
    mov ss, ax  ; Update the SS (Stack Segment) register. This is crucial for stack operations to work correctly in protected mode.

    ; Far jump to the code segment with selector 0x08, and offset flush_gdp_table_2.
    ; This not only jumps to the next instruction but also updates the CS (Code Segment)
    ; register to use the code segment described by the GDT entry with selector 0x08.
    ; Far jumps are used to flush the CPU's prefetch queue and ensure the CS register
    jmp 0x08:.next_instruction

.next_instruction:
    ; return to the caller
    ret

flush_tss:
    mov ax, 5*8     ; TSS at the 6th position of the table
    ltr ax          ; Load Task Register (TSS)
    ret
