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

1. Booting
2. Simple KHeap
3. VBE VESA (Graphics)
4. Panic
5. CPU-ID (extra)
6. GDT
7. Ports
8. PIC Manager (Hardware Interrupts)
9. Interrupts
10. Physical Memory
11. Paging
12. Multiprocessing
