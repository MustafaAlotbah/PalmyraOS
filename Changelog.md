# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 01.06.2024

### `.gitignore`
- Removed the following patterns:
    - `*.d`, `*.slo`, `*.lo`, `*.obj`, `*.gch`, `*.pch`, `*.so`, `*.dylib`, `*.dll`, `*.mod`, `*.smod`, `*.lai`, `*.la`, `*.a`, `*.lib`, `*.exe`, `*.out`, `*.app`
- Added the following patterns:
    - `*.pdf`, `*.json`, `*.bin`, `*.iso`, `bin/`, `.vscode/`, `.idea/`, `cmake-build-debug/`

### `Changelog.md`
- Initial creation of the changelog file.

### `PalmyraOS/CMakeLists.txt`
- Initial creation of the CMake configuration file, currently only for CLion compatibility.

### `PalmyraOS/Makefile`
- Initial creation of the Makefile for building Palmyra OS with targets for compiling source files and creating ISO images.

### `PalmyraOS/include/boot/multiboot.h`
- Updated `multiboot.h`:
    - Added detailed definitions for VBE mode and control info structures.
    - Defined `multiboot_info_t` structure with detailed fields for BIOS-provided information.

### `PalmyraOS/linker.ld`
- Initial creation of the linker script defining the memory layout for Palmyra OS, including sections for text, data, and bss.

### `PalmyraOS/source/kernel.cpp`
- Updated `kernel.cpp`:
    - Added multiboot information handling and a basic screen fill function for testing VBE mode.
    - Defined the kernel entry point and constructor handling.

### `scripts/changes.py`
- Initial creation of the script to fetch and print git changes, with output copied to the clipboard.

