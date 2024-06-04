; File: PalmyraOS\source\core\cpu.asm

BITS 32
global read_tsc

global read_tsc_low
global read_tsc_high
global read_cpuid

; Function to read the low 32 bits of the time stamp counter
read_tsc_low:
    rdtsc
    mov eax, eax
    ret

; Function to read the high 32 bits of the time stamp counter
read_tsc_high:
    rdtsc
    mov eax, edx
    ret
