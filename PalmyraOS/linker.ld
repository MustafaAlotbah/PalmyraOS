
/* Entry Point (see bootloader.asm) */
ENTRY(kernel_start)

/* Output format 32-bit ELF (Executable and Linkable Format) */
OUTPUT_FORMAT(elf32-i386)

/* Architecture: Intel 80386 (i386) */
OUTPUT_ARCH(i386:i386)

/* Layout of the output binary */
SECTIONS {
    /* Place all sections starting from address 0x01000000 (1MB) */
    /* region below 1MB some reserved areas such as:
        * real-mode interrupt vector table
        * BIOS data area
        * Video memory for VGA graphics
    */
    . = 0x01000000;

    /* text section: where execuutable code resides */
    /* text section memory is a multiple of 4 KB  */
    .text ALIGN(4096) : {
        KEEP(*(.multiboot))   /* Multiboot header (see bootloader.asm) */
        KEEP(*(.text*))       /* All .text files from all sources */
        KEEP(*(.rodata))      /* Read-only data, constants etc... */
    }

    /* data section: where global and static varaibles resides */
    .data ALIGN(4096) : {

        /* Mark the start of constructors (before entry loader()) */
        /* we will loop through these constructors
        from start_ctors to end_stors (see call_ctors in kernel.cpp) */
        first_constructor = .;

        /* DONT DISCARD: GCC pointers to global constructors in C++ */
        KEEP(*( .init_array ));

        /* DONT DISCARD: C++ allows priorities to constructors,
        sort constructors according to their priority */
        KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.* )))

        /* Mark the end of constructors */
        last_constructor = .;

        /* Destructors start_dtors, end_dtors not needed */

        /* wildcard: include all .data sections from input files here */
        *(.data)
    }

    /* bss section: where un-initialized data resides (takes space in runtime) */
    .bss ALIGN(4096) : {
        *.(.bss*)
    }

    /* Mark the end of globals with a variable end */
    /* Helpful for e.g. setting up the dynamic memory allocation */
    end = .; _end = .; __end = .;

    /* Discard the following sections */
    /DISCARD/ : {
        *(.fini_array*)     /* Distructor for global objects */
        *(.comment)         /* Compiler and linker comments */
    }
}