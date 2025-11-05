# PalmyraOS
An Educational Operating System based on x86 aiming for simplicity.

## Capabilities

Currently:

- Runs on x86 (intel i386) architecture
- Video Display (VESA)
- Dynamic Memory with Paging
- Multitasking (User / Kernel Spaces)
- Virtual File System

## Screenshots

Currently, it looks as follows:

![PalmyraOS](media/PalmyraOS__05_11_2025_20_26_30.png)

## Tools

- GCC Compiler

```shell
sudo apt install build-essential g++-12 g++-12-multilib
```

Note: GCC 13 prohibits the usage of containers such as map, vector and queue in a freestanding environment.

- NASM (Netwide Assembler)

```shell
sudo apt install nasm
```

- Xorriso 1.5.4

```shell
sudo apt install xorriso
```
- grub-mkrescue

```shell
sudo apt install grub-pc-bin grub-common grub2-common
```
- VirtualBox (optional)
- Qemu (optional)

## Third Party Code

Copyrights included in third party code:

- `core/std/tree.cpp` _For `std::set`_

## Getting started

using Qemu

```shell
qemu-system-i386.exe -cdrom bin/kernel.iso -boot d -m 1024 -S -gdb tcp::1234 -drive file="path/to/hard_disk.vdi",format=vdi,media=disk
```

or

```shell
qemu-system-i386.exe -cdrom bin/kernel.iso -boot d -m 1024 -S -gdb tcp::1234 -hda "path/to/hard_disk.vhd" -serial file:serial_output.txt
```

## Convert your hard disk

#### from VDI to Raw

```shell
VBoxManage.exe convertfromraw harddrive.img harddrive2.vdi --format VDI
```