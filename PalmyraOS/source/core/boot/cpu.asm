; File: PalmyraOS\source\core\cpu.asm

BITS 32

section .text

global read_tsc_low:function
global read_tsc_high:function

global enable_sse:function
global test_sse:function
global memcpy_sse:function

; Function to read the low 32 bits of the time stamp counter
read_tsc_low:
    rdtsc
    mov eax, eax ; TODO redundant but kept for clarity
    ret

; Function to read the high 32 bits of the time stamp counter
read_tsc_high:
    rdtsc
    mov eax, edx
    ret

; Function to enable SSE and SSE2
enable_sse:
    push eax
    mov eax, cr4
    or eax, 0x600  ; Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    mov cr4, eax
    pop eax
    ret

; Function to test SSE functionality
test_sse:
    mov eax, 0xdeadbeef
    mov ecx, 0x01234567
    movd xmm0, eax
    movd xmm1, ecx
    paddd xmm0, xmm1
    movd eax, xmm0
    ret

memcpy_sse:
    ; Prologue
    push ebp           ; Save the base pointer
    mov ebp, esp       ; Set the base pointer to the current stack pointer
    push esi           ; Save the source index register
    push edi           ; Save the destination index register

    ; Load arguments
    mov edi, [ebp+8]    ; edi=dst
    mov esi, [ebp+12]   ; esi=src
    mov ecx, [ebp+16]   ; ecx=num
                        ; eax = chunks

    ; Check if num is less than 16, use regular copy
    cmp ecx, 16        ; Compare num with 16
    jb .small_copy     ; if (num < 16) goto small_copy

    ; uint32_t chunks = num / 16 bytes (128-bit)
    mov eax, ecx       ; Copy num to eax
    shr ecx, 4         ; chunks <- num / 16

align 16               ; Align the following loop to a 16-byte boundary
.sse_copy:
    movdqu xmm0, [esi]  ; Load 16 bytes from source into xmm0 (unaligned load)
    movdqa [edi], xmm0  ; Store 16 bytes from xmm0 to destination (aligned store)
    add esi, 16         ; src += 16
    add edi, 16         ; dst += 16
    dec ecx             ; chunks--
    jnz .sse_copy       ; if (chunks != 0) goto sse_copy;

    ; Copy remaining bytes
    mov ecx, eax        ; remaining = num
    and ecx, 15         ; remaining %= 16
    jz .done            ; if (remaining == 0) goto done;

.small_copy:
    ; Regular copy for remaining bytes
    ; Repeat MOVSB (move byte from [esi] to [edi]) until ecx is 0
    rep movsb           ;  while (remaining--) *dst++ = *src++

.done:
    ; Epilogue
    pop edi            ; Restore the destination index register
    pop esi            ; Restore the source index register
    mov esp, ebp       ; Restore the stack pointer
    pop ebp            ; Restore the base pointer
    ret                ; Return from the function