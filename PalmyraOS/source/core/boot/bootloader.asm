; Compile 32 bit instructions
BITS 32

; -------------------- MULTIBOOT HEADER --------------------
; https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
; So that grub bootloader sees this binary as a 'bootloader'
; Multiboot macros
%define MB_MAGIC    0x1BADB002
%define MB_FLAG_ALIGN    1 << 0
%define MB_FLAG_MEMINFO  1 << 1
%define MB_FLAG_CMDLINE  1 << 2
%define MB_FLAG_GRAPHICS 1 << 16
%define MB_FLAGS     (MB_FLAG_CMDLINE |  MB_FLAG_ALIGN | MB_FLAG_MEMINFO )

; Calculate the checksum
%define MB_CHECKSUM -(MB_MAGIC + MB_FLAGS)

; Multiboot header structure
section .multiboot align=4
dd MB_MAGIC                  ; Magic number required by Multiboot
dd MB_FLAGS                  ; Flags - 0x00040003 in this case
dd MB_CHECKSUM               ; Checksum (must be 0 when added to the above two)

; Graphics mode specific fields (not typically required for basic setup but can be used for VBE)
dd 0                         ; VbeControlInfo
dd 0                         ; VbeModeInfo
dd 0                         ; VbeMode
dd 0                         ; VbeInterfaceSeg
dd 0                         ; VbeInterfaceOff

; Screen width, height, depth (if you want GRUB to set a specific video mode)
dd 0                         ; 0: Graphics, 1: Text Mode

; If an unsupported resolution provided, GRUB falls back to 640x480
; dd 1600                      ; Screen width requested
; dd 1200                       ; Screen height requested

; dd 1280                      ; Screen width requested
; dd 1024                       ; Screen height requested

dd 1024                     ; Screen width requested
dd 768                       ; Screen height requested

; dd 800                      ; Screen width requested
; dd 600                       ; Screen height requested

dd 32                        ; Bits per pixel

; ----------------------------------------------------------

; Globals
GLOBAL kernel_start:function
GLOBAL get_kernel_stack_start:function
GLOBAL get_kernel_stack_end:function
GLOBAL enable_protected_mode:function

; Externs from CPP
EXTERN kernelEntry

multiboot_info_struct   dd 0
multiboot_info_high     dd 0
multiboot_info_low      dd 0

; Stack grows down!
; Leave empty 4 MByte before the kernel stack starts

kernel_stack_end:
    times 4*1024*1024 db 0  ; Allocates Space for the stack
kernel_stack_start:

section .text
kernel_start:
    ; at the start:

    ; multiboot_info_struct := ebx (temporarily)
    mov dword [multiboot_info_struct], ebx

    ; set up the stack pointer (esp) 4MB after memory start.
    ; Before calling C++ functions (hence push/pop to stack)
    ; esp = kernel_stack_start
    mov dword esp, kernel_stack_start     


    ; start the kernel and pass pointer to multiboot info
    push ebx
    call kernelEntry

; endless loop
_stop:
    cli
    hlt
    jmp _stop

; Function to get the start address of the stack
get_stack_start:
    mov eax, kernel_stack_start
    ret

; Function to get the end address of the stack
get_stack_end:
    mov eax, kernel_stack_end
    ret


; Function to enable the protected mode
; Protection Enable bit (PE) is the least significant bit of Control Register 0 (CR0)
enable_protected_mode:
    push eax            ; temporarily save eax onto the stack (as we will use the register)

    mov dword eax, cr0  ; Load the current value of the CR0 register into eax (cr0 cannot perform arithmetic operations)
    or eax, 1           ; Set the Protection Enable (PE) bit in eax
    mov dword cr0, eax  ; Update the CR0 register with the modified value (enabling protected mode)

    pop eax             ; restore the original value of eax from the stack
    ret

; Function to retrive the kernel stack start
get_kernel_stack_start:
    mov eax, kernel_stack_start
    ret

; Function to retrive the kernel stack end
get_kernel_stack_end:
    mov eax, kernel_stack_end
    ret