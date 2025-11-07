; Compile 32 bit instructions
BITS 32

; ============================================================================
; MULTIBOOT 2 HEADER
; ============================================================================
; Multiboot 2 specification: 
; https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
;
; The header must be 8-byte aligned and come before the first 32KB of the kernel

%define MB2_MAGIC           0xE85250D6         ; Multiboot 2 magic number
%define MB2_ARCHITECTURE    0                  ; i386 protected mode
%define MB2_HEADER_LENGTH   (multiboot2_header_end - multiboot2_header_start)
%define MB2_CHECKSUM        -(MB2_MAGIC + MB2_ARCHITECTURE + MB2_HEADER_LENGTH)

; Multiboot 2 header tag types
%define MB2_TAG_END             0
%define MB2_TAG_INFO_REQUEST    1
%define MB2_TAG_FRAMEBUFFER     5

; Tag flags
%define MB2_TAG_OPTIONAL        1
%define MB2_TAG_REQUIRED        0

section .multiboot2 align=8
multiboot2_header_start:
    ; Required header fields (16 bytes)
    dd MB2_MAGIC                               ; Magic: 0xE85250D6
    dd MB2_ARCHITECTURE                        ; Architecture: 0 (i386 protected mode)
    dd MB2_HEADER_LENGTH                       ; Header length
    dd MB2_CHECKSUM                            ; Checksum: -(magic + arch + length)

    ; -------- Tag: Information Request --------
    ; Request specific tags from bootloader
    align 8
information_request_tag_start:
    dw MB2_TAG_INFO_REQUEST                    ; Type: Information request
    dw MB2_TAG_OPTIONAL                        ; Flags: Optional
    dd information_request_tag_end - information_request_tag_start  ; Size
    ; Requested tags (list of tag types we want)
    dd 4                                       ; Basic memory info
    dd 6                                       ; Memory map
    dd 8                                       ; Framebuffer
    dd 14                                      ; ACPI old RSDP
    dd 15                                      ; ACPI new RSDP
information_request_tag_end:

    ; -------- Tag: Framebuffer Request --------
    ; Request specific video mode from bootloader
    align 8
framebuffer_tag_start:
    dw MB2_TAG_FRAMEBUFFER                     ; Type: Framebuffer
    dw MB2_TAG_OPTIONAL                        ; Flags: Optional (fallback to defaults)
    dd framebuffer_tag_end - framebuffer_tag_start  ; Size
    dd 1024                                    ; Width: 1024 pixels
    dd 768                                     ; Height: 768 pixels
    dd 32                                      ; Depth: 32 bits per pixel (ARGB)
framebuffer_tag_end:

    ; -------- Tag: End (Required) --------
    ; Marks the end of the header tags
    align 8
end_tag_start:
    dw MB2_TAG_END                             ; Type: End
    dw MB2_TAG_REQUIRED                        ; Flags: Required
    dd end_tag_end - end_tag_start             ; Size: 8 bytes
end_tag_end:

multiboot2_header_end:

; ============================================================================

; Globals
GLOBAL kernel_start:function
GLOBAL get_kernel_stack_start:function
GLOBAL get_kernel_stack_end:function
GLOBAL enable_protected_mode:function

; Externs from CPP
EXTERN kernelEntry

; Multiboot 2 info storage
multiboot2_magic        dd 0
multiboot2_info_addr    dd 0

; Stack grows down!
; Leave empty 4 MByte before the kernel stack starts

kernel_stack_end:
    times 4*1024*1024 db 0  ; Allocates Space for the stack
kernel_stack_start:

section .text
kernel_start:
    ; ========================================================================
    ; Multiboot 2 Entry Point
    ; ========================================================================
    ; When bootloader calls this function:
    ;   EAX = Multiboot 2 magic number (0x36d76289)
    ;   EBX = Physical address of Multiboot 2 info structure
    ;   CS  = 32-bit read/execute code segment with offset 0 and limit 0xFFFFFFFF
    ;   DS,ES,FS,GS,SS = 32-bit read/write data segment with offset 0 and limit 0xFFFFFFFF
    ;   A20 gate = enabled
    ;   CR0 = PE bit set, PG bit clear
    ;   EFLAGS = VM and IF bits clear
    ; ========================================================================

    ; Save Multiboot 2 magic number and info structure address
    mov dword [multiboot2_magic], eax
    mov dword [multiboot2_info_addr], ebx

    ; Set up the stack pointer (ESP) to the top of our 4MB stack
    ; Stack grows downward, so we point to the highest address
    mov esp, kernel_stack_start

    ; Pass Multiboot 2 parameters to kernelEntry(uint32_t magic, uint32_t addr)
    ; C calling convention: arguments pushed right-to-left
    push ebx        ; Second parameter: Multiboot 2 info address
    push eax        ; First parameter: Multiboot 2 magic number
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