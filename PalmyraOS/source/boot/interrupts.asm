BITS 32


global enable_interrupts:function
global disable_interrupts:function


; Enables interrupts and returns to the caller. (After IDT is set up)
enable_interrupts:
    sti         ; Set Interrupts
    nop         ; just wait a bit
    ret         ; return

disable_interrupts:
    cli         ; Clear Interrupts
    ret         ; return
