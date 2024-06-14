BITS 32

GLOBAL set_page_directory:function
GLOBAL enable_paging:function
GLOBAL get_esp:function
GLOBAL get_ss:function

set_page_directory:
    mov dword eax, [esp+4]
    mov dword cr3, eax
    ret

enable_paging:
    push eax
    mov dword eax, cr0
    or eax, 0x80000000  ;set the PG bit
    mov cr0, eax
    pop eax
    ret

get_esp:
    mov eax, esp     ; Move the value of ESP (Stack Pointer) into EAX
    ret              ; Return, EAX will hold the value to be returned

get_ss:
    mov eax, ss
    ret
