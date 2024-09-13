# Set parameters for the compiler and assembler
# 32-Bit output
GPP_PARAMS = -g -O0 -m32 -ffreestanding -fno-use-cxa-atexit -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore -fdata-sections -ffunction-sections
AS_PARAMS = -g -f elf
LD_PARAMS = -melf_i386

# Define subdirectories containing source files
SUBDIRS := \
	source/ \
	source/libs/ \
	source/libs/shared/memory \
	source/tests \
	source/core/ \
	source/core/boot/ \
	source/core/peripherals/ \
	source/core/memory \
	source/core/std \
	source/core/tasks \
	source/core/files \
	source/core/files/partitions \
	source/palmyraOS \
	source/userland/systemWidgets \
	source/userland/tests

# Find all .cpp and .asm source files in the specified subdirectories
SRC_CPP_FILES := $(wildcard $(addsuffix /*.cpp, $(SUBDIRS)))
SRC_ASM_FILES := $(wildcard $(addsuffix /*.asm, $(SUBDIRS)))

# Remove preceding 'source/' and replace .cpp/.asm with .o
OBJ_CPP_FILES := $(patsubst source/%,%, $(SRC_CPP_FILES:.cpp=.o))
OBJ_ASM_FILES := $(patsubst source/%,%, $(SRC_ASM_FILES:.asm=.o))
objects := $(OBJ_CPP_FILES) $(OBJ_ASM_FILES)

# Print the list of object files
objects:
	echo $(objects)

# Compile .cpp files into .o files
bin/%.o: source/%.cpp
	@mkdir -p $(@D)
	g++ $(GPP_PARAMS) -Iinclude -o $@ -c $<

# Compile .asm files into .o files
bin/%.o: source/%.asm
	@mkdir -p $(@D)
	nasm $(AS_PARAMS) -o $@ $<

# Link all object files using the linker script to create the kernel binary
kernel.bin: linker.ld $(addprefix bin/, $(objects))
	ld $(LD_PARAMS) -T $< -o bin/$@ $(addprefix bin/, $(objects))

# run 'install' task depending on my kernel.bin using the following command
build: kernel.bin

iso: kernel.bin
	@echo "Creating ISO image"
	@mkdir -p bin/iso
	@mkdir -p bin/iso/boot
	@mkdir -p bin/iso/boot/grub
	@cp bin/kernel.bin bin/iso/boot
	@cp source/core/boot/grub.cfg bin/iso/boot/grub/grub.cfg
	@grub-mkrescue --output bin/kernel.iso bin/iso
	@echo "ISO image created"

clean:
	@echo "Cleaning generated subdirectories and object files"
	@rm bin/*.o
	@rm -rf bin/iso
	@for dir in $(SUBDIRS); do \
		rm -rf bin/$$(basename $$dir); \
	done
	@echo "Clean completed"

.PHONY: objects build iso