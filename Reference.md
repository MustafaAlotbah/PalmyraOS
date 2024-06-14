# __Palmyra OS Reference__

## Initial Setup

### Project Structure
```
PalmyraOS/
├── CMakeLists.txt
├── Makefile
├── include/
│   └── boot/
│       └── multiboot.h
├── linker.ld
└── source/
    ├── boot/
    │   └── grub.cfg
    ├── bootloader.asm
    └── kernel.cpp
```

## Roadmap

1. MultiBooting
2. Ports
3. Logger + (TSC/delay)
4. Allocator + Simple KHeap Allocator
5. VBE VESA (Graphics)
6. Panic
7. CPU-ID (extra)
8. GDT
9. PIC Manager (Hardware Interrupts)
10. Interrupts
11. Physical Memory
12. Paging
13. Heap Manager
14. Multitasking
15. User Space
16. SysCalls
17. Elf

- Keyboard driver
- Mouse driver
- Hard-disk driver
- FAT16
- Virtual File System (VFS)
- Userland
- System Calls
- Elf
- stdlib
- shell
- Networking