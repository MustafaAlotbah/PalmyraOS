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


section .text
align 16
memcpy_sse:
    ; Prologue
    push ebp
    mov ebp, esp
    push esi
    push edi

    ; Load arguments
    mov edi, [ebp + 8]      ; Destination address
    mov esi, [ebp + 12]     ; Source address
    mov ecx, [ebp + 16]     ; Number of bytes to copy (num)

    ; Check for small copies
    cmp ecx, 16
    jb small_copy           ; If num < 16, jump to small_copy

    ; Align destination
    mov edx, edi
    and edx, 15             ; edx = edi % 16
    jz check_source_align   ; If destination is aligned, check source alignment

    ; Align destination by copying bytes up to alignment boundary
    mov eax, 16
    sub eax, edx            ; eax = 16 - (edi % 16), bytes to align
    sub ecx, eax            ; ecx -= bytes copied
    cld                     ; Clear direction flag for forward copying
    rep movsb               ; Copy bytes to align destination and advance esi, edi

check_source_align:
    ; Now edi is aligned
    ; Check source alignment
    mov edx, esi
    and edx, 15             ; edx = esi % 16
    jz aligned_copy         ; If source is aligned, proceed to aligned copy

    ; Unaligned source, use unaligned loads
    mov edx, ecx
    shr edx, 4              ; edx = num / 16
    cmp edx, 0
    je remaining_bytes

align 16
unaligned_source_loop:
    movdqu xmm0, [esi]      ; Unaligned load from source
    movdqa [edi], xmm0      ; Aligned store to destination
    add esi, 16
    add edi, 16
    dec edx
    jnz unaligned_source_loop

    ; Update ecx for remaining bytes
    mov ecx, ecx
    and ecx, 15             ; Remaining bytes after 16-byte chunks
    jmp remaining_bytes

align 16
aligned_copy:
    ; Both source and destination are aligned
    mov edx, ecx
    shr edx, 4              ; edx = num / 16
    cmp edx, 0
    je remaining_bytes

align 16
aligned_loop:
    movdqa xmm0, [esi]      ; Aligned load
    movdqa [edi], xmm0      ; Aligned store
    add esi, 16
    add edi, 16
    dec edx
    jnz aligned_loop

    ; Update ecx for remaining bytes
    mov ecx, ecx
    and ecx, 15             ; Remaining bytes after 16-byte chunks

remaining_bytes:
    cmp ecx, 0
    je done                 ; If no remaining bytes, finish

    ; Copy remaining bytes
    rep movsb

done:
    ; Epilogue
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret

align 16
small_copy:
    ; Handle small copies (num < 16)
    cld                     ; Clear direction flag
    rep movsb               ; Copy bytes

    ; Epilogue
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret