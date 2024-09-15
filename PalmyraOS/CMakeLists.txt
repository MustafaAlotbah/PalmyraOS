cmake_minimum_required(VERSION 3.10)
project(PalmyraOS)

#enable_language(ASM_NASM)
#if(NOT CMAKE_ASM_NASM_COMPILER_LOADED)
#    message("!-- NASM compiler not found")
#endif()

# Set the compilation flags for C++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -fno-use-cxa-atexit -fno-exceptions -fno-leading-underscore")

# Include directories
include_directories(include)

# Recursively find all .cpp and .asm files in the src/ directory
file(GLOB_RECURSE SOURCES "source/*.cpp" "source/*.asm")

# Define the executable output and its source files
add_executable(kernel.bin ${SOURCES})

# Set properties for assembly files
set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-g")

# Linker and linker script settings
set_target_properties(kernel.bin PROPERTIES LINK_FLAGS "-melf_i386 -T ${CMAKE_SOURCE_DIR}/src/linker.ld")

# Add a custom target for making an ISO image
add_custom_target(iso
        COMMAND mkdir -p bin/iso
        COMMAND mkdir -p bin/iso/boot
        COMMAND mkdir -p bin/iso/boot/grub
        COMMAND cp $<TARGET_FILE:kernel.bin> bin/iso/boot
        COMMAND echo 'set timeout=0' > bin/iso/boot/grub/grub.cfg
        COMMAND echo 'set default=0' >> bin/iso/boot/grub/grub.cfg
        COMMAND echo 'menuentry "PalmyraOS" {' >> bin/iso/boot/grub/grub.cfg
        COMMAND echo '  multiboot /boot/kernel.bin' >> bin/iso/boot/grub/grub.cfg
        COMMAND echo '  boot' >> bin/iso/boot/grub/grub.cfg
        COMMAND echo '}' >> bin/iso/boot/grub/grub.cfg
        COMMAND grub-mkrescue --output bin/kernel.iso bin/iso
        COMMAND rm -rf bin/iso
        COMMENT "Creating ISO image"
)