BITS 32


; Define global functions for handling interrupts
global enable_interrupts:function
global disable_interrupts:function


; Enables interrupts and returns to the caller. (After IDT is set up)
enable_interrupts:
    sti         ; Set Interrupts
    nop         ; No operation (just wait a bit)
    ret         ; return

; Disables interrupts and returns to the caller.
disable_interrupts:
    cli         ; Clear Interrupts
    ret         ; return
