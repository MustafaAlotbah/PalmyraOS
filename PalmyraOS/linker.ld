
/* Entry Point (see bootloader.asm) */
ENTRY(kernel_start)

/* Output format 32-bit ELF (Executable and Linkable Format) */
OUTPUT_FORMAT(elf32-i386)

/* Architecture: Intel 80386 (i386) */
OUTPUT_ARCH(i386:i386)

/* Layout of the output binary */
SECTIONS {
    /* Place all sections starting from address 0x01000000 (16MB) */
    /* region below 16MB some reserved areas such as:
        * real-mode interrupt vector table
        * BIOS data area
        * Video memory for VGA graphics
    */
    __mem_start = .;
    . = 0x01000000;
    __mem_end = .;

    /* text section: where execuutable code resides */
    /* text section memory is a multiple of 4 KB  */
    .text ALIGN(4096) : {

        __mem_multiboot_start = .;
        KEEP(*(.multiboot2))  /* Multiboot 2 header (see bootloader.asm) */
        __mem_multiboot_end = .;

        __mem_rodata_start = .;
        KEEP(*(.rodata))      /* Read-only data, constants etc... */
        __mem_rodata_end = .;

        __mem_text_start = .;
        KEEP(*(.text*))       /* All .text files from all sources */
        __mem_text_end = .;
    }

    /* data section: where global and static varaibles resides */
    .data ALIGN(4096) : {

        __mem_cons_start = .;
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

        __mem_cons_end = . ;

        /* Destructors start_dtors, end_dtors not needed */

        __mem_data_start = .;
        /* wildcard: include all .data sections from input files here */
        *(.data)
        __mem_data_end = .;
    }

    /* bss section: where un-initialized data resides (takes space in runtime) */
    .bss ALIGN(4096) : {
        __mem_bss_start = .;
        *(.bss*)
        __mem_bss_end = .;
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