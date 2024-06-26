BITS 32

GLOBAL set_page_directory:function
GLOBAL enable_paging:function
GLOBAL disable_paging:function
GLOBAL is_paging_enabled:function
GLOBAL get_esp:function
GLOBAL get_ss:function
GLOBAL get_cr3:function

set_page_directory:
    mov dword eax, [esp+4]
    mov dword cr3, eax
    ret

enable_paging:
    push eax            ; Preserve the value of EAX
    mov dword eax, cr0  ; Load the current value of CR0
    or eax, 0x80000000  ; Set the PG bit to enable paging
    mov cr0, eax        ; Write back the modified value to CR0
    pop eax             ; Restore the original value of EAX
    ret                 ; Return from the function

; Function to disable paging
disable_paging:
    push eax            ; Preserve the value of EAX
    mov eax, cr0        ; Load the current value of CR0
    and eax, 0x7FFFFFFF ; Clear the PG bit to disable paging
    mov cr0, eax        ; Write back the modified value to CR0
    pop eax             ; Restore the original value of EAX
    ret                 ; Return from the function


is_paging_enabled:
    push eax            ; Preserve the value of EAX
    mov eax, cr0        ; Load the current value of CR0 into EAX
    and eax, 0x80000000 ; Isolate the PG bit (bit 31)
    shr eax, 31         ; Shift the PG bit to the least significant bit
    mov ecx, eax        ; Save the result (0 or 1) into ECX
    pop eax             ; Restore the original value of EAX
    mov eax, ecx        ; Move the result from ECX to EAX
    ret                 ; Return from the function with EAX holding the result (0 or 1)


get_esp:
    mov eax, esp     ; Move the value of ESP (Stack Pointer) into EAX
    ret              ; Return, EAX will hold the value to be returned

get_ss:
    mov eax, ss
    ret

get_cr3:
    mov eax, cr3     ; Move the value of CR3 (Page Directory Base Register) into EAX
    ret              ; Return, EAX will hold the value to be returned
